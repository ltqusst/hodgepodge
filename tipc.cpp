// tipc.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

// cross-platform IPC (Linux/Windows)
//   accept
//   listen
//   connect
//   send
//   recv
//   onrecv


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
//==================================================================
// Async IPC API
// 
// On Windows:
//      I/O Completion Port based NamedPipe
// On Linux:
//      epoll & nonblocking read/write based Unix Domain Sockets
//==================================================================

#ifdef WIN32
#include "windows.h"
#else
#include "SysInclude.h"
#include "SocketHelper.h"
#endif

#ifdef WIN32
typedef HANDLE OS_HANDLE;
#define INVALID_OS_HANDLE INVALID_HANDLE_VALUE
#define CLOSE_OS_HANDLE(a) CloseHandle(a)
#else
typedef int OS_HANDLE;
#define INVALID_OS_HANDLE -1
#define CLOSE_OS_HANDLE(a) close(a)
#endif



class ipc_connection;
class ipc_io_service;

//
//
// service:   run in its own thread (so all callbacks also called from there, make sure do not blocking inside call back)
//
// executor:  with the help of service coupled together
//			  this helper fulfills common async IO(include passive connection) requests
//
// one service can co-work with many different kind of executors, 
// for example, socket and namedPipe can be implemented in 2 different executor
// 
//-------------
// user point of view is:
//     service/executor is nothing but a pair of helper used to archive async capability
//     for some originally blocking IO executor, and those executor is pure C-Style OS API like Socket/NamedPipe
//     
//     
//   service::service()
//	 service.run()
//   service.associate()
// 
//   executor_base::async_read
//   executor_base::async_write
//   executor_base::async_accept
//
//   actuall executor will derive from executor_base
//
// key point of design of the IPC component is:
// 1. hide OS differences.
// 2. single thread IPC server.
//
// so we choose to implement:
// 1. async_accept & async_read.	they are the key to do single thread IO
// 2. read/write of sync version.   
//    (async write is not so useful for replying message, paralell output capability is not so useful in our case )
// 3. async version will invoke callback when completed. 
//      inside callback, 
//          async_read can be called only once
//          read/write can be called as many times as needed.
// 
// 
// we need OS dependent code to setup connection and 

class ipc_io_service
{
public:
	ipc_io_service();
	~ipc_io_service();

	void run();
	void associate(ipc_connection * pconn);

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
		unsigned long v = (unsigned long) oshd;
		if (v >= 0 && v < m_map_fast_size) 
			return m_map_fast[v];

		if (m_map.find(oshd) == m_map.end())
			m_map.insert(std::make_pair(oshd, (ipc_connection*)NULL));

		return m_map[oshd];
	}

#ifdef WIN32
	//WIN32
	HANDLE							m_h_io_compl_port;

	//there is no need to enqueue because Windows does that
	//automatically inside ReadFile/WriteFile

#else
	//Linux

	static const int        m_maxEpollEvents = 100;

	// we have to simulate the I/O completion queue
	// based on epoll, so we need the ability to:
	//		1. queue the request
	bool enqueue(IOID & id, IORequest & r);

	//		2. locate the request when some fd is ready to IN/OUT
	//		3. request of same type on same fd should be queued in FIFO way
	std::multimap<IOID, IORequest>	m_multimap;

	int								m_epollFd;
	std::map<IOID, IORequest>    	m_queue;
	std::mutex                  	m_mutex;
#endif
};


class ipc_connection
{
public:
	ipc_connection(ipc_io_service & s):m_service(s){}
	virtual ~ipc_connection() {
		if (m_oshd != INVALID_OS_HANDLE) {
			//close it
			CLOSE_OS_HANDLE(m_oshd);
			m_oshd = INVALID_OS_HANDLE;
		}
	}
	static ipc_connection * create(ipc_io_service & s, const char* ipc_type);

	virtual OS_HANDLE native_handle() const { return m_oshd; }
	virtual void set_native_handle(OS_HANDLE oshd) {
		m_oshd = oshd; 
		m_service.associate(this);	//change association as well
	}

	virtual ipc_io_service & get_service() { return m_service; }

	//on Windows, notify means one IO request is completed or error occured
	//on Linux, notify means one IO request type is ready to issue without blocking
	virtual void notify(int error_code, int transferred_cnt, unsigned long hint) = 0;

	virtual int async_accept(void) = 0;									//passive connect for server side( like accept)

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
	virtual int connect(const char * dest) = 0;
	virtual int bind(const char * dest) = 0;
	virtual void close(void) = 0; //close connection

	//async callbacks are free for register by simple assign
	std::function<void(ipc_connection * pconn, const std::error_code & ec, std::size_t len)>	on_read;
	std::function<void(ipc_connection * pconn)>													on_accept;
	std::function<void(ipc_connection * pconn)>													on_close;

private:
	ipc_io_service &    	m_service;
	OS_HANDLE				m_oshd;

	
};



