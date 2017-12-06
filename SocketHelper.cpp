#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "utils/SocketHelper.h"
#include "utils/SysInclude.h"
#include "utils/HLog.h"

#define CLI_PATH    "/var/tmp/"

#define MAKE_SOCKADDR_IN_H(var,hostAddr,hostPort) \
    struct sockaddr_in var; \
    var.sin_family = AF_INET; \
    if (hostAddr) { \
        var.sin_addr.s_addr = inet_addr(hostAddr); \
    } else { \
        var.sin_addr.s_addr = htonl(INADDR_ANY); \
    } \
    var.sin_port = htons(hostPort);

static int reuseFlag = 1;

int createStreamSocket()
{
    int newSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (newSocket < 0) {
        HError("failed to create stream socket: %s", strerror(errno));
        return newSocket;
    }

    fcntl(newSocket, F_SETFD, FD_CLOEXEC);

    if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR,
        (const char*)&reuseFlag, sizeof reuseFlag) < 0) {
        HError("setsockopt(SO_REUSEADDR) error: %s", strerror(errno));
        close(newSocket);
        return -1;
    }

    return newSocket;
}

int setupListenSocket(int listenPort, int maxConnNum, bool makeNonBlocking)
{
    int newSocket = createStreamSocket();
    if(newSocket <= 0) {
        return -1;
    }

    MAKE_SOCKADDR_IN_H(listenAddr, INADDR_ANY, listenPort);

    if(bind(newSocket, (struct sockaddr*)&listenAddr, sizeof(listenAddr)) != 0) {
        HError(" failed to bind socket at port %d", listenPort);
        close(newSocket);
        return -1;
    }

    if(!setSocketBlockMode(newSocket, makeNonBlocking)) {
        HError("failed to make non-blocking socket");
        close(newSocket);
        return -1;
    }

    if (listen(newSocket, maxConnNum) < 0) {
        HError("listen(%d) error: %s\n", newSocket, strerror(errno));
        close(newSocket);
        return -1;
    }

    return newSocket;
}

int acceptConnectSocket(int listenfd, struct sockaddr_in* clientAddr)
{
    int connfd = -1;

    if(!clientAddr) {
        connfd = accept(listenfd, NULL, NULL);
    } else {
        socklen_t addrLen = sizeof(clientAddr);
        connfd = accept(listenfd, (struct sockaddr*)&clientAddr, &addrLen);
    }

    return connfd;
}

int setupConnectionSocket(char* remoteIP, int connectPort)
{
    int newSocket = createStreamSocket();
    if(newSocket <= 0) {
        return -1;
    }

#ifdef REUSE_FOR_TCP
#ifdef SO_REUSEPORT
    if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEPORT,
        (const char*)&reuseFlag, sizeof reuseFlag) < 0) {
        HError("setsockopt(SO_REUSEPORT) error: %s\n", strerror(errno));
        close(newSocket);
        return -1;
    }
#endif
#endif

    MAKE_SOCKADDR_IN_H(remoteAddr, remoteIP, connectPort);

    if (connect(newSocket, (struct sockaddr*)&remoteAddr, sizeof(remoteAddr)) != 0) {
        HError("connect(%d) error: %s\n", newSocket, strerror(errno));
        close(newSocket);
        return -1;
    }

    return newSocket;
}

int setupListenSocket(const char* serverName, int maxConnNum)
{
    int fd, len, err, rval;
    struct sockaddr_un addr;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        HError("socket error: %s\n", strerror(errno));
        return -1;
    }

    unlink(serverName);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, serverName);

    len = offsetof(struct sockaddr_un, sun_path) + strlen(serverName);

    if(bind(fd, (struct sockaddr*)&addr, len) < 0) {
        HError("bind error: %s\n", strerror(errno));
        rval = -2;
        goto errout;
    }

    if(listen(fd, maxConnNum) < 0) {
        HError("listen error: %s\n", strerror(errno));
        rval = -3;
        goto errout;
    }

    return fd;

errout:
    err = errno;
    close(fd);
    errno = err;
    return(rval);
}

