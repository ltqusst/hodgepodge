// tipc.cpp:
//
//==================================================================
// Linux/Windows cross-platform IPC based on linux-epoll behavior
// 
// On Windows:
//      I/O Completion Port based NamedPipe
// On Linux:
//      epoll & nonblocking read/write based Unix Domain Sockets
//==================================================================
//#include "stdafx.h"

#ifdef WIN32
#include "windows.h"
#endif

#ifdef linux
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//net
#include <net/if_arp.h>
#include <net/if.h>
//sys
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/epoll.h> /* epoll function */
#include <sys/un.h>
#endif


//C++
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <cassert>
#include <cstddef>
#include <string>
#include <functional>
#include <stdexcept>
#include <map>
#include <tuple>
#include <deque>
#include <atomic>
#include <iostream>
#include <thread>
#include <cassert>
#include <system_error>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <vector>
#include <iomanip>
#include <sstream>
#include <iterator>

#ifdef WIN32
typedef HANDLE OS_HANDLE;
#define INVALID_OS_HANDLE INVALID_HANDLE_VALUE
#define CLOSE_OS_HANDLE(a) CloseHandle(a)
#else
typedef int OS_HANDLE;
#define INVALID_OS_HANDLE -1
#define CLOSE_OS_HANDLE(a) ::close(a)
#endif

// a safe replacement for strcpy
// let compiler deduce destsz whenever possible
template<size_t destsz>
int string_copy(char (&dest)[destsz], const std::string & str, size_t pos = 0)
{
#ifdef WIN32
	strcpy_s(dest, destsz, str.c_str() + pos);
	return std::min<>(destsz-1, str.length() - pos);
#else
	size_t cnt = str.copy(dest, destsz - 1, pos);
	cnt = std::min<size_t>(cnt, destsz - 1);
	dest[cnt] = '\0';
	return cnt;
#endif
}

class ipc_connection;
class ipc_io_service;


//
// service:   run in its own thread (so all callbacks also called from there, make sure do not blocking inside call back)
//
// connection:  with the help of service coupled together
//			  this helper fulfills common async IO(include passive connection) requests
//
// one service can co-work with many different kind of connections,
// for example, socket and namedPipe can be implemented in 2 different executor
//

class ipc_io_service
{
public:
	ipc_io_service();
	~ipc_io_service();

	void run();
	void associate(ipc_connection * pconn);
	void unassociate(ipc_connection * pconn);
	OS_HANDLE native_handle(void);

private:
	static const int        m_epollTimeout = 100;
	std::atomic<bool>		m_exit;

	//a general mapping facility
	static const int							m_map_fast_size = 1024;
	ipc_connection *							m_map_fast[m_map_fast_size];
	std::map<OS_HANDLE, ipc_connection*>		m_map;

	ipc_connection* & get(OS_HANDLE oshd)
	{
		unsigned long v = (unsigned long)oshd;
		if (v >= 0 && v < m_map_fast_size)
			return m_map_fast[v];

		if (m_map.find(oshd) == m_map.end())
			m_map.insert(std::make_pair(oshd, (ipc_connection*)NULL));

		return m_map[oshd];
	}
	void erase(OS_HANDLE oshd)
	{
		unsigned long v = (unsigned long)oshd;
		if (v >= 0 && v < m_map_fast_size) {
			m_map_fast[v] = NULL;
			return;
		}
		auto it = m_map.find(oshd);
		if (it != m_map.end()) 
			m_map.erase(it);
	}

#ifdef WIN32
	OS_HANDLE						m_h_io_compl_port;
#else
	//Linux
	static const int                m_maxEpollEvents = 100;
	static const int 				m_epollSize = 1000;
	int								m_epollFd;
#endif
};


class ipc_connection
{
public:
	static ipc_connection * create(ipc_io_service & s, const char* ipc_type, const char* server_name = "");

	virtual ~ipc_connection() {}
	virtual OS_HANDLE native_handle() = 0;
	virtual ipc_io_service & get_service() { return m_service; }

	//on Windows, notify means one IO request is completed or error occured
	//on Linux, notify means one IO request type is ready to issue without blocking
	virtual void notify(int error_code, int transferred_cnt, unsigned long hint) = 0;

	//blocking/sync version
	//  based on async version, so they cannot be called within async handler!
	//  or deadlock may occur. actually no blocking operation should be called inside
	//  async handler at all ( or async mode will have performance issue ).
	//
	//  so inside async handler, user can:
	//		1. response by using async_write
	//      2. dispatch the response task to another thread, and in that thread, 
	//         sync version IO can be used without blocking io_service.
	virtual int read(void * pbuff, const int len, int *lp_cnt = NULL) = 0;
	virtual int write(void * pbuff, const int len) = 0;

	//client(blocking is acceptable because usually its short latency)
	virtual int connect(const std::string & serverName) = 0;
	virtual int listen(void) = 0;
	virtual void close(void) = 0; //close connection

								  //async callbacks are free for register by simple assign
	std::function<void(ipc_connection * pconn, const std::error_code & ec, std::size_t len)>	on_read;
	std::function<void(ipc_connection * pconn)>													on_accept;
	std::function<void(ipc_connection * pconn)>													on_close;

protected:
	ipc_connection(ipc_io_service & s) :m_service(s) {}
	ipc_io_service &    	m_service;
};