#ifdef WIN32

// this class is an extention to fd/handle
// on Windows, scheduler can locate it through CompletionKey.
// on Linux, this needs derived from fd.
//           https://stackoverflow.com/questions/8175746/is-there-any-way-to-associate-a-file-descriptor-with-user-defined-data
class ipc_connection_win_namedpipe : public ipc_connection
{
public:
	ipc_connection_win_namedpipe(ipc_io_service & service);
	~ipc_connection_win_namedpipe();
	virtual void notify(int error_code, int transferred_cnt, unsigned long hint);

	//as defined by socket, accept() returns a connected executor for communication with client
	virtual int async_accept(void);

	//blocking/sync version(based on async version)
	virtual int read(void * pbuff, const int len, int *lp_cnt);
	virtual int write(void * pbuff, const int len);

	//client(blocking is acceptable because usually its short latency)
	virtual int connect(const char * dest);
	virtual int bind(const char * dest);
	virtual void close(void);
protected:

private:
	void trigger_async_cache_read(void);
		
	// <0 means no cached byte0
	// >0 means 1 byte is cached and should be filled to user's buffer first
	unsigned char			m_cache_byte0;	
	bool					m_cache_empty;
	OVERLAPPED				m_cache_overlapped;
	OVERLAPPED				m_error_overlapped;
	OVERLAPPED				m_accept_overlapped;
	OVERLAPPED				m_sync_overlapped;

	const char *			m_name;		//IPC name
};

ipc_connection_win_namedpipe::ipc_connection_win_namedpipe(ipc_io_service &service) : 
	ipc_connection(service), m_cache_byte0(0), m_cache_empty(true), m_cache_overlapped{}, m_error_overlapped{}, m_sync_overlapped{}
{
	m_sync_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}
ipc_connection_win_namedpipe::~ipc_connection_win_namedpipe()
{
	CloseHandle(m_sync_overlapped.hEvent);
}

void ipc_connection_win_namedpipe::notify(int error_code, int transferred_cnt, unsigned long hint)
{
	OVERLAPPED * poverlapped = (OVERLAPPED *)hint;
	
	const char * pevt = "";
	if (poverlapped == &m_cache_overlapped) pevt = "cache";
	if (poverlapped == &m_error_overlapped) pevt = "error";
	if (poverlapped == &m_accept_overlapped) pevt = "accept";
	
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
	else if (poverlapped == &m_accept_overlapped) {
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
			ipc_connection_win_namedpipe * pconn = new ipc_connection_win_namedpipe(get_service());
			pconn->set_native_handle(this->native_handle());	//transfer current pipe to this new instance
			
			//create a new instance of the pipe
			bind(m_name);

			//user will setup callbacks inside on_accept()
			on_accept(pconn);

			//trigger
			pconn->trigger_async_cache_read();
		}
	}else
		return;
}