int acceptConnectSocket(int listenfd, uid_t* uidptr)
{
    int clifd, err, rval;
    struct stat statbuf;
    struct sockaddr_un addr;

    socklen_t len = sizeof(addr);
    if ((clifd = accept(listenfd, (struct sockaddr *)&addr, &len)) < 0) {
        HError("accept error: %s\n", strerror(errno));
        return -1;     /* often errno=EINTR, if signal caught */
    }

    /* obtain the client's uid from its calling address */
    len -= offsetof(struct sockaddr_un, sun_path); /* len of pathname */
    addr.sun_path[len] = 0;           /* null terminate */

    if (stat(addr.sun_path, &statbuf) < 0) {
        HError("stat error: %s\n", strerror(errno));
        rval = -2;
        goto errout;
    }

    if (S_ISSOCK(statbuf.st_mode) == 0) {
        HError("S_ISSOCK error: %s\n", strerror(errno));
        rval = -3;      /* not a socket */
        goto errout;
    }

    if (uidptr != NULL)
        *uidptr = statbuf.st_uid;   /* return uid of caller */
    unlink(addr.sun_path);        /* we're done with pathname now */
    return(clifd);

errout:
    err = errno;
    close(clifd);
    errno = err;
    return(rval);
}

int setupConnectionSocket(const char* serverName)
{
    int fd, len, err, rval;
    struct sockaddr_un addr;

    /* create a UNIX domain stream socket */
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        return(-1);

    /* fill socket address structure with our address */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    sprintf(addr.sun_path, "%s%05d", CLI_PATH, getpid());
    len = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path);

    unlink(addr.sun_path);        /* in case it already exists */
    if (bind(fd, (struct sockaddr *)&addr, len) < 0) {
        HError("bind error: %s\n", strerror(errno));
        rval = -2;
        goto errout;
    }

    /* fill socket address structure with server's address */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, serverName);
    len = offsetof(struct sockaddr_un, sun_path) + strlen(serverName);
    if (connect(fd, (struct sockaddr *)&addr, len) < 0) {
        HError("connect error: %s\n", strerror(errno));
        rval = -4;
        goto errout;
    }
    return(fd);

    errout:
    err = errno;
    close(fd);
    errno = err;
    return(rval);
}

/*
 * readSocket
 * Receive <bufferSize> bytes to <buffer> through <socket>.
 * This API ensures <bufferSize> are received before return or
 * reach end of file (EOF).
 *
 * @Parameter:
 *     socket:
 *     buffer:
 * bufferSize:
 *
 * @Return Value:
 * Success: Number of byte received.
 *          0 indicates end of file.
 *  Failed: -1, errno set.
 */
int readSocket(int socket, void* buffer, const int bufferSize)
{
    if(socket <= 0 || !buffer || bufferSize <= 0) {
        return -1;
    }

    char *ptr = static_cast<char*>(buffer);

    int leftBytes = bufferSize;
    while(leftBytes > 0) {
        int readBytes = read(socket, ptr, leftBytes);
        if (readBytes == 0) {
            /* reach EOF */
            return 0;
        } else if(readBytes < 0) {
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                return -1;
            }
            readBytes = 0;
        }

        ptr += readBytes;
        leftBytes -= readBytes;
    }

    return (bufferSize-leftBytes);
}

/* writeSocket
 * Write <bufferSize> bytes in <buffer> through <socket>
 * This API ensures write <bufferSize> bytes before return or
 * socket is closed (EPIPE).
 *
 * @Parameter:
 *      socket:
 *      buffer:
 *  bufferSize:
 * @Return Value:
 *    Success: The number of bytes written.
 *     Failed: -1, errno is set. EPIPE indicates the socket is closed.
 */
int writeSocket(int socket, void* buffer, const int bufferSize)
{
    if(socket <= 0 || !buffer || bufferSize <= 0) {
        return -1;
    }

    char *ptr = static_cast<char*>(buffer);

    int leftBytes = bufferSize;
    while(leftBytes > 0) {
        int writeBytes = write(socket, ptr, leftBytes);
        if(writeBytes < 0) {
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                return -1;
            }
            writeBytes = 0;
        }

        ptr += writeBytes;
        leftBytes -= writeBytes;
    }

    return (bufferSize-leftBytes);
}

