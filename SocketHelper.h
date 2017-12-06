#ifndef __HDDL_SOCKET_HELPER_H__
#define __HDDL_SOCKET_HELPER_H__

    // internet socket based interfaces
    int setupListenSocket(int listenPort, int maxConnNum, bool makeNonBlocking);
    int acceptConnectSocket(int listenfd, struct sockaddr_in* clientAddr);
    int setupConnectionSocket(char* remoteIP, int connectPort);

    // unix domain socket based interfaces
    int setupListenSocket(const char* serverName, int maxConnNum);
    int acceptConnectSocket(int listenfd, uid_t* uidptr);
    int setupConnectionSocket(const char* serverName);

    // general interfaces for socket read/write
    int readSocket(int socket, void* buffer, const int bufferSize);
    int writeSocket(int socket, void* buffer, const int bufferSize);
    int receiveFdsThroughSocket(int socket, int fdNum, int* fds);
    int sendFdsThroughSocket(int socket, int fdNum, int* fds);

    bool setSocketBlockMode(int sockfd, bool makeNonBlocking);

#endif