void ipc_io_service::associate(ipc_connection * pconn)
{
	assert(pconn != NULL);

	OS_HANDLE oshd = pconn->native_handle();
	if (oshd == INVALID_OS_HANDLE) return;

	ipc_connection* p_conn = get(oshd);

	if (p_conn == NULL) {
#ifdef WIN32
		//first time association: register into IO completion port system.
		//we use the handle as completion key directly to be more compatible with linux/epoll
		//though this is not the best solution
		if (NULL == CreateIoCompletionPort(oshd, m_h_io_compl_port, (ULONG_PTR)oshd, 0)) {
			std::error_code ec(GetLastError(), std::system_category());
			throw std::runtime_error(std::string("associate() CreateIoCompletionPort failed with ") + ec.message());
		}
#else
		//linux, just add the fd into epoll
		struct epoll_event event;

		// we choose level-trigger mode, so blocking socket is enough.
		//
		// if we use edge-trigger mode, then we need to drain all available data in cache
		// using non-blocking socket on each epoll-event, and this can bring some difficulty
		// to application parser implementation
		//
		event.events = EPOLLIN | EPOLLRDHUP;
		event.data.fd = pconn->native_handle();
		epoll_ctl(m_epollFd, EPOLL_CTL_ADD, pconn->native_handle(), &event);
#endif
	}
	else {
		//do we need remove previous fd?
	}

	//internal association is mutable (although CreateIoCompletionPort can be down only once)
	get(oshd) = pconn;
}

void ipc_io_service::unassociate(ipc_connection * pconn)
{
	assert(pconn != NULL);
	OS_HANDLE oshd = pconn->native_handle();
	if (oshd == INVALID_OS_HANDLE) return;

#ifdef WIN32
	//no way to un-associate unless we close the file handle
#else
	epoll_ctl(m_epollFd, EPOLL_CTL_DEL, pconn->native_handle(), NULL);
#endif

	//remove from cache
	erase(oshd);
}




#ifdef WIN32

// this class is an extention to fd/handle
// on Windows, scheduler can locate it through CompletionKey.
// on Linux, this needs derived from fd.
//           https://stackoverflow.com/questions/8175746/is-there-any-way-to-associate-a-file-descriptor-with-user-defined-data
class ipc_connection_win_namedpipe : public ipc_connection
{
public:
	ipc_connection_win_namedpipe(ipc_io_service & service, const std::string & servername);
	~ipc_connection_win_namedpipe();
	virtual void notify(int error_code, int transferred_cnt, unsigned long hint);
	
	virtual OS_HANDLE native_handle() {return m_oshd;}

	//blocking/sync version(based on async version)
	virtual int read(void * pbuff, const int len, int *lp_cnt);
	virtual int write(void * pbuff, const int len);

	//client(blocking is acceptable because usually its short latency)
	virtual int connect(const std::string & dest);
	virtual int listen(void);
	virtual void close(void);

protected:

private:
	ipc_connection_win_namedpipe(ipc_io_service & service, HANDLE handle);

	int wait_for_connection(void);
	void trigger_async_cache_read(void);

	HANDLE					m_oshd;

	// <0 means no cached byte0
	// >0 means 1 byte is cached and should be filled to user's buffer first
	unsigned char			m_cache_byte0;
	bool					m_cache_empty;
	OVERLAPPED				m_cache_overlapped;
	OVERLAPPED				m_error_overlapped;
	OVERLAPPED				m_waitconn_overlapped;
	OVERLAPPED				m_sync_overlapped;

	std::string				m_name;		//IPC name
};

ipc_connection_win_namedpipe::ipc_connection_win_namedpipe(ipc_io_service &service, const std::string & servername):
	ipc_connection(service), m_cache_byte0(0), m_cache_empty(true), m_cache_overlapped{}, m_error_overlapped{}, m_sync_overlapped{}, m_name(servername)
{
	m_sync_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}

ipc_connection_win_namedpipe::ipc_connection_win_namedpipe(ipc_io_service & service, HANDLE handle):
	ipc_connection(service), m_cache_byte0(0), m_cache_empty(true), m_cache_overlapped{}, m_error_overlapped{}, m_sync_overlapped{}, m_oshd(handle), m_name("")
{
	m_sync_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_service.associate(this);	//change association as well
}
ipc_connection_win_namedpipe::~ipc_connection_win_namedpipe()
{
	CloseHandle(m_sync_overlapped.hEvent);
	close();
}