bool setSocketBlockMode(int sockfd, bool makeNonBlocking)
{
    int curFlags = fcntl(sockfd, F_GETFL, 0);

    int newFlags = 0;
    if (makeNonBlocking) {
        newFlags = curFlags | O_NONBLOCK;
    } else {
        newFlags = curFlags & (~O_NONBLOCK);
    }

    int status = fcntl(sockfd, F_SETFL, newFlags);
    if(status < 0) {
        return false;
    }

    return true;
}

/* sendFdsThroughSocket
 * Send <fdNum> fd from <fds> through <sock>.
 * This API sends <fdNum> of fds before return or
 * socket is closed (EPIPE).
 *
 * @Parameter:
 *      socket : Socket from which to send fd
 *       fdNum : Number of fd to send
 *         fds : Fd array holding the fd to send
 * @Return Value:
 *    Success: Return the number of fd sent.
 *    Failure: -1, errno is set.
 *             EPIPE means read-end of the socket is closed.
 */
int sendFdsThroughSocket(int socket, int fdNum, int* fds)
{
    if (socket <= 0 || fdNum <= 0 || fds == NULL) {
        HDebug("socket = %d, fdNum = %d, fds = %p", socket, fdNum, fds);
        errno = EINVAL;
        return -1;
    }

    int ret = 0;

    struct iovec iov[1];
    struct msghdr msg;
    char cmsgBuf[CMSG_LEN(sizeof(int))];

    iov[0].iov_base = &fdNum;
    iov[0].iov_len = sizeof(fdNum);

    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    struct cmsghdr *cmsg;
    cmsg = (struct cmsghdr*)cmsgBuf;
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));

    msg.msg_control = cmsg;
    msg.msg_controllen = CMSG_LEN(sizeof(int));


    int cnt = 0;
    while(cnt < fdNum) {
        *(int *)CMSG_DATA(cmsg) = fds[cnt];

        ret = sendmsg(socket, &msg, 0);
        if (ret < 0) {
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                if (errno == EPIPE) {
                    /* This error must capture or ignore SIGPIPE to get. */
                    HError("Error: Read-end of socket %d is closed\n", socket);
                } else {
                    HError("Error: send fd(%d) through socket(%d) failed. errno = %d\n", fds[cnt], socket, errno);
                }
                return -1;
            }
            continue;
        }
        cnt++;
    }

    return cnt;
}


/* receiveFdsThroughSocket
 * Receive <fdNum> fd from <sock> and save to <fds>
 * This API ensure to receive <fdNum> fds before return or reach
 * end of file.
 *
 * @Parameter:
 *    socket : Socket from which to receive fd
 *     fdNum : Number of fd to receive
 *       fds : Fd array to hold the received fd.
 * @Return Value:
 *    Success: Return the number of fd received
 *             0 indicates end of file.
 *    Failure: -1, errno is set.
 */
int receiveFdsThroughSocket(int socket, int fdNum, int* fds)
{
    if (socket <= 0 || fdNum <= 0 || fds == NULL) {
        errno = EINVAL;
        return -1;
    }

    struct cmsghdr *cmsg;
    char   cmsgBuf[CMSG_LEN(sizeof(int))];

    int ret = 0;
    int cnt = 0;
    int rfd = 0;
    int num_fd = 0;

    struct iovec iov[1];
    struct msghdr msg;

    iov[0].iov_base = &num_fd;
    iov[0].iov_len = sizeof(int);

    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    cmsg = (struct cmsghdr*)cmsgBuf;
    msg.msg_control = cmsg;
    msg.msg_controllen = CMSG_LEN(sizeof(int));

    while(cnt < fdNum) {
        ret = recvmsg(socket, &msg, 0);
        if (ret == 0) {
            HError("Error: Write-end of va socket (%d) is closed.");
            return 0;
        }
        if (ret < 0) {
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                HError("Error: receive fd through socket(%d) failed. errno = %d\n", socket, errno);
                return -1;
            }
            continue;
        }

        rfd = *(int*)CMSG_DATA(cmsg);
        if (rfd < 0) {
            HError("Error: could not get recv_fd from socket");
            return -1;
        }

        fds[cnt] = rfd;
        cnt++;
    }

    return cnt;
}


