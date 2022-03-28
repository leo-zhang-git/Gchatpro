#include "TcpServer.h"


TcpServer::TcpServer()
{
    listenfd = -1;
    cliaddr_len = sizeof cliaddr;
}

TcpServer::~TcpServer()
{
    close(listenfd);

}

bool TcpServer::InitServer(const unsigned short port)
{
    if(listenfd > 0) {close(listenfd); listenfd = -1;}


        //      create a server listen socket

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0) return false;
        
        //      set SO_REUSEADDR (允许bind()过程中地址可重复使用)

        //      Linux
    int opt = 1; unsigned int len = sizeof opt;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, len);

        //      Windows
    // char b_opt = "1";
    // setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &b_opt, sizeof b_opt);


    
        //      set protocl address to server

    memset(&seraddr, 0, sizeof seraddr);
    seraddr.sin_family = AF_INET;
    seraddr.sin_port = htons(port);
    seraddr.sin_addr.s_addr = htonl(INADDR_ANY);


        //      bind the address to server socket

    if(bind(listenfd, (struct sockaddr*) &seraddr, sizeof seraddr))
    {
        close(listenfd);
        return false;
    }
        

        //      set listen mod

    listen(listenfd, LISTEN_LOG);
    return true;
}

bool TcpServer::Accept()
{
    if(listenfd == -1) return false;


        //      create a client socket
    clientfd = accept(listenfd, (struct sockaddr*) &cliaddr,(socklen_t*) &cliaddr_len);
    if(clientfd < 0) return false;
    return true;
}

char* TcpServer::GetIP(int fd)
{
    try {
        getpeername(fd, (struct sockaddr*)&cliaddr, &cliaddr_len);
    }
    catch (std::exception e) {
        std::cout << "getip exception ! \n";
    }
    return inet_ntoa(cliaddr.sin_addr);
}

u_int32_t TcpServer::Write(int fd,const char *buffer, int size)
{
    if(!size) size = strlen(buffer);
    return send(fd, buffer, size, 0);
}

u_int32_t TcpServer::Read(int fd,char *buffer, int len, int flg)
{   
    int res =  recv(fd, buffer, len, flg);
    buffer[res] = 0;
    return res;
}