void ipc_connection_win_namedpipe::notify(int error_code, int transferred_cnt, unsigned long hint)
{
	OVERLAPPED * poverlapped = (OVERLAPPED *)hint;

	const char * pevt = "";
	if (poverlapped == &m_cache_overlapped) pevt = "cache";
	if (poverlapped == &m_error_overlapped) pevt = "error";
	if (poverlapped == &m_waitconn_overlapped) pevt = "accept";

	if (poverlapped == &m_sync_overlapped) return;

	if (poverlapped == &m_cache_overlapped ||
		poverlapped == &m_error_overlapped) {

		std::error_code ec2;
		if (poverlapped == &m_error_overlapped)
			ec2 = std::error_code(m_error_overlapped.Offset, std::system_category());
		else {
			ec2 = std::error_code(error_code, std::system_category()); ;
		}

		if (transferred_cnt == 1) {
			//fprintf(stderr, ">>>>>>>>>> m_cache_byte0=%c from [%s]  \n", m_cache_byte0, pevt);
			m_cache_empty = false;
		}

		assert(transferred_cnt == 1 || transferred_cnt == 0);

		//cache byte received, 
		if (on_close && ec2.value() == ERROR_BROKEN_PIPE)
			on_close(this);
		else
		{
			bool b_new_data_arrived = !m_cache_empty;

			if (on_read)
				on_read(this, ec2, 0);

			//user must atleast read one byte inside the callback
			if (b_new_data_arrived && !m_cache_empty) {
				//no one clear the cache!
				assert(0);
			}
			else
				trigger_async_cache_read();
		}
	}
	else if (poverlapped == &m_waitconn_overlapped) {
		//accept
		if (on_accept) {
			//on windows, the namedpipe handle that accepted the connection
			//will serve the connection directly.
			//
			//In socket, the listening socket is keep listenning, a new socket 
			//will be returned for client connection purpose.
			//
			//to make them consistent, we follow the socket way, return client connection
			//

			//transffer our handle to new wrapper(this is possible, thanks to the mapping that io_service supported)
			ipc_connection_win_namedpipe * pconn = new ipc_connection_win_namedpipe(get_service(), native_handle());
		
			listen();//create a new instance of the pipe

			//user will setup callbacks inside on_accept()
			on_accept(pconn);

			wait_for_connection();

			//trigger target connection read
			pconn->trigger_async_cache_read();
		}
	}
	else
		return;
}

int ipc_connection_win_namedpipe::wait_for_connection(void)
{
	assert(native_handle() != INVALID_OS_HANDLE || (fprintf(stderr, "%s:%d %s", __FILE__, __LINE__, "  m_oshd == NULL!"), 0));

	BOOL bRet = ConnectNamedPipe(native_handle(), &m_waitconn_overlapped);
	if (bRet) {
		//connection success, no IO pending, make sure on_accept is called within right thread
		//according to behaviour of ReadFile, completion IO event should be trigered automatically

		//PostQueuedCompletionStatus(get_service().native_handle(), 1, (ULONG_PTR)native_handle(), &m_waitconn_overlapped);
	}
	else {
		DWORD dwErr = GetLastError();
		if (dwErr == ERROR_PIPE_CONNECTED)
			PostQueuedCompletionStatus(get_service().native_handle(), 1, (ULONG_PTR)native_handle(), &m_waitconn_overlapped);

		else if (dwErr != ERROR_IO_PENDING) {
			//only report error async when it happens during async
			//so here we just
			return dwErr;
			//m_error_overlapped.Offset = dwErr;
			//PostQueuedCompletionStatus(get_service().native_handle(), 1, (ULONG_PTR)native_handle(), &m_error_overlapped);
		}
	}
	return 0;
}

