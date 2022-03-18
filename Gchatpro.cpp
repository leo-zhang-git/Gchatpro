// Gchatpro.cpp: 定义应用程序的入口点。
//

#include <iostream>

#include "TcpServer.h"
#include "json/json.h"

constexpr int MAX_EVENTS = 500000;

static TcpServer server;
constexpr unsigned short port = 8011;
epoll_event ev, events[MAX_EVENTS];
int epollfd;


void AddConnect(epoll_event &event);
void DelConnect(epoll_event &event);

void init() {
	server;
	if (!server.InitServer(port)) 
	{
		std::cout << "server init failed ! " << std::endl;
		exit(-1);
	}
	else
	{
		std::cout << "server init complete ! listenfd: " << server.listenfd << std::endl;
	}

	//  init epoll attribute
	epollfd = epoll_create1(0);
	if (epollfd == -1)
	{
		std::cout << "epollfd create failed !" << std::endl;
		exit(-1);
	}

	ev.events = EPOLLIN;
	ev.data.fd = server.listenfd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, server.listenfd, &ev) == -1)
	{
		std::cout << "epoll add listenfd failed !" << std::endl;
		exit(-1);
	}


}
void waitConnect() {
	int nfds;
	while (true) {
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if (nfds < 0)
		{
			std::cout << "epoll wait failed !" << std::endl;
			continue;
		}
		else if (!nfds)
		{
			//  timeout
			std::cout << "epoll wait timeout!" << std::endl;
			continue;
		}

		for (int i = 0; i < nfds; i++) 
		{
			
			if (events[i].data.fd == server.listenfd)
			{  
				//  having new connection
				AddConnect(events[i]);
				std::cout << "new Connection sock: " << server.clientfds[server.clientfds.size() - 1].fd << " address" << server.GetIP() << std::endl;
			}
			else if (server.Read(events[i].data.fd, server.buffer, BUFFER_SIZE) <= 0) 
			{	
				//  a connection closed
				std::cout << "close socket : " << events[i].data.fd << std::endl;
				DelConnect(events[i]);
			}
			else 
			{  
				//  deal IO
				std::cout << "recv : " << server.buffer << std::endl;
				server.Write(events[i].data.fd, server.buffer, BUFFER_SIZE);
			}
		}
	}
}
int main()
{
	init();
	waitConnect();
	return 0;
}

void AddConnect(epoll_event& event) {
	server.Accept();
	int fd = server.clientfds[server.clientfds.size() - 1].fd; 
	ev.events = EPOLLIN;
	ev.data.fd = fd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1)
	{
		std::cout << "epoll add new fd failed !" << std::endl;
		exit(-1);
	}
}
void DelConnect(epoll_event& event) {
	epoll_ctl(epollfd, EPOLL_CTL_DEL, event.data.fd, &event);
	close(event.data.fd);
}