int ipc_connection_win_namedpipe::async_accept(void)
{
	assert(native_handle() != INVALID_OS_HANDLE || (fprintf(stderr, "%s:%d %s", __FILE__, __LINE__, "  m_oshd == NULL!"), 0));

	BOOL bRet = ConnectNamedPipe(native_handle(), &m_accept_overlapped);
	if (bRet) {
		//connection success, no IO pending, make sure on_accept is called within right thread
		//according to behaviour of ReadFile, completion IO event should be trigered automatically

		//PostQueuedCompletionStatus(get_service().native_handle(), 1, (ULONG_PTR)native_handle(), &m_accept_overlapped);
	}
	else {
		DWORD dwErr = GetLastError();
		if(dwErr == ERROR_PIPE_CONNECTED)
			PostQueuedCompletionStatus(get_service().native_handle(), 1, (ULONG_PTR)native_handle(), &m_accept_overlapped);

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

	fprintf(stderr, ">");

	//clear the cache if there is data
	if (!m_cache_empty) {
		pbuff[cnt] = m_cache_byte0;
		cnt++;
		m_cache_empty = true;
	}

	if (lp_cnt) *lp_cnt = 0;

	while(cnt < size && error_code == 0) {
		DWORD NumberOfBytesTransferred = 0;
		BOOL bRet = ReadFile(native_handle(), pbuff + cnt, size - cnt, &NumberOfBytesTransferred, &m_sync_overlapped);
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
	fprintf(stderr, "<");

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

int ipc_connection_win_namedpipe::connect(const char * dest)
{
	HANDLE oshd = CreateFile(dest,   // pipe name 
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
	}

	m_name = dest;
	this->set_native_handle(oshd);
	trigger_async_cache_read();
}

int ipc_connection_win_namedpipe::bind(const char * dest)
{
	//server
#define PIPE_TIMEOUT 5000
#define BUFSIZE 4096
	HANDLE oshd = CreateNamedPipe(dest,            // pipe name 
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

	// bind() is only for ipc server 
	// which do not do IO at all, only async_accept() will be called

	m_name = dest;
	this->set_native_handle(oshd);
}

void ipc_connection_win_namedpipe::close(void)
{
	if (native_handle() != INVALID_OS_HANDLE) {
		//close it
		CLOSE_OS_HANDLE(native_handle());
		set_native_handle(INVALID_OS_HANDLE);
	}
}

//==================================================================================================================
ipc_io_service::ipc_io_service():
	m_map_fast{}
{
	m_h_io_compl_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL, 0, 0);
	if (m_h_io_compl_port == NULL) {
		std::error_code ec(GetLastError(), std::system_category());
		throw std::runtime_error(std::string("CreateIoCompletionPort failed with ") + ec.message());
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
void ipc_io_service::associate(ipc_connection * pconn)
{
	assert(pconn != NULL);

	OS_HANDLE oshd = pconn->native_handle();
	
	if (oshd == INVALID_OS_HANDLE) return;

	ipc_connection* p_conn = get(oshd);

	if (p_conn == NULL) {
		//first time association: register into IO completion port system.
		//we use the handle as completion key directly to be more compatible with linux/epoll
		//though this is not the best solution
		if (NULL == CreateIoCompletionPort(oshd, m_h_io_compl_port, (ULONG_PTR)oshd, 0)) {
			std::error_code ec(GetLastError(), std::system_category());
			throw std::runtime_error(std::string("associate() CreateIoCompletionPort failed with ") + ec.message());
		}
	}

	//internal association is mutable (although CreateIoCompletionPort can be down only once)
	get(oshd) = pconn;
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
		DWORD dwErr = bSuccess ? 0:GetLastError();

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


// errno will never set to zero by system call
// but drain_read/write return non-zero errno only 
// when error occurs
//
// pointer pdst & cnt updated accordingly
//
static int drain_read(int fd, char * & pdst, int &cnt)
{
	ssize_t r;

	while (cnt > 0) {
		r = read(fd, (void*)pdst, cnt);
		if (r < 0) return errno;

		pdst += r;
		cnt -= r;
	};
	return 0;
}
static int drain_write(int fd, char * & pdst, int &cnt)
{
	ssize_t r;

	while (cnt > 0) {
		r = write(fd, (void*)pdst, cnt);
		if (r < 0) return errno;

		pdst += r;
		cnt -= r;
	}
	return 0;
}

// own the completion queue:
//    1. add new pending request into the queue
//    2. 
// 
ipc_io_service::ipc_io_service()
{

}

void ipc_io_service::run()
{
	while (1)
	{
		struct epoll_event events[m_maxEpollEvents];
		int numEvents = epoll_wait(m_epollFd, events, m_maxEpollEvents, m_epollTimeout);
		for (int i = 0; i < numEvents; i++)
		{
			int fd = events[i].data.fd;

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
		}
	}
}

bool ipc_io_service::enqueue(IOID & id, IORequest & r)
{
	std::lock_guard<std::mutex> guard(m_mutex);
	if (m_queue.find(id) != m_queue.end())
		return false;

	m_queue.insert(std::make_pair(id, r));
	return true;
}


async_io_stream::async_io_stream(ipc_io_service &service, int fd) : m_service(service), m_fd(fd) 
{
}
void async_io_stream::async_read(const void * pbuff, const int len, IOHandler handler)
{
	char *  pdst = pbuff;
	int     cnt = len;

	int err = drain_read(m_fd, pdst, cnt);
	switch (err)
	{
	case 0:
		handler(make_IOEvent(IOTYPE_IN, 0, cnt));                  //completed on-site
		break;
	case EAGAIN:
	case EWOULDBLOCK:
		m_service.enqueue(
			std::make_pair(m_fd, IOTYPE_IN),
			std::make_tuple(pdst, cnt, handler, this));
		//queue the request for async execution
		break;
	default:
		handler(make_IOEvent(IOTYPE_IN, err, cnt));              //error
	}
}

void async_io_stream::async_write(const void * pbuff, const int len, IOHandler handler)
{
	char *  pdst = pbuff;
	int     cnt = len;

	int err = drain_write(m_fd, pdst, cnt);
	switch (err)
	{
	case 0:
		handler(make_IOEvent(IOTYPE_OUT, 0, cnt));                  //completed on-site
		break;
	case EAGAIN:
	case EWOULDBLOCK:
		m_service.enqueue(
			std::make_pair(m_fd, IOType::IOTYPE_OUT),
			std::make_tuple(pdst, cnt, handler, this));
		//queue the request for async execution
		break;
	default:
		handler(make_IOEvent(IOTYPE_OUT, err, cnt));              //error
	}
}

void async_io_stream::async_notify(IOHandler handler, IOType type)
{
	// in this case, user is interested in when there is a input ready to read
	// but this request is difficult to satisfy on Windows ?
	m_service.enqueue(
		std::make_pair(m_fd, type),
		std::make_tuple(NULL, 0, handler, this));
	return;
}


#endif






#ifdef WIN32
class ipc_connection_win_namedpipe;
#endif

ipc_connection * ipc_connection::create(ipc_io_service & s, const char* ipc_type)
{
	if (strcmp(ipc_type, "win_namedpipe") == 0) {
		return new ipc_connection_win_namedpipe(s);
	}
	return NULL;
}




using namespace std::placeholders;


void server_tst1(ipc_connection * pconn)
{
	char buff_tx[1024];
	char buff_rx[1024];
	int total = 0;
	pconn->read(&total, sizeof(total));

	printf("Client [%p] tst1 on %d bytes:\n", pconn, total);
	int id = 0;
	while (id < total)
	{
		int len = rand() % _countof(buff_rx);
		if (len > total - id) len = total - id;

		if (pconn->read(buff_rx, len)) {
			printf(" Aborted\n");
			return;
		}

		for (int k = 0; k < len && id < total; k++, id++) {
			if (buff_rx[k] != (char)id)
			{
				throw std::runtime_error(std::string("tst1 failed\n"));
			}
		}
		printf("\r%d/%d  (%d%%)   ", id, total, id * 100 / total);
	}
	printf(" Done\n");
}



int main(int argc, char * argv[])
{
	LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");

#ifdef WIN32
	const char * ipc_type = "win_namedpipe";
#endif

	try {
		ipc_io_service	io_service;

		if (argc == 1) {
			//server mode
			
			std::shared_ptr<ipc_connection>				acceptor(ipc_connection::create(io_service, ipc_type));
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

				if (ec.value() == ERROR_BROKEN_PIPE) {
					fprintf(stderr, "ERROR_BROKEN_PIPE\n");
					return;
				}

				if (ec)
					throw std::runtime_error(std::string("on_read failed with ") + ec.message());

				if (pconn->read(buff_rx, 4))
				{
					fprintf(stderr, "connection read error\n");
					return;
				}

				if (strncmp(buff_rx, "tst1", 4) == 0) {
					//tst1 test
					server_tst1(pconn);
					return;
				}

				if (pconn->read(buff_rx + 4, sizeof(buff_rx) - 4, &len))
				{
					fprintf(stderr, "connection read error\n");
					return;
				}
				len += 4;

				//response
				//async_write(buff_tx, 10, std::bind(&ipc_server1::on_write, this, _1, _2));
				printf("Client [%p] got %d bytes:", pconn, len);
				for (int i = 0; i < len; i++) {
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

				strcpy_s(buff_tx, sizeof(buff_tx), "GOT IT");
				pconn->write(buff_tx, strlen(buff_tx));
			};

			auto on_accept = [&](ipc_connection * pconn) {
				connections.push_back(pconn);
				printf("Client [%p] connected.\n", pconn);
				pconn->on_close = on_close;
				pconn->on_read = on_read;
				acceptor->async_accept();   //accept next connection(socket model)
			};

			acceptor->on_accept = on_accept;
			acceptor->bind(lpszPipename);
			acceptor->async_accept();

			printf("Waitting...\n");

			th.join();
		}
		else {
			//client mode

			std::shared_ptr<ipc_connection> client(ipc_connection::create(io_service, ipc_type));
			
			std::thread th(&ipc_io_service::run, &io_service);

			char buff_rx[32];
			char buff_tx[32];

			client->on_read = [&](ipc_connection * pconn, const std::error_code & ec, std::size_t len) {
				printf("Server:");
				int cnt = 0;
				pconn->read(buff_rx, sizeof(buff_rx), &cnt);
				for (int i = 0; i < cnt; i++) {
					printf("%c", buff_rx[i]);
				}
				printf("\n");
			};

			client->connect(lpszPipename);

			printf("Start client:\n");

			while (1) {
				printf("Input:"); fflush(stdout);
				scanf_s("%10s", buff_tx, (unsigned)_countof(buff_tx));
				if (buff_tx[0] == 'q')
					break;

				if (strcmp(buff_tx, "tst1") == 0)
				{
					//brute force test:
					//	send 1000 random length packet containinig continous numbers
					//
					client->write(buff_tx, strlen(buff_tx));
					int total = 1024 * 1024 * 1024;
					client->write(&total, sizeof(total));
					int id = 0;
					while(id < total)
					{
						int len = rand() % _countof(buff_tx);
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
		std::cerr << "exception occurs: \n" <<  ex.what() << std::endl;
	}
    return 0;
}