//blocking/sync version(based on async version)
int ipc_connection_win_namedpipe::read(void * pvbuff, const int len, int *lp_cnt)
{
	assert(native_handle() != INVALID_OS_HANDLE || (fprintf(stderr, "%s:%d %s", __FILE__, __LINE__, "  m_oshd == NULL!"), 0));
	//m_ior_in.setup((char*)pbuff, len, IOTYPE_IN, true);
	unsigned char * pbuff = (unsigned char *)pvbuff;
	int size = len;
	int cnt = 0;
	int error_code = 0;
	int bytes_avail = -1;
	
	//fprintf(stderr, ">");

	//clear the cache if there is data
	if (!m_cache_empty) {
		pbuff[cnt] = m_cache_byte0;
		cnt++;
		m_cache_empty = true;
	}

	if (lp_cnt) {
		DWORD TotalBytesAvail = 0;
		if (PeekNamedPipe(m_oshd, NULL, 0, NULL, &TotalBytesAvail, NULL)) {
			bytes_avail = TotalBytesAvail;
		}
		*lp_cnt = cnt;

		//some mode, return as fast as we can
		if (bytes_avail == 0) return 0;
	}

	while (cnt < size && error_code == 0) {
		DWORD NumberOfBytesTransferred = 0;
		DWORD io_cnt = size - cnt;

		//if we are in async mode and we know bytes in pipe, then we just read so much data that
		//no blocking/async pending will be triggered
		if (bytes_avail > 0)
			io_cnt = std::min<DWORD>(bytes_avail, io_cnt);

		BOOL bRet = ReadFile(native_handle(), pbuff + cnt, io_cnt, &NumberOfBytesTransferred, &m_sync_overlapped);
		//bRet = WriteFile(m_oshd, m_buff + m_cnt, io_cnt, &NumberOfBytesTransferred, &m_sync_overlapped);
		if (bRet) {
			cnt += NumberOfBytesTransferred;
		}
		else {
			DWORD dwErr = GetLastError();
			if (dwErr == ERROR_IO_PENDING) {
				NumberOfBytesTransferred = 0;
				if (GetOverlappedResult(native_handle(), &m_sync_overlapped, &NumberOfBytesTransferred, TRUE)) {
					cnt += NumberOfBytesTransferred;
					error_code = 0;
				}
				else {
					//cnt += NumberOfBytesTransferred;	//do we need it? little confusing
					error_code = GetLastError();
				}
				ResetEvent(m_sync_overlapped.hEvent);
			}
			else
				error_code = dwErr;
		}

		if (lp_cnt != NULL && cnt > 0) {
			*lp_cnt = cnt;
			break;
		}
	};
	//fprintf(stderr, "<");

	if (error_code) {
		std::error_code ec(error_code, std::system_category());
		fprintf(stderr, "connect read() returns %d:%s\n", error_code, ec.message().c_str());
	}

	return error_code;
}
int ipc_connection_win_namedpipe::write(void * pvbuff, const int len)
{
	assert(native_handle() != INVALID_OS_HANDLE || (fprintf(stderr, "%s:%d %s", __FILE__, __LINE__, "  m_oshd == NULL!"), 0));

	unsigned char * pbuff = (unsigned char *)pvbuff;
	int size = len;
	int cnt = 0;
	int error_code = 0;

	while (cnt < size && error_code == 0) {
		DWORD NumberOfBytesTransferred = 0;
		BOOL bRet = WriteFile(native_handle(), pbuff + cnt, size - cnt, &NumberOfBytesTransferred, &m_sync_overlapped);
		if (bRet) {
			cnt += NumberOfBytesTransferred;
		}
		else {
			DWORD dwErr = GetLastError();
			if (dwErr == ERROR_IO_PENDING) {
				NumberOfBytesTransferred = 0;
				if (GetOverlappedResult(native_handle(), &m_sync_overlapped, &NumberOfBytesTransferred, TRUE)) {
					cnt += NumberOfBytesTransferred;
					error_code = 0;
				}
				else {
					//cnt += NumberOfBytesTransferred;	//do we need it? little confusing
					error_code = GetLastError();
				}
				ResetEvent(m_sync_overlapped.hEvent);
			}
			else
				error_code = dwErr;
		}
	};
	return error_code;
}
void ipc_connection_win_namedpipe::trigger_async_cache_read(void)
{
	assert(m_cache_empty == true);

	DWORD NumberOfBytesTransferred = 0;
	m_cache_overlapped = {};	//do it async 
	m_cache_empty = true;
	BOOL bRet = ReadFile(native_handle(), &m_cache_byte0, 1, NULL, &m_cache_overlapped);
	if (bRet) {
		//for FILE_FLAG_OVERLAPPED, completion IO event will trigger even when the READ opeartion is completed on the spot. 
		//PostQueuedCompletionStatus(get_service().native_handle(), 1, (ULONG_PTR)native_handle(), &m_cache_overlapped);
	}
	else {
		DWORD dwErr = GetLastError();
		if (dwErr != ERROR_IO_PENDING) {
			//error occurs, make sure on_read() callback running inside io_service() thread, like epoll does
			//std::error_code ec(dwErr, std::system_category());
			//fprintf(stderr, "trigger_async_cache_read() returns %d:%s\n", dwErr, ec.message().c_str());
			m_error_overlapped.Offset = dwErr;
			PostQueuedCompletionStatus(get_service().native_handle(), 0, (ULONG_PTR)native_handle(), &m_error_overlapped);
		}
	}
}

int ipc_connection_win_namedpipe::connect(const std::string & dest)
{
	HANDLE oshd = CreateFile(dest.c_str(),   // pipe name 
		GENERIC_READ |  // read and write access 
		GENERIC_WRITE,
		0,              // no sharing 
		NULL,           // default security attributes
		OPEN_EXISTING,  // opens existing pipe 
		FILE_FLAG_OVERLAPPED,              // default attributes 
		NULL);          // no template file 
	if (oshd == INVALID_HANDLE_VALUE) {
		std::error_code ec(GetLastError(), std::system_category());
		throw std::runtime_error(ec.message());
		//throw std::runtime_error(std::string("CreateFile failed with ") + last_error(ERROR_BROKEN_PIPE));
		//throw std::runtime_error(std::string("CreateFile failed with ") + last_error(GetLastError()));
		return 1;
	}
	m_oshd = oshd;
	m_service.associate(this);
	trigger_async_cache_read();
	return 0;
}

int ipc_connection_win_namedpipe::listen(void)
{
	//server
#define PIPE_TIMEOUT 5000
#define BUFSIZE 4096
	HANDLE oshd = CreateNamedPipe(m_name.c_str(),            // pipe name 
		PIPE_ACCESS_DUPLEX |     // read/write access 
		FILE_FLAG_OVERLAPPED,    // overlapped mode 
		PIPE_TYPE_BYTE |      // byte-type pipe 
		PIPE_READMODE_BYTE |  // message-read mode 
		PIPE_WAIT,               // blocking mode 
		PIPE_UNLIMITED_INSTANCES,               // number of instances 
		BUFSIZE * sizeof(TCHAR),   // output buffer size 
		BUFSIZE * sizeof(TCHAR),   // input buffer size 
		PIPE_TIMEOUT,            // client time-out 
		NULL);                   // default security attributes 
	if (oshd == INVALID_HANDLE_VALUE) {
		std::error_code ec(GetLastError(), std::system_category());
		throw std::runtime_error(std::string("CreateNamedPipe failed with ") + ec.message());
	}
	// only for ipc server 
	// which do not do IO at all, only async_accept() will be called
	m_oshd = oshd;
	m_service.associate(this);
	wait_for_connection();

	return 0;
}

