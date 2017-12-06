// cross-platform IPC (Linux/Windows)
//   accept
//   listen
//   connect
//   send
//   recv
//   onrecv


#include "SysInclude.h"
#include "SocketHelper.h"

#include <functional>
#include <stdexcept>
#include <map>
#include <tuple>

// a basic IPC entity can:
// 1. read/write normal data(in a Blocking-way)
// 2. transfer (file descriptor on Linux)/(handle on Windows)
// 3. Asynchronous receive by provide callback.


//==========================================
// Use Proactor mode IPC for read/write:
//   1. initiate nonblocking/async IO request with completion handler.
//   2. when required bytes are transmitted, completion handler will be called
//
// under the hood, there are at least 1 background thread waiting on the completion queue
//
// Traditional blocking read/write also supported by specifying completion handler to be a notifier
// and waitting on the notification in a blocking way after initiate Proactor read/write.
//
// (This is less efficient than traditional because of context switch and addtional queue management)
//

// On Windows:
//      completion queue is directly supported by I/O Completion Port
// On Linux:
//      we have to use epoll & nonblocking read/write for the same purpose.

#ifdef WIN32


template <typename AsyncReadStream, typename ReadHandler>
void async_read(AsyncReadStream& s, const void * pbuff, const int len, ReadHandler handler)
{
    
}
template <typename AsyncReadStream, typename ReadHandler>
void async_write(const void * pbuff, const int len, ReadHandler handler)
{
    
}

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
    
    while(cnt > 0){
        r = read(fd, (void*)pdst, cnt);
        if(r < 0) return errno;
        
        pdst += r;
        cnt -= r;
    };
    return 0;
}
static int drain_write(int fd, char * & pdst, int &cnt)
{
    ssize_t r;
    
    while(cnt > 0){
        r = write(fd, (void*)pdst, cnt);
        if(r < 0) return errno;
        
        pdst += r;
        cnt -= r;
    }
    return 0;
}

// own the completion queue:
//    1. add new pending request into the queue
//    2. 
// 


class async_io_stream;

enum IOType {IOTYPE_IN, IOTYPE_OUT};
//typedef void(*IOHandler)(async_io_stream* s, int err, int cnt);
struct IOEvent{
	IOEvent(IOType t, int e, int c):type(t),err(e), cnt_left(c){}
	IOType  type;
	int 	err;
	int 	cnt_left;
};
#define make_IOEvent(t,e,c) IOEvent(t,e,c)
typedef std::function<void(IOEvent)> 							IOHandler;
typedef std::pair<int, IOType> 									IOID;		//first: fd, second: type
typedef std::tuple<char *, int, IOHandler, async_io_stream*> 	IORequest;

