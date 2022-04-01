// Gchatpro.cpp: 定义应用程序的入口点。

#include <iostream>
#include <memory>
#include <unordered_map>

#include "TcpServer.h"
#include "json/json.h"
#include "threadpool.h"
#include "DBC.h"
constexpr u_int32_t MAX_EVENTS = 500000;
constexpr u_int32_t BUFFER_SIZE = 65535;
constexpr u_int16_t HEAD_SIZE = 12;

enum Act
{
	ACT_SIGNUP = 0,
	ACT_SIGNIN = 1,
	ACT_CHATF = 2,
	ACT_ACKSIGNUP = 3,
	ACT_ACKSIGNIN = 4,
	ACT_ACKCHATF = 5,
	ACT_SIGNOUT = 6
};

std::threadpool tp;

//  mysql connect
const char* host = "0.0.0.0";
const char* user = "root";
const char* pwd = "zw0727@qq.cn";
const char* dbname = "Gchatpro";
unsigned int dbport = 3306;
zwdbc::Connectpool cp{ host, user, pwd, dbname, dbport };


std::mutex _outlock;


TcpServer server;
epoll_event ev, events[MAX_EVENTS];
int epollfd;

std::unordered_map<int, int> getsock;
std::unordered_map<int, int> getid;

void AddConnect(epoll_event &event);
void DelConnect(int fd);
void DealInput(int fd);
void DoTask(std::shared_ptr<char>& buffer, int fd);
void Signup(Json::Value& jroot, int fd);
void Signin(Json::Value& jroot, int fd);
void Signout(int fd);


void sendMessage(int fd, const std::string& s)
{
	u_int32_t len = s.size();
	std::unique_ptr<char> buffer(new char[BUFFER_SIZE]);
	memset(buffer.get(), 0, HEAD_SIZE);
	memcpy(buffer.get() + HEAD_SIZE, s.c_str(), len);
	len = htonl(len);
	memcpy(buffer.get(), &len, 4);

	std::lock_guard<std::mutex> outlock(_outlock);
	if (server.Write(fd, buffer.get(), s.size() + HEAD_SIZE) <= 0)
		DelConnect(fd);
}

bool isEqual(const char* a,const char* b)
{
	int n = strlen(a);
	if (n != strlen(b)) return false;
	for (int i = 0; i < n; i++)
	{
		if (a[i] != b[i]) return false;
	}
	return true;
}

void init() 
{
	//  init server
	constexpr u_int16_t listenport = 8011;
	if (!server.InitServer(listenport)) 
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
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = server.listenfd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, server.listenfd, &ev) == -1)
	{
		std::cout << "epoll add listenfd failed !" << std::endl;
		exit(-1);
	}
}
void waitEvent() 
{
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
				std::cout << "new Connection sock: " << server.clientfd << " address" << server.GetIP(server.clientfd) << std::endl;
			}
			else 
			{
				//  deal IO
				DealInput(events[i].data.fd);
			}
			
		}
	}
}

int main()
{
	init();
	waitEvent();
	return 0;
}

void AddConnect(epoll_event& event) 
{
	server.Accept();
	int fd = server.clientfd; 
	ev.events = EPOLLIN|EPOLLET;
	ev.data.fd = fd;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1)
	{
		std::cout << "epoll add new fd failed !" << std::endl;
		exit(-1);
	}
	getid[fd] = -1;
}

void DelConnect(int fd) 
{
	if (getid[fd] == -1) getid.erase(fd);
	else Signout(fd);
	epoll_event evt;
	evt.data.fd = fd;
	epoll_ctl(epollfd, EPOLL_CTL_DEL, evt.data.fd, &evt);
	std::cout << "close socket : " << fd << std::endl;
	close(evt.data.fd);
}

void DealInput(int fd) 
{
	std::shared_ptr<char> buffer(new char[BUFFER_SIZE]);
	u_int32_t len = 0;
	bool isconnect = false;

	while (true) 
	{
		// read buffer is empty
		int t = 0;
		if ( (t = server.Read(fd, buffer.get(), HEAD_SIZE, 0, 2)) <= 0)
		{
			//  the first loop is empty, it prove that connect is closed
			if (!isconnect)
			{
				DelConnect(fd);
			}
			else 
			{
				std::cout << "buffer is empty\n";
			}
			return;
		}
		else 
		{
			isconnect = true;
			std::cout << "read a message\n";
		}
		if (t < 12)
		{
			std::cout << "recv:============================================================= \n";
			std::cout << "only recv : " << t << std::endl;
			return;
		}
		//  show message
		std::cout << "recv:============================================================= \n";
		//  print recv details on console
		std::cout << "head content: ";
		for (int i = 0; i < 12; i++)
		{
			std::cout << (int)((buffer.get() + i)[0]) << " ";
		}
		std::cout << "\n ";
		//  read message
		memcpy(&len, buffer.get(), sizeof len);
		len = ntohl(len);
		if (!len)
		{
			std::cout << "len is zero ! \n";
			continue;
		}
		else if (len >= BUFFER_SIZE)
		{
			//  TODO  long message problem
			std::cout << "message is too long! \n";
			continue;
		}

		int tmp = server.Read(fd, buffer.get() + HEAD_SIZE, len);
		if (tmp < len)
		{
			std::cout << "len in head is longer than real len \n";
			continue;
		}
		
		//  show message
		std::cout << "len: " << len << "\n\njson :\n";
		for (int i = 0; i < len; i++)
		{
			std::cout << (buffer.get() + i + 12)[0];
		}
		std::cout << std::endl;

		tp.commit(DoTask, buffer, fd);
	}
	return;
}