void ipc_connection_win_namedpipe::close(void)
{
	if (m_oshd != INVALID_OS_HANDLE) {
		//close it
		m_service.unassociate(this);
		CloseHandle(m_oshd);
		m_oshd = INVALID_OS_HANDLE;
	}
}

//==================================================================================================================
ipc_io_service::ipc_io_service() :
	m_map_fast{}
{
	m_h_io_compl_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (m_h_io_compl_port == NULL) {
		std::error_code ec(GetLastError(), std::system_category());
		throw std::runtime_error(std::string("ipc_io_service ctor: CreateIoCompletionPort failed with ") + ec.message());
	}
}
ipc_io_service::~ipc_io_service()
{
	m_exit.store(true);
	CloseHandle(m_h_io_compl_port);
}
OS_HANDLE ipc_io_service::native_handle(void)
{
	return m_h_io_compl_port;
}
void ipc_io_service::run()
{
	m_exit.store(false);
	//this thread will exit when m_exit is set
	// or the CompletionPort is closed
	while (!m_exit.load())
	{
		// the I/O completion port will post event on each low-level packet arrival
		// which means the actuall NumberOfBytes still may less than required.
		//
		// but most time it is of the same size as sender's write buffer length
		//
		DWORD NumberOfBytes;
		ULONG_PTR CompletionKey;
		LPOVERLAPPED  lpOverlapped;
		BOOL bSuccess = GetQueuedCompletionStatus(m_h_io_compl_port,
			&NumberOfBytes,
			&CompletionKey,
			&lpOverlapped,
			m_epollTimeout);

		//Only GetLastError() on failure
		DWORD dwErr = bSuccess ? 0 : GetLastError();

		if (!bSuccess && ERROR_ABANDONED_WAIT_0 == dwErr)
			break;

		//Success
		if (lpOverlapped == NULL) {
			continue;
		}

		if (0) {
			std::error_code ec(dwErr, std::system_category());
			printf(">>>> %s GetQueuedCompletionStatus() returns bSuccess=%d, NumberOfBytes=%d, lpOverlapped=%p GetLastError=%d %s\n", __FUNCTION__,
				bSuccess, NumberOfBytes, lpOverlapped, dwErr, ec.message().c_str());
		}

		if (lpOverlapped->hEvent) {
			//a sync operation, skip callback
			continue;
		}

		//lpOverlapped  is not null
		// CompletionKey is the file handle
		// we don't need a map because this Key is the Callback
		OS_HANDLE oshd = static_cast<OS_HANDLE>((void*)CompletionKey);

		ipc_connection * pio = get(oshd);

		assert(pio != NULL);

		if (pio)
			pio->notify(dwErr, NumberOfBytes, (unsigned long)lpOverlapped);
		else
			fprintf(stderr, "Error: ipc_io_service::run() got file handle un-associated!\n");
	}
}
//==================================================================================================================

#else

//Unix Domain Sockets
class ipc_connection_linux_UDS : public ipc_connection
{
public:
	ipc_connection_linux_UDS(ipc_io_service & service, const std::string & serverName);
	~ipc_connection_linux_UDS();

	virtual void notify(int error_code, int transferred_cnt, unsigned long hint);

	virtual OS_HANDLE native_handle() { return m_fd; }

	//blocking/sync version(based on async version)
	virtual int read(void * pbuff, const int len, int *lp_cnt);
	virtual int write(void * pbuff, const int len);

	//client(blocking is acceptable because usually its short latency)
	virtual int connect(const std::string & serverName);
	virtual int listen(void);
	virtual void close(void);
protected:

private:
	ipc_connection_linux_UDS(ipc_io_service & service, int fd);
	bool set_block_mode(bool makeBlocking = true);

	constexpr static const char * CLI_PATH = "/var/tmp/";
	constexpr static const int  m_numListen = 5;

	int 					m_fd;
	const char *			m_name;		//IPC name
	bool 					m_listening;
};