class async_io_service
{
public:
    void run()
    {
        while(1)
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

                    if(it == m_queue.end()){
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

                if(ioid.second == IOTYPE_IN){
                    int err = (cnt == 0)?0:drain_read(fd, pdst, cnt);
                    if((err == EAGAIN) || (err == EWOULDBLOCK)){
                        //the data arrived are not enough, keep waitting
                        //pdst & cnt is updated automatically by drain function
                    }else{
                        //otherwise its completed or error occurs
                        {//request satisfied
                            std::lock_guard<std::mutex> guard(m_mutex);
                            m_queue.erase(it);
                        }
                        handler(make_IOEvent(ioid.second, err, cnt)); //callback
                    }
                }
                else
                if(ioid.second == IOTYPE_OUT){
                    int err = (cnt == 0)?0:drain_write(fd, pdst, cnt);
                    if((err == EAGAIN) || (err == EWOULDBLOCK)){
                    }else{
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
    
    bool enqueue(IOID & id,  IORequest & r)
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        if(m_queue.find(id) != m_queue.end())
        	return false;

        m_queue.insert(std::make_pair(id, r));
        return true;
    }
    
    int                     m_epollFd;
    static const int        m_maxEpollEvents = 100;
    static const int        m_epollTimeout = 100;
    
    std::map<IOID, IORequest>    	m_queue;
    std::mutex                  	m_mutex;
};



class async_io_stream
{
public:
    async_io_stream(async_io_service &service, int fd): m_service(service), m_fd(fd){}
    operator int() { return m_fd; }

    void async_read(const void * pbuff, const int len, IOHandler handler)
    {
        char *  pdst = pbuff;
        int     cnt = len;

        int err = drain_read(m_fd, pdst, cnt);
        switch(err)
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

    void async_write(const void * pbuff, const int len, IOHandler handler)
    {
        char *  pdst = pbuff;
        int     cnt = len;

        int err = drain_write(m_fd, pdst, cnt);
        switch(err)
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

    void async_notify(IOHandler handler, IOType type)
    {
    	// in this case, user is interested in when there is a input ready to read
    	// but this request is difficult to satisfy on Windows ?
    	m_service.enqueue(
    			std::make_pair(m_fd, type),
				std::make_tuple(NULL, 0, handler, this));
    	return;
    }
private:
    async_io_service&    	m_service;
    int         			m_fd;
};




#endif

class ipc_peer
{
public:
    //a client can be created by 
    // 1. server. when it accepted a connection request
    //            in this case, it will use server's thread for 
    //            onrecv report (so server will handle onrecv())
    //
    // 2. user, when he/she wants to make a connection to an IPC server
    //            in this case, it needs own thread for onrecv 
    ipc_peer(bool b_create_recv_thread);
    
    ~ipc_peer(){
        disconnect();
    }
    
    int connect(const char * server_name);
    
    // disconnect will stop onrecv() thread
    int disconnect();

    int write(const void * pbuff, const int len);
    int read(const void * pbuff, const int len);
    
    void async_read(const void * pbuff, const int len, );
    void async_write(const void * pbuff, const int len, );

#ifdef WIN32
    int send_fd(HANDLE h);
    int recv_fd(HANDLE &h);
#else
    int send_fd(int fd);
    int recv_fd(int &fd);
#endif

    typedef std::function<void(ipc_peer &)> callback_onrecv_t;
    
    //will be called on peer data arrives(in a separated recv thread)
    //user can derived from ipc_client to handle data received
    //multiple clients can share thread for onrecv() callback
    callback_onrecv_t m_onrecv;

private:
    //dispatcher/reactor design pattern
    //  1. should be run inside a std::thread()
    //  2. callbacks/handlers will be invoked from this thread
    void run_dispatcher();
    
    // this function will stop dispatcher if there is one
    void stop_dispatcher();
};



//server is a framework accept connection request and help
//local client accept the connect from remote client
//
// server do not act like client, they don't send/recv data
//
class ipc_server
{
public:
    //it has a thread create server waiting for clients
    //once a client is connected, the client instance is created
    //and reported through callback onconn()
    ipc_server();
    
    //start/stop service
    void start(const char * name);
    void stop();
    
    typedef std::function<void(ipc_peer &)> callback_onconn_t;
    
    // a connection request arrives, user is informed in this callback
    // and they can register handler onrecv()
    callback_onconn_t m_onconn;

private:
    //dispatcher/reactor design pattern
    //  1. should be run inside a std::thread()
    //  2. callbacks/handlers will be invoked from this thread
    void run_dispatcher();
    
    // this function will stop dispatcher if there is one
    void stop_dispatcher();
    
    int                             m_fd; 
    std::shared_ptr<std::thread>    m_p_thread;
};



ipc_server::ipc_server()
{
    
}
void ipc_server::start(const char * name, int backlog)
{
    if(m_fd > 0) {
        stop();
    }
    
    m_fd = setupListenSocket(name, backlog);
    if(m_fd <= 0){
        throw std::runtime_error(std::string("setupListenSocket() failed with ") + std::to_string(m_fd));
    }
    
    //start dispatcher
    m_p_thread.reset(new std::thread(&ipc_server::run_dispatcher, this));
}
void ipc_server::stop()
{
    if(m_fd <= 0) {
        return;
    }
    
    if(m_p_thread){
        //inform dispatcher exit
        m_p_thread->join();
    }
    
    close(m_fd);
}

void ipc_server::run_dispatcher()
{
    //demultiplexer is a loop:
    //  1.wait for data arrive
    //  2.call corresponding handler
    for(;;)
    {
        
    }
}




