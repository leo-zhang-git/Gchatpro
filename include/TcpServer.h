#pragma once

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

constexpr auto LISTEN_LOG = 5;
constexpr auto BUFFER_SIZE = 5242880;

struct Connection {
    int fd;
    int uid;
    Connection(int fd = 0, int uid = -1):fd(fd), uid(uid) {}
};
class TcpServer
{
public:
    
    int listenfd;       //      listen socket
    std::vector<Connection> clientfds;       //      client socket
    char buffer[BUFFER_SIZE]; 
    
    TcpServer();
    ~TcpServer();

        //  init server, set port
    bool InitServer(const unsigned short port);

        //  wait a connection
    bool Accept();

        //  get user`s ip address
    char* GetIP();
    
    static int Write(int fd, char *buffer, int size = 0);
    static int Read(int fd, char *buffer, int len);

//private:
    int cliaddr_len;
    struct sockaddr_in cliaddr;
    struct sockaddr_in seraddr;

};