ipc_connection_linux_UDS::ipc_connection_linux_UDS(ipc_io_service &service, const std::string & serverName) :
	ipc_connection(service), m_listening(false)
{
	struct sockaddr_un addr;

	/* create a UNIX domain stream socket */
	if ((m_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		std::error_code ec(errno, std::system_category());
		throw std::runtime_error(std::string("ipc_connection_linux_UDS ctor: socket() failed. ") + ec.message());
		return;
	}

	// socket name is not changable, only one bind() can be called
	// so we treat it as a part of resource acquire process (RAll)

	//memset(&addr, 0, sizeof(addr));

	// since its a aggregate C struct, initialize zero can be done this way
	// make sure it do not have irresponsible default constructor or this may fail
	// but since its a C struct, we can safely value-initialize it
	// (https://akrzemi1.wordpress.com/2013/09/10/value-initialization-with-c/)

	addr = {};
	addr.sun_family = AF_UNIX;

	std::string sun_path = serverName;
	if (sun_path.empty()) {
		//client socket do not have server name, build one for it
		std::ostringstream stringStream;
		stringStream << CLI_PATH;
		stringStream << std::setw(5) << std::setfill('0') << getpid();
		sun_path = stringStream.str();
	}

	::unlink(sun_path.c_str());

	int cnt = string_copy(addr.sun_path, sun_path);
	int len = offsetof(sockaddr_un, sun_path) + cnt + 1;

	if (::bind(m_fd, (struct sockaddr*)&addr, len) < 0) {
		std::error_code ec(errno, std::system_category());
		throw std::runtime_error(std::string("ipc_connection_linux_UDS ctor: bind() failed. ") + ec.message());
		return;
	}

	service.associate(this);
}
ipc_connection_linux_UDS::ipc_connection_linux_UDS(ipc_io_service & service, int fd) :
	ipc_connection(service), m_listening(false), m_fd(fd)
{
	service.associate(this);
}
ipc_connection_linux_UDS::~ipc_connection_linux_UDS()
{
	m_service.unassociate(this);
	close();
}

void ipc_connection_linux_UDS::notify(int error_code, int transferred_cnt, unsigned long hint)
{
	//printf(">>>>>>>>>>>>>>> notify : %s,%s\n",hint&EPOLLIN?"EPOLLIN":"",hint&EPOLLRDHUP?"EPOLLRDHUP":"");

	//if we are listening socket, 		do on_accept on EPOLLIN
	//
	//	to be consistent with common behavior, we just
	//  accept the connection and return connected connection to user.
	//
	//if we are communication socket, 	do on_read on EPOLLIN
	//
	if (m_listening && (hint & EPOLLIN)) {
		int clifd, err, rval;
		struct stat statbuf;
		struct sockaddr_un addr;

		socklen_t len = sizeof(addr);
		if ((clifd = ::accept(m_fd, (struct sockaddr *)&addr, &len)) < 0) {
			//HError("accept error: %s\n", strerror(errno));
			return;     /* often errno=EINTR, if signal caught */
		}

		if (on_accept) {
			/* obtain the client's uid from its calling address */
			len -= offsetof(struct sockaddr_un, sun_path); /* len of pathname */
			addr.sun_path[len] = 0;           /* null terminate */

			if (stat(addr.sun_path, &statbuf) < 0) {
				//HError("stat error: %s\n", strerror(errno));
				::close(clifd);
				return;
			}

			if (S_ISSOCK(statbuf.st_mode) == 0) {
				//HError("S_ISSOCK error: %s\n", strerror(errno));
				//rval = -3;      /* not a socket */
				::close(clifd);
				return;
			}
			__uid_t uid = statbuf.st_uid;   /* return uid of caller */
			unlink(addr.sun_path);        /* we're done with pathname now */

			ipc_connection_linux_UDS * pconn = new ipc_connection_linux_UDS(get_service(), clifd);

			on_accept(static_cast<ipc_connection *>(pconn));
		}

	}
	else if (hint & EPOLLRDHUP) {
		if (on_close)
			on_close(this);
	}
	else if (hint & EPOLLIN) {
		//new data arrived
		std::error_code ec(error_code, std::system_category());
		if (on_read)
			on_read(this, ec, transferred_cnt);
	}
}

int ipc_connection_linux_UDS::connect(const std::string & serverName)
{
	int len, err, rval;
	struct sockaddr_un addr = { 0 };

	/* fill socket address structure with server's address */
	addr.sun_family = AF_UNIX;

	int cnt = string_copy(addr.sun_path, serverName);

	len = offsetof(struct sockaddr_un, sun_path) + cnt + 1;

	if (::connect(m_fd, (struct sockaddr *)&addr, len) < 0) {
		//HError("connect error: %s\n", strerror(errno));
		return errno;
	}

	return 0;
}
int ipc_connection_linux_UDS::listen(void)
{
	if (!m_listening) {
		if (::listen(m_fd, m_numListen) < 0) {
			return errno;
		}
		m_listening = true;
	}
	return 0;
}

int ipc_connection_linux_UDS::read(void * buffer, const int bufferSize, int * lp_cnt)
{
	int err = 0;

	if (m_fd <= 0 || !buffer || bufferSize <= 0) {
		return -1;
	}

	char *ptr = static_cast<char*>(buffer);

	set_block_mode(lp_cnt == NULL);

	int leftBytes = bufferSize;
	while (leftBytes > 0) {
		int readBytes = ::read(m_fd, ptr, leftBytes);
		if (readBytes == 0) {
			/* reach EOF */
			break;
		}
		else if (readBytes < 0) {
			if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
				err = errno;
				fprintf(stderr, "read failed with %d: %s\n", errno, strerror(errno));
				break;
			}
			readBytes = 0;
		}

		ptr += readBytes;
		leftBytes -= readBytes;

		if (lp_cnt)
			break;
	}

	if (lp_cnt)
		*lp_cnt = (bufferSize - leftBytes);

	//fprintf(stderr,"read %d bytes, with errno = %d: %s\n", bufferSize - leftBytes, err, strerror(err));
	return err;
}

int ipc_connection_linux_UDS::write(void * buffer, const int bufferSize)
{
	int err = 0;

	if (m_fd <= 0 || !buffer || bufferSize <= 0) {
		return -1;
	}

	char *ptr = static_cast<char*>(buffer);

	int leftBytes = bufferSize;
	while (leftBytes > 0) {
		int writeBytes = ::write(m_fd, ptr, leftBytes);
		if (writeBytes < 0) {
			if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
				err = errno;
				fprintf(stderr, "write failed with %d: %s\n", errno, strerror(errno));
				break;
			}
			writeBytes = 0;
		}

		ptr += writeBytes;
		leftBytes -= writeBytes;
	}

	return err;
}

