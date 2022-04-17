#pragma once

#include <exception>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <vector>
#include <unordered_map>
#include <exception>

constexpr auto LISTEN_LOG = 5;

struct Connection {
    int fd;
    int uid;
    Connection(int fd = 0, int uid = -1):fd(fd), uid(uid) {}
};
class TcpServer
{
public:
    
    int listenfd;       //      listen socket
    int clientfd;
    
    TcpServer();
    ~TcpServer();

        //  init server, set port
    bool InitServer(const unsigned short port);

        //  wait a connection
    bool Accept();

        //  get user`s ip address
    char* GetIP(int fd);
    
    static u_int32_t Write(int fd,const char *buffer, int size = 0);
    static u_int32_t Read(int fd, char *buffer, int len, int flg = 0, int timeval = 0);

//private:
    socklen_t cliaddr_len;
    struct sockaddr_in cliaddr;
    struct sockaddr_in seraddr;

};