void DoTask(std::shared_ptr<char> &buffer, int fd)
{
	Json::Reader jreader;
	Json::Value jroot;



	if (jreader.parse(buffer.get() + HEAD_SIZE, jroot))
	{
		if (jroot["act"].isNull())
		{
			std::cout << " act is Null !\n";
			return;
		}
		switch (jroot["act"].asInt())
		{
		case Act::ACT_SIGNUP:
			Signup(jroot, fd);
			break;
		case Act::ACT_SIGNIN:
			Signin(jroot, fd);
			break;
		default:
			break;
		}

	}
	else
	{
		std::cout << server.GetIP(fd) << " send a invaid format !" << std::endl;
	}

	// std::cout << "send: \n len: " << server.Write(event.data.fd, buffer.get(), len + 12) << std::endl;
}

void Signup(Json::Value& jroot, int fd) 
{
	std::cout << "\ndo sign up ================================================================================== \n";
	Json::FastWriter jwriter;
	Json::Value rejroot;
	zwdbc::MysqlQuery myq;
	std::string sqlstr;
	std::string account, name, password;

	rejroot["act"] = Act::ACT_ACKSIGNUP;
	if (jroot["account"].isNull())
	{
		std::cout << "account is null \n";
		return;
	}
	account = jroot["account"].asString();

	sqlstr = "select uaccount from tUser where uaccount= '" + account + "'";
	myq = cp.query(sqlstr);
	if (!myq.getState()) return;

	if (myq.rowNum())
	{
		std::cerr << "had a same account !" << std::endl;
		rejroot["state"] = false;
	}
	else
	{
		if (jroot["name"].isNull() || jroot["password"].isNull())
		{
			std::cout << "name / password is null \n";
			return;
		}
		name = jroot["name"].asString();
		password = jroot["password"].asString();
		sqlstr = "insert into tUser (uaccount, upassword, uname) values ('"+account +"', '"+password+"', '"+name+"');";
		std::cout << "sql : " << sqlstr << std::endl;
		myq = cp.query(sqlstr);
		if (!myq.getState()) return;
		rejroot["state"] = true;
	}

	sendMessage(fd, jwriter.write(rejroot));
}

void Signin(Json::Value& jroot, int fd)
{
	std::cout << "\ndo sign in ================================================================================== \n";
	Json::FastWriter jwriter;
	Json::Reader jreader;
	Json::Value rejroot;
	zwdbc::MysqlQuery myq;
	std::string sqlstr;
	std::string account, password;
	rejroot["act"] = Act::ACT_ACKSIGNIN;
	
	if (jroot["account"].isNull() || jroot["password"].isNull())
	{
		std::cout << "account / password is Null\n";
		return;
	}
	account = jroot["account"].asString();
	password = jroot["password"].asString();

	sqlstr = "select upassword, uname, friList, id from tUser where uaccount='" + account + "'";
	myq = cp.query(sqlstr);
	if (!myq.getState()) return;

	myq.nextline();
	if (!myq.rowNum() || !isEqual(myq.getRow()[0], password.data()))
	{
		rejroot["state"] = false;
		std::cout << "account / password is Incorrect\n";
	}
	else
	{
		std::cout << "sign in success\n";
		rejroot["state"] = true;
		rejroot["name"] = std::string(myq.getRow()[1]);
		rejroot["id"] = atoi(myq.getRow()[3]);
		Json::Value friList, frikvList;
		if (!jreader.parse(myq.getRow()[2], friList))
		{
			std::cerr << "jreader parse failed !\n";
			return;
		}
		sqlstr = "select id, uname from tUser where id in (";
		for (int i = 0; i < friList.size(); i++)
		{
			sqlstr += friList[i].asString();
			if (i < friList.size() - 1) sqlstr += ",";
			else sqlstr += ")";
		}
		myq = cp.query(sqlstr);
		if (!myq.getState()) return;

		while (myq.nextline())
			frikvList[myq.getRow()[0]] = myq.getRow()[1];
		rejroot["friList"] = frikvList;

		int id = rejroot["id"].asInt();
		if (getsock.count(id)) DelConnect(getsock[id]);
		getid[fd] = id;
		getsock[id] = fd;
	}
	sendMessage(fd, jwriter.write(rejroot));
}

void Signout(int fd)
{
	Json::FastWriter jwriter;
	Json::Value rejroot;

	getsock.erase(getid[fd]);
	getid.erase(fd);
	std::cout << "signout : " << fd << std::endl;
	rejroot["act"] = Act::ACT_SIGNOUT;
	sendMessage(fd, jwriter.write(rejroot));
}