bool ipc_connection_linux_UDS::set_block_mode(bool makeBlocking)
{
	int curFlags = fcntl(m_fd, F_GETFL, 0);

	int newFlags = 0;
	if (makeBlocking) {
		newFlags = curFlags & (~O_NONBLOCK);
	}
	else {
		newFlags = curFlags | O_NONBLOCK;
	}

	int status = fcntl(m_fd, F_SETFL, newFlags);
	if (status < 0) {
		return false;
	}
	return true;
}

void ipc_connection_linux_UDS::close(void)
{
	if (m_fd >= 0) {
		::close(m_fd);
		m_fd = -1;
	}
}

// own the completion queue:
//    1. add new pending request into the queue
//    2. 
// 
ipc_io_service::ipc_io_service()
{
	m_epollFd = epoll_create(m_epollSize);
	if (m_epollFd < 0) {
		std::error_code ec(errno, std::system_category());
		throw std::runtime_error(std::string("ipc_io_service ctor: epoll_create failed with ") + ec.message());
	}
}
ipc_io_service::~ipc_io_service()
{
	m_exit.store(true);
	close(m_epollFd);
}
OS_HANDLE ipc_io_service::native_handle(void)
{
	return m_epollFd;
}

void ipc_io_service::run()
{
	m_exit.store(false);
	//this thread will exit when m_exit is set
	// or the CompletionPort is closed
	while (!m_exit.load())
	{
		struct epoll_event events[m_maxEpollEvents];
		int numEvents = epoll_wait(m_epollFd, events, m_maxEpollEvents, m_epollTimeout);
		for (int i = 0; i < numEvents; i++)
		{
			int fd = events[i].data.fd;

			ipc_connection * pconn = get(fd);
			assert(pconn);
			pconn->notify(0, 0, events[i].events);

#if 0
			decltype(m_queue)::iterator it;
			{
				std::lock_guard<std::mutex> guard(m_mutex);

				if (events[i].events & EPOLLIN)
					it = m_queue.find(std::make_pair(fd, IOTYPE_IN));

				if (events[i].events & EPOLLOUT)
					it = m_queue.find(std::make_pair(fd, IOTYPE_OUT));

				if (it == m_queue.end()) {
					//no one is expecting it, keep it in the system buffer
					//for future
					//epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, NULL);
					continue;
				}
			}

			IOID &		ioid = it->first;
			IORequest &	ioreq = it->second;

			char * & 	   pdst = std::get<0>(ioreq);
			int &     	    cnt = std::get<1>(ioreq);
			IOHandler & handler = std::get<2>(ioreq);
			async_io_stream * paio = std::get<3>(ioreq);

			if (ioid.second == IOTYPE_IN) {
				int err = (cnt == 0) ? 0 : drain_read(fd, pdst, cnt);
				if ((err == EAGAIN) || (err == EWOULDBLOCK)) {
					//the data arrived are not enough, keep waitting
					//pdst & cnt is updated automatically by drain function
				}
				else {
					//otherwise its completed or error occurs
					{//request satisfied
						std::lock_guard<std::mutex> guard(m_mutex);
						m_queue.erase(it);
					}
					handler(make_IOEvent(ioid.second, err, cnt)); //callback
				}
			}
			else
				if (ioid.second == IOTYPE_OUT) {
					int err = (cnt == 0) ? 0 : drain_write(fd, pdst, cnt);
					if ((err == EAGAIN) || (err == EWOULDBLOCK)) {
					}
					else {
						{//request satisfied
							std::lock_guard<std::mutex> guard(m_mutex);
							m_queue.erase(it);
						}
						handler(make_IOEvent(ioid.second, err, cnt)); //callback
					}
				}
#endif
		}
	}
}


#endif




ipc_connection * ipc_connection::create(ipc_io_service & s, const char* ipc_type, const char * server_name)
{
	assert(server_name);
#ifdef WIN32
	if (strcmp(ipc_type, "") == 0 ||
		strcmp(ipc_type, "win_namedpipe") == 0) {
		return new ipc_connection_win_namedpipe(s, server_name);
	}
#endif
#ifdef linux
	if (strcmp(ipc_type, "") == 0 ||
		strcmp(ipc_type, "linux_UDS") == 0) {
		return new ipc_connection_linux_UDS(s, server_name);
	}
#endif
	return NULL;
}



void server_tst1(ipc_connection * pconn)
{
	char buff_rx[1024];
	int total = 0;
	pconn->read(&total, sizeof(total));

	printf("Client [%p] tst1 on %d bytes:\n", pconn, total);
	int id = 0;
	int ms0 = 0;
	auto time1 = std::chrono::steady_clock::now();
	while (id < total)
	{
		//int len = (rand() % sizeof(buff_rx)) + 1;
		int len = sizeof(buff_rx);

		if (len > total - id) len = total - id;

		int err;
		if ((err = pconn->read(buff_rx, len)) != 0) {
			std::error_code ec(err, std::system_category());
			printf(" Aborted with err=%d, %s, len=%d\n", ec.value(), ec.message().c_str(), len);
			return;
		}

		for (int k = 0; k < len && id < total; k++, id++) {
			if (buff_rx[k] != (char)id)
			{
				throw std::runtime_error(std::string("tst1 failed\n"));
			}
		}

		auto time2 = std::chrono::steady_clock::now();
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time2 - time1);;
		if (ms.count() > ms0 + 1000 || id >= total) {
			ms0 = ms.count();
			std::cerr << "\r" << id << "/" << total << "  (" << (int64_t)id * 100 / total << "%)  " << id / (ms0 * 1024 * 1024 / 1000) << "MB/s";
		}
	}
	std::cerr << " Done\n";
}



int main(int argc, char * argv[])
{

#ifdef linux
	const char * servername = "/var/tmp/hddl_service.sock";
	const char * ipc_type = "";	//default
#endif


#ifdef WIN32
	LPTSTR servername = TEXT("\\\\.\\pipe\\mynamedpipe");
	const char * ipc_type = "win_namedpipe";
#endif

	try {
		ipc_io_service	io_service;

		if (argc == 1) {
			//server mode

			std::shared_ptr<ipc_connection>				acceptor(ipc_connection::create(io_service, ipc_type, servername));
			std::vector<ipc_connection*>				connections;

			std::thread th(&ipc_io_service::run, &io_service);

			auto on_close = [&](ipc_connection * pconn) {
				auto it = std::find(connections.begin(), connections.end(), pconn);
				if (it == connections.end()) {
					fprintf(stderr, "Cannot find client connection %p when closing\n", pconn);
					return;
				}
				connections.erase(it);
				delete pconn;
				fprintf(stderr, "Client [%p] closed and deleted\n", pconn);
			};

			auto on_read = [&](ipc_connection * pconn, const std::error_code & ec, std::size_t lenx) {
				char buff_tx[1024];
				char buff_rx[1024];
				int len = 0;

				if (ec == std::errc::connection_aborted) {
					fprintf(stderr, "std::errc::connection_aborted\n");
					return;
				}
#ifdef WIN32
				if (ec.value() == ERROR_BROKEN_PIPE) {
					fprintf(stderr, "ERROR_BROKEN_PIPE\n");
					return;
				}
#endif
				if (ec)
					throw std::runtime_error(std::string("on_read failed with ") + ec.message());

				if (pconn->read(buff_rx, 4, &len)){
					fprintf(stderr, "connection read error\n");
					return;
				}

				if (len == 4 && strncmp(buff_rx, "tst1", 4) == 0) {
					//tst1 test
					server_tst1(pconn);
					return;
				}

				//async mode will return with 0 bytes if no other data in the pipe right now
				int len2 = 0;
				if (pconn->read(buff_rx + len, sizeof(buff_rx)-len, &len2)){
					fprintf(stderr, "connection read error\n");
					return;
				}
				len += len2;

				//response
				//async_write(buff_tx, 10, std::bind(&ipc_server1::on_write, this, _1, _2));
				printf("Client [%p] got %d bytes:", pconn, len);
				for (int i = 0; i < std::min<>(32, len); i++) {
					printf("%c", buff_rx[i]);
				}

				if (pconn->read(buff_rx, 1))
				{
					fprintf(stderr, "connection read error\n");
					return;
				}
				buff_rx[1] = 0;
				printf("And %s:", buff_rx);
				printf("\n");

				string_copy(buff_tx, "GOT IT");
				pconn->write(buff_tx, strlen(buff_tx));
			};

			auto on_accept = [&](ipc_connection * pconn) {
				connections.push_back(pconn);
				printf("Client [%p] connected.\n", pconn);
				pconn->on_close = on_close;
				pconn->on_read = on_read;
			};

			acceptor->on_accept = on_accept;
			acceptor->listen();

			printf("Waitting...\n");

			th.join();
		}
		else {
			//client mode

			std::shared_ptr<ipc_connection> client(ipc_connection::create(io_service, ipc_type));

			std::thread th(&ipc_io_service::run, &io_service);

			char buff_rx[32];
			char buff_tx[1024];

			client->on_read = [&](ipc_connection * pconn, const std::error_code & ec, std::size_t len) {
				printf("Server:");
				int cnt = 0;
				pconn->read(buff_rx, sizeof(buff_rx), &cnt);
				for (int i = 0; i < cnt; i++) {
					printf("%c", buff_rx[i]);
				}
				printf("\n");
			};
			client->on_close = [&](ipc_connection * pconn) {
				printf("Server closed\n");
				exit(0);
			};

			client->connect(servername);

			printf("Start client:\n");

			while (1) {
				printf("Input:"); fflush(stdout);

				//scanf_s("%10s", buff_tx, (unsigned)sizeof(buff_tx));
				std::cin.getline(buff_tx, sizeof(buff_tx));

				if (strcmp(buff_tx, "tst1") == 0)
				{
					//brute force test:
					//	send 1000 random length packet containinig continous numbers
					//
					client->write(buff_tx, strlen(buff_tx));
					int total = 1024 * 1024 * 100;
					client->write(&total, sizeof(total));
					int id = 0;
					while (id < total)
					{
						int len = (rand() % sizeof(buff_tx)) + 1;
						//int len = sizeof(buff_tx);
						int k;
						for (k = 0; k < len && id < total; k++, id++) {
							buff_tx[k] = id;
						}
						client->write(buff_tx, k);
					}
					continue;
				}

				printf("[len=%d]", strlen(buff_tx));
				client->write(buff_tx, strlen(buff_tx));
			}

			th.join();
		}
	}
	catch (const std::exception & ex) {
		std::cerr << "exception occurs: \n" << ex.what() << std::endl;
	}
	return 0;
}

