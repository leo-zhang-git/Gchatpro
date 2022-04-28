// Gchatpro.cpp: 定义应用程序的入口点。

#include <iostream>
#include <memory>
#include <unordered_map>
#include <fstream>

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
	ACT_ADDFA = 5,
	ACT_ADDFB = 6,
	ACT_REMOVEF = 7,
	ACT_ACKADDFA = 8,
	ACT_CHATR = 9,
	ACT_GETLOG = 10,
	ACT_GETNAME = 11,
	ACT_ONLINECNT = 12,
	ACT_FRISTATE = 13,
	ACT_GETMEMBER = 14,
	ACT_ADDFAID = 15,
	ACT_GETSTATUS = 16,
	ACT_ADDRA = 17,
	ACT_ADDRB = 18,
	ACT_ACKADDRA = 19,
	ACT_GETRNAME = 20,
	ACT_CREATER = 21,
	ACT_REMOVEM = 22,
	ACT_DELROOM = 23,
};
enum UserState
{
	OFFLINE = 0,
	ONLINE = 1,
};

std::threadpool tp;

//  mysql connect
//const char* host = "0.0.0.0";
//const char* user = "root";
//const char* pwd = "zw0727@qq.cn";
//const char* dbname = "Gchatpro";
//unsigned int dbport = 3306;
//zwdbc::Connectpool cp{ host, user, pwd, dbname, dbport };
zwdbc::Connectpool cp;

std::mutex _outlock, _createroom;
std::unordered_map<int, std::mutex> fdreadlock;
std::unordered_map<int, std::mutex> fdwritelock;

TcpServer server;
epoll_event ev, events[MAX_EVENTS];
int epollfd;

//  get sockfd by id or get id by sockfd
//  if a new sockfd connect but not sigin in, it's getid[fd] will get -1
std::unordered_map<int, int> getsock;
std::unordered_map<int, int> getid;
std::atomic_int online_cnt{0};

void AddConnect(epoll_event &event);
void DelConnect(int fd);
void DealInput(int fd);
void DoTask(Json::Value& jroot, int fd);
void SetOnlineCnt();
void NotifyFriState(int target, int who, UserState state);
void Signup(Json::Value& jroot, int fd);
void Signin(Json::Value& jroot, int fd);
void Signout(int fd);
void ChatToOne(Json::Value& jroot, int sender);
void AddFriendA(Json::Value& jroot, int fd);
void AddFriendAbyID(Json::Value& jroot, int fd);
void AddFriendB(Json::Value& jroot, int fd);
bool AddFriend(int a, int b);
void RemoveFriend(Json::Value& jroot, int fd);
void AremoveB(int auid, int buid);
void AddRoomA(Json::Value& jroot, int fd);
void AddRoomB(Json::Value& jroot, int fd);
void ChatToRoom(Json::Value& jroot, int sender);
void SendChatLog(std::string timestamp, int rid, int fd);
void SendName(Json::Value& jroot, int fd);
void SendRName(Json::Value& jroot, int fd);
void GetMember(Json::Value& jroot, int fd);
void GetStatus(Json::Value& jroot, int fd);
void CreateRoom(Json::Value& jroot, int fd);
void DelRoom(Json::Value& jroot, int fd);
void RemoveMember(Json::Value& jroot, int fd);


std::string JsonToString(const Json::Value& root)
{
	static Json::Value def = []() {
		Json::Value def;
		Json::StreamWriterBuilder::setDefaults(&def);
		def["emitUTF8"] = true;
		return def;
	}();

	std::ostringstream stream;
	Json::StreamWriterBuilder stream_builder;
	stream_builder.settings_ = def;//Config emitUTF8
	std::unique_ptr<Json::StreamWriter> writer(stream_builder.newStreamWriter());
	writer->write(root, &stream);
	return stream.str();
}

//  find a value in a json array, return index of the value
//  if it is not in array return -1, return -2 when array is empty
template<typename T>
int findJsonArray(const Json::Value& array, T a)
{
	if (!array.isArray()) return -2;
	for (int i = 0; i < array.size(); i++)
	{
		if (array[i] == Json::Value(a)) return i;
	}
	return -1;
}

// 只会给记录了id的fd发送消息，（即已经发送了登录json并登录成功的用户）
void sendMessage(int fd, const std::string& s)
{
	if (!getid.count(fd)) return;
	std::cout << "send ++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n" << "fd : " << fd << "json : \n" << s << std::endl;
	u_int32_t len = s.size();
	std::unique_ptr<char> buffer(new char[BUFFER_SIZE]);
	memset(buffer.get(), 0, HEAD_SIZE);
	memcpy(buffer.get() + HEAD_SIZE, s.c_str(), len);
	if (len > BUFFER_SIZE)
	{
		std::cerr << "send len is too long !\n";
		return;
	}
	len = htonl(len);
	memcpy(buffer.get(), &len, 4);

	std::unique_lock<std::mutex> outlock{ _outlock };
	fdwritelock[fd];
	std::unique_lock<std::mutex> fdwrite{ fdwritelock[fd] };
	outlock.unlock();
	if (server.Write(fd, buffer.get(), s.size() + HEAD_SIZE) <= 0)
		DelConnect(fd);

}

std::string& replaceSinglequote(std::string& s)
{
	for (char& i : s)
		if (i == '\'') i = '`';
	return s;
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
	Json::Value conf;
	Json::Reader jreader;

	std::ifstream in("../conf.json");
	if (!in)
	{
		std::cout << "open conf.json error !" << std::endl;
		exit(-1);
	}
	std::ostringstream tmp;
	tmp << in.rdbuf();
	std::string str = tmp.str();
	
	if (!jreader.parse(str, conf))
	{
		std::cout << "conf.json format error!" << std::endl;
		exit(-1);
	}
	

	//  init server
	u_int16_t listenport = conf["tcpport"].asInt();
	if (!server.InitServer(listenport)) 
	{
		std::cout << "server init failed ! " << std::endl;
		exit(-1);
	}
	else
	{
		std::cout << "server init complete ! listenfd: " << server.listenfd << std::endl;
	}


	//  connect database
	const char* host = conf["dbhost"].asCString();
	const char* user = conf["dbuser"].asCString();
	const char* pwd = conf["dbpassword"].asCString();
	const char* dbname = conf["dbname"].asCString();
	unsigned int dbport = conf["dbport"].asInt();
	cp.Init(host, user, pwd, dbname, dbport);

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
				//  DealInput(events[i].data.fd);
				fdreadlock[events[i].data.fd];
				tp.commit(DealInput, int(events[i].data.fd));
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
	std::cout << "start  close socket : " << fd << std::endl;
	if (getid[fd] == -1) getid.erase(fd);
	else Signout(fd);
	fdreadlock.erase(fd);
	fdwritelock.erase(fd);
	epoll_event evt;
	evt.data.fd = fd;
	epoll_ctl(epollfd, EPOLL_CTL_DEL, evt.data.fd, &evt);
	std::cout << "over  close socket : " << fd << std::endl;
	close(evt.data.fd);
}

void DealInput(int fd) 
{
	std::unique_lock<std::mutex> lock{ fdreadlock[fd] };
	std::shared_ptr<char> buffer(new char[BUFFER_SIZE]);

	u_int32_t len = 0;

	while (true) 
	{
		// read buffer is empty
		int t = 0;
		if ( (t = server.Read(fd, buffer.get(), HEAD_SIZE, 0, 2)) < 0)
		{
			//  recv head over time
			return;
		}
		else if (t == 0)
		{
			std::cout << "close event !!!\n";
			DelConnect(fd);
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
		if (!tmp)
		{
			std::cout << "close event !!!\n";
			DelConnect(fd);
		}
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
		Json::Reader jreader;
		Json::Value jroot;
		if (!jreader.parse(buffer.get() + HEAD_SIZE, jroot)) 
		{
			std::cerr << "jreader parse failed !\n";
			std::cout << server.GetIP(fd) << " send a invaid format !" << std::endl;
			return;
		}
		tp.commit(DoTask, jroot, fd);
	}
	return;
}

void DoTask(Json::Value &jroot, int fd)
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
	case Act::ACT_CHATF:
		if (getid[fd] == -1 )
		{
			std::cerr << "没有登录，不允许聊天！";
			return;
		}
		ChatToOne(jroot, getid[fd]);
		break;
	case Act::ACT_ADDFA:
		AddFriendA(jroot, fd);
		break;
	case Act::ACT_ADDFAID:
		AddFriendAbyID(jroot, fd);
		break;
	case Act::ACT_ADDFB:
		AddFriendB(jroot, fd);
		break;
	case Act::ACT_REMOVEF:
		RemoveFriend(jroot, fd);
		break;
	case Act::ACT_CHATR:
		if (getid[fd] == -1)
		{
			std::cerr << "没有登录，不允许群聊！";
			return;
		}
		ChatToRoom(jroot, getid[fd]);
		break;
	case Act::ACT_GETLOG:
		SendChatLog(jroot["starttime"].asString(), jroot["id"].asInt(), fd);
		break;
	case Act::ACT_GETNAME:
		SendName(jroot, fd);
		break;
	case Act::ACT_GETRNAME:
		SendRName(jroot, fd);
		break;
	case Act::ACT_GETMEMBER:
		GetMember(jroot, fd);
		break;
	case Act::ACT_GETSTATUS:
		GetStatus(jroot, fd);
		break;
	case Act::ACT_ADDRA:
		AddRoomA(jroot, fd);
		break;
	case Act::ACT_ADDRB:
		AddRoomB(jroot, fd);
		break;
	case Act::ACT_CREATER:
		CreateRoom(jroot, fd);
		break;
	case Act::ACT_DELROOM:
		DelRoom(jroot, fd);
		break;
	case Act::ACT_REMOVEM:
		RemoveMember(jroot, fd);
		break;
	default:
		break;
	}
	// std::cout << "send: \n len: " << server.Write(event.data.fd, buffer.get(), len + 12) << std::endl;
}
void SetOnlineCnt() 
{
	std::cout << "update online cnt \n";
	Json::Value jroot;

	jroot["act"] = Act::ACT_ONLINECNT;
	jroot["cnt"] = (int)online_cnt;
	for (auto i : getsock)
	{
		sendMessage(i.second, JsonToString(jroot));
	}
}

/// <summary>
/// 通知id为target的用户，id为who的用户当前的状态state
/// </summary>
/// <param name="target"></param>
/// <param name="who"></param>
/// <param name="state"></param>
void NotifyFriState(int target, int who, UserState state)
{
	if (getsock.count(target) == 0) return;
	Json::Value jroot;
	jroot["act"] = Act::ACT_FRISTATE;
	jroot["id"] = who;
	jroot["state"] = state;
	sendMessage(getsock[target], JsonToString(jroot));
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

	sqlstr = "select uaccount from tUser where uaccount= '" + replaceSinglequote(account) + "'";
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
		sqlstr = "insert into tUser (uaccount, upassword, uname) values ('"
			+replaceSinglequote(account) +"', '"+replaceSinglequote(password)+"', '"+replaceSinglequote(name)+"');";
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
	Json::Value rejroot, friList;
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

	sqlstr = "select upassword, uname, friList, id, friReq, roomReq, offline_message, roomList from tUser where uaccount='" + replaceSinglequote(account) + "'";
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

		//  get friend list
		Json::Value  frikvList;
		if (myq.getRow()[2] != nullptr)
		{
			if (!jreader.parse(myq.getRow()[2], friList))
			{
				std::cerr << "jreader parse failed !\n";
				return;
			}
			if (friList.size() > 0)
			{
				sqlstr = "select id, uname from tUser where id in (";
				for (int i = 0; i < friList.size(); i++)
				{
					NotifyFriState(friList[i].asInt(), rejroot["id"].asInt(), UserState::ONLINE);
					sqlstr += friList[i].asString();
					if (i < friList.size() - 1) sqlstr += ",";
					else sqlstr += ")";
				}
				zwdbc::MysqlQuery tquery = cp.query(sqlstr);
				if (!tquery.getState()) return;
				while (tquery.nextline())
					frikvList[tquery.getRow()[0]] = tquery.getRow()[1];
			}
		}
		rejroot["friList"] = frikvList;

		//  get room list
		Json::Value roomList, roomkvList;
		if (myq.getRow()[7] != nullptr)
		{
			if (!jreader.parse(myq.getRow()[7], roomList))
			{
				std::cerr << "jreader parse failed !\n";
				return;
			}
			if (roomList.size() > 0)
			{
				sqlstr = "select id, name from tRoom where id in (";
				for (int i = 0; i < roomList.size(); i++)
				{
					sqlstr += roomList[i].asString();
					if (i < roomList.size() - 1) sqlstr += ",";
					else sqlstr += ")";
				}
				zwdbc::MysqlQuery tquery = cp.query(sqlstr);
				if (!tquery.getState()) return;

				roomList.clear();
				while (tquery.nextline())
				{
					roomkvList[tquery.getRow()[0]] = tquery.getRow()[1];
					roomList.append(atoi(tquery.getRow()[0]));
				}
				//  删除群列表中被解散的群
				sqlstr = "update tUser set roomList='" + JsonToString(roomList) + "' where id=" + rejroot["id"].asString();
				tquery = cp.query(sqlstr);
				if (!tquery.getState()) return;
			}
		}
		rejroot["roomList"] = roomkvList;

		//  断开之前登录的相同账号的连接
		int id = rejroot["id"].asInt();
		if (getsock.count(id) && getsock[id] != fd) DelConnect(getsock[id]);
		getid[fd] = id;
		getsock[id] = fd;
	}
	sendMessage(fd, jwriter.write(rejroot));

	//  send offline message
	if (rejroot["state"].asBool())
	{
		online_cnt++;
		SetOnlineCnt();

		for (int i = 0; i < friList.size(); i++)
		{
			if(getsock.count(friList[i].asInt()))
				NotifyFriState(rejroot["id"].asInt(), friList[i].asInt(), UserState::ONLINE);
		}

		//  离线消息所需参数赋值
		Json::Value friReqs, onefriReq, roomReqs, oneRoomReq;
		if (myq.getRow()[4] != nullptr) jreader.parse(myq.getRow()[4], friReqs);
		if (myq.getRow()[5] != nullptr) jreader.parse(myq.getRow()[5], roomReqs);
		Json::Value chatlog, message;
		if (myq.getRow()[6] != nullptr) jreader.parse(myq.getRow()[6], chatlog);

		//  send offline friendship request
		onefriReq["act"] = Act::ACT_ADDFA;
		for (auto &i : friReqs.getMemberNames())
		{
			onefriReq["id"] = atoi(i.c_str());
			onefriReq["name"] = friReqs[i];
			sendMessage(fd, JsonToString(onefriReq));
		}

		//  send offline add room request
		oneRoomReq["act"] = Act::ACT_ADDRA;
		for (auto& i : roomReqs.getMemberNames())
		{
			oneRoomReq["rid"] = atoi(i.c_str());
			for (int j = 0; j < roomReqs[i].size(); j++)
			{
				oneRoomReq["uid"] = roomReqs[i][j].asInt();
				sendMessage(fd, JsonToString(oneRoomReq));
			}
		}

		//  send offline chat message
		std::cerr << "send offline message !\n";
		message["act"] = Act::ACT_CHATF;
		for (std::string& s : chatlog.getMemberNames())
		{
			message["id"] = getid[fd];
			for (int i = 0; i < chatlog[s].size(); i++)
			{
				message["text"] = chatlog[s][i];
				std::cout << "chat log :\n" << JsonToString(message) << std::endl;
				ChatToOne(message, atoi(s.c_str()));
			}
		}
		
		//  delete offline message, update the lastlogin time
		sqlstr = "update tUser set friReq=null, roomReq=null, offline_message=NULL, lastlogin_time=now()  where id=" + rejroot["id"].asString();
		myq = cp.query(sqlstr);
		if (!myq.getState()) return;

	}
}

void Signout(int fd)
{
	if (getid[fd] == -1) return;
	Json::Reader jreader;
	Json::Value friList;
	zwdbc::MysqlQuery myq;
	std::string sqlstr;
	int id = getid[fd];
	sqlstr = "select friList from tUser where id = " + std::to_string(id);
	getsock.erase(id);
	getid.erase(fd);
	std::cout << "signout fd : " << fd << "  id :" << id << std::endl;
	
	online_cnt --;
	SetOnlineCnt();
	myq = cp.query(sqlstr);
	if (!myq.nextline()) return;
	if (myq.getRow()[0] != nullptr) jreader.parse(myq.getRow()[0], friList);
	for (int i = 0; i < friList.size(); i++)
	{
		NotifyFriState(friList[i].asInt(), id, UserState::OFFLINE);
	}
	// rejroot["act"] = Act::ACT_SIGNOUT;
	// sendMessage(fd, jwriter.write(rejroot));
}

void ChatToOne(Json::Value& jroot, int sender)
{
	std::cout << "\ndo ChatToOne ================================================================================== \n";
	int receiver = jroot["id"].asInt();
	Json::FastWriter jwriter;
	if (getsock.count(receiver))
	{
		int fd = getsock[receiver];
		jroot["id"] = sender;

		std::cout << "receiver: " << getid[fd] << "\ntext:" << jroot["text"].asString() << std::endl;
		sendMessage(fd, jwriter.write(jroot));
		jroot["id"] = receiver;
	}
	else
	{
		Json::Reader jreader;
		Json::Value chatlog, arr;
		zwdbc::MysqlQuery myq;
		std::string sqlstr;

		sqlstr = "select offline_message, friList from tUser where id=" + std::to_string(receiver);
		myq = cp.query(sqlstr);
		if (!myq.getState()) return;
		if (myq.rowNum() == 0)
		{
			std::cerr << "账号不存在 ！\n";
			// TODO after del friend
			return;
		}
		myq.nextline();
		if (myq.getRow()[1] != nullptr) jreader.parse(myq.getRow()[1], arr);
		if (findJsonArray(arr, sender) < 0)
		{
			std::cerr << "目标不是好友！\n";
			// TODO after del friend
			return;
		}
		if (myq.getRow()[0] != nullptr)
		{
			if (!jreader.parse(myq.getRow()[0], chatlog))
			{
				std::cerr << "jreader parse failed !\n";
				return;
			}
		}
		
		chatlog[std::to_string(sender)].append(jroot["text"]);
		std::string ts = JsonToString(chatlog);
		sqlstr = "update tUser set offline_message= '" + replaceSinglequote(ts) + "' where id=" + std::to_string(receiver);
		myq = cp.query(sqlstr);
		if (!myq.getState()) return;
		std::cout << "target is offline, save the message into database\n";
	}
}
void AddFriendA(Json::Value& jroot, int fd) 
{
	std::cout << "\ndo AddFirendA ================================================================================== \n";
	if (getid[fd] == -1)
	{
		std::cout << "未登录，不能添加好友！\n";
		return;
	}
	std::string account = jroot["account"].asString();
	std::string sqlstr;
	zwdbc::MysqlQuery myq;
	Json::Reader jreader;
	Json::Value rejroot, friList, ackjroot;
	int receiver;

	sqlstr = "select id, friReq, friList from tUser where uaccount='" + replaceSinglequote(account) + "'";
	myq = cp.query(sqlstr);

	//  验证请求信息并返回状态给发起者
	ackjroot["act"] = Act::ACT_ACKADDFA;
	if (!myq.getState()) return;
	if (!myq.nextline()) 
	{
		"账号不存在 ! ";
		ackjroot["state"] = -1;
		sendMessage(fd, JsonToString(ackjroot));
		return;
	}
	if (atoi(myq.getRow()[0]) == getid[fd])
	{
		std::cout << "不能添加自己为好友！\n";
		ackjroot["state"] = -2;
		sendMessage(fd, JsonToString(ackjroot));
		return;
	}
	if (myq.getRow()[2] != nullptr) jreader.parse(myq.getRow()[2], friList);
	if (findJsonArray(friList, getid[fd]) >= 0)
	{
		std::cout << "已经是好友了！\n";
		ackjroot["state"] = -3;
		sendMessage(fd, JsonToString(ackjroot));
		return;
	}
	ackjroot["state"] = 0;
	sendMessage(fd, JsonToString(ackjroot));

	receiver = atoi(myq.getRow()[0]);
	if (getsock.count(receiver))
	{
		sqlstr = "select uname from tUser where id=" + std::to_string(getid[fd]);
		myq = cp.query(sqlstr);
		if (!myq.getState()) return;
		myq.nextline();
		
		rejroot["act"] = Act::ACT_ADDFA;
		rejroot["id"] = getid[fd];
		rejroot["name"] = myq.getRow()[0];
		sendMessage(getsock[receiver], JsonToString(rejroot));
	}
	else
	{
		if (myq.getRow()[1] != nullptr) jreader.parse(myq.getRow()[1], rejroot);
		sqlstr = "select uname from tUser where id=" + std::to_string(getid[fd]);
		myq = cp.query(sqlstr);
		if (!myq.getState()) return;
		myq.nextline();
		rejroot[std::to_string(getid[fd])] = myq.getRow()[0];
		sqlstr = "update tUser set friReq='" + JsonToString(rejroot) + "' where id=" + std::to_string(receiver);
		cp.query(sqlstr);
		if (!myq.getState()) return;
	}
}
void AddFriendAbyID(Json::Value& jroot, int fd) 
{
	std::cout << "\ndo AddFirendAbyID ================================================================================== \n";
	if (getid[fd] == -1)
	{
		std::cout << "未登录，不能添加好友！\n";
		return;
	}
	std::string sqlstr;
	zwdbc::MysqlQuery myq;
	Json::Reader jreader;
	Json::Value rejroot, friList, ackjroot;
	int receiver;

	sqlstr = "select id, friReq, friList from tUser where id=" + jroot["id"].asString();
	myq = cp.query(sqlstr);

	//  验证请求信息并返回状态给发起者
	ackjroot["act"] = Act::ACT_ACKADDFA;
	if (!myq.getState()) return;
	if (!myq.nextline())
	{
		"账号不存在 ! ";
		ackjroot["state"] = -1;
		sendMessage(fd, JsonToString(ackjroot));
		return;
	}
	if (atoi(myq.getRow()[0]) == getid[fd])
	{
		std::cout << "不能添加自己为好友！\n";
		ackjroot["state"] = -2;
		sendMessage(fd, JsonToString(ackjroot));
		return;
	}
	if (myq.getRow()[2] != nullptr) jreader.parse(myq.getRow()[2], friList);
	if (findJsonArray(friList, getid[fd]) >= 0)
	{
		std::cout << "已经是好友了！\n";
		ackjroot["state"] = -3;
		sendMessage(fd, JsonToString(ackjroot));
		return;
	}
	ackjroot["state"] = 0;
	sendMessage(fd, JsonToString(ackjroot));

	receiver = atoi(myq.getRow()[0]);
	if (getsock.count(receiver))
	{
		sqlstr = "select uname from tUser where id=" + std::to_string(getid[fd]);
		myq = cp.query(sqlstr);
		if (!myq.getState()) return;
		myq.nextline();

		rejroot["act"] = Act::ACT_ADDFA;
		rejroot["id"] = getid[fd];
		rejroot["name"] = myq.getRow()[0];
		sendMessage(getsock[receiver], JsonToString(rejroot));
	}
	else
	{
		if (myq.getRow()[1] != nullptr) jreader.parse(myq.getRow()[1], rejroot);
		sqlstr = "select uname from tUser where id=" + std::to_string(getid[fd]);
		myq = cp.query(sqlstr);
		if (!myq.getState()) return;
		myq.nextline();
		rejroot[std::to_string(getid[fd])] = myq.getRow()[0];
		sqlstr = "update tUser set friReq='" + JsonToString(rejroot) + "' where id=" + std::to_string(receiver);
		cp.query(sqlstr);
		if (!myq.getState()) return;
	}
}
void AddFriendB(Json::Value& jroot, int fd)
{
	std::cout << "\ndo AddFirendB ================================================================================== \n";
	if (getid[fd] == -1)
	{
		std::cout << "未登录，不能添加好友！\n";
		return;
	}
	int target = jroot["id"].asInt();
	if (!AddFriend(getid[fd], target) || !AddFriend(target, getid[fd])) return;

	Json::Value greeting;
	greeting["act"] = Act::ACT_CHATF;
	greeting["id"] = target;
	greeting["text"] = "我们已经是好友啦！";

	if (!getsock.count(target))
	{
		ChatToOne(greeting, getid[fd]);
		return;
	}
	NotifyFriState(getid[fd], target, ONLINE);
	std::string sqlstr = "select uname from tUser where id=" + std::to_string(getid[fd]);
	zwdbc::MysqlQuery myq = cp.query(sqlstr);
	if (!myq.getState()) return;
	myq.nextline();

	jroot["id"] = getid[fd];
	jroot["name"] = myq.getRow()[0];
	sendMessage(getsock[target], JsonToString(jroot));
	NotifyFriState(target, getid[fd], ONLINE);
	ChatToOne(greeting, getid[fd]);
}
bool AddFriend(int a, int b)
{
	std::cout << a << " add a friend " << b << "\n";
	std::string sqlstr;
	zwdbc::MysqlQuery myq;
	Json::Value rejroot, friList;
	Json::Reader jreader;

	sqlstr = "select friList from tUser where id=" + std::to_string(a);
	myq = cp.query(sqlstr);
	if (!myq.getState()) return false;
	if (!myq.nextline())
	{
		"账号不存在 ! ";
		return false;
	}
	if (myq.getRow()[0] != nullptr) jreader.parse(myq.getRow()[0], friList);
	if (friList.size() == 0 || findJsonArray(friList, b) == -1) friList.append(b);
	else
	{
		std::cout << "已经是好友了 ！\n";
		return false;
	}
	sqlstr = "update tUser set friList='" + JsonToString(friList) + "' where id=" + std::to_string(a);
	cp.query(sqlstr);
	if (!myq.getState()) return false;
	return true;
}
void RemoveFriend(Json::Value& jroot, int fd)
{
	std::cout << "\ndo RemoveFriend ================================================================================== \n";
	if (getid[fd] == -1)
	{
		std::cout << "未登录，删好友！\n";
		return;
	}
	Json::Value friList;
	Json::Reader jreader;
	zwdbc::MysqlQuery myq;
	std::string sqlstr;

	int auid = getid[fd], buid = jroot["id"].asInt();

	AremoveB(auid, buid);
	AremoveB(buid, auid);
}
void AremoveB(int auid, int buid)
{
	Json::Value jroot, friList;
	Json::Reader jreader;
	zwdbc::MysqlQuery myq;
	std::string sqlstr;
	int idx;

	jroot["act"] = Act::ACT_REMOVEF;
	jroot["id"] = buid;

	sqlstr = "select friList from tUser where id=" + std::to_string(auid);
	myq = cp.query(sqlstr);
	if (!myq.nextline()) return;
	if (myq.getRow()[0]) jreader.parse(myq.getRow()[0], friList);
	idx = findJsonArray(friList, buid);
	if (idx >= 0) friList.removeIndex(idx, nullptr);
	sqlstr = "update tUser set friList='" + JsonToString(friList) + "' where id=" + std::to_string(auid);
	myq = cp.query(sqlstr);
	if (!myq.getState()) return;
	if(getsock.count(auid))sendMessage(getsock[auid], JsonToString(jroot));
	
}
void AddRoomA(Json::Value& jroot, int fd)
{
	std::cout << "\ndo AddRoomA ================================================================================== \n";
	if (getid[fd] == -1)
	{
		std::cout << "未登录，不能添加群！\n";
		return;
	}
	std::string sqlstr;
	zwdbc::MysqlQuery myq;
	Json::Reader jreader;
	Json::Value rejroot, memberList, ackjroot;
	ackjroot["act"] = Act::ACT_ACKADDRA;
	sqlstr = "select members from tRoom where id=" + jroot["rid"].asString();
	myq = cp.query(sqlstr);
	if (!myq.getState()) return;
	if (!myq.nextline())
	{
		//  群不存在
		std::cout << "群不存在\n";
		ackjroot["state"] = -1;
		sendMessage(fd, JsonToString(ackjroot));
		return;
	}
	if (!jreader.parse(myq.getRow()[0], memberList))
	{
		std::cout << "json reader failed !\n";
		return;
	}
	if (findJsonArray(memberList, getid[fd]) >= 0)
	{
		//  已经在群里
		std::cout << "已经在群里\n";
		ackjroot["state"] = -2;
		sendMessage(fd, JsonToString(ackjroot));
		return;
	}
	ackjroot["state"] = 0;
	sendMessage(fd, JsonToString(ackjroot));
	
	sqlstr = "select uname from tUser where id=" + std::to_string(getid[fd]);
	myq = cp.query(sqlstr);
	if (!myq.nextline()) return;
	// 群主在线则发送请求
	if (getsock.count(memberList[0].asInt()) > 0)
	{
		rejroot["act"] = Act::ACT_ADDRA;
		rejroot["rid"] = jroot["rid"].asInt();
		rejroot["uid"] = getid[fd];
		sendMessage(getsock[memberList[0].asInt()], JsonToString(rejroot));
	}
	else
	{
		sqlstr = "select roomReq from tUser where id=" + memberList[0].asString();
		myq = cp.query(sqlstr);
		if (!myq.nextline()) return;

		if (myq.getRow()[0] != nullptr) jreader.parse(myq.getRow()[0], rejroot);
		if(findJsonArray(rejroot[jroot["rid"].asString()], getid[fd]) < 0) rejroot[jroot["rid"].asString()].append(getid[fd]);

		sqlstr = "update tUser set roomReq='" + JsonToString(rejroot) + "' where id=" + memberList[0].asString();
		cp.query(sqlstr);
		if (!myq.getState()) return;
	}
}
void AddRoomB(Json::Value& jroot, int fd)
{
	std::cout << "\ndo AddRoomB ================================================================================== \n";
	Json::Reader jreader;
	Json::Value memberList, roomList;
	zwdbc::MysqlQuery myq;
	std::string sqlstr;

	sqlstr = "select members from tRoom where id=" + jroot["rid"].asString();
	myq = cp.query(sqlstr);
	if (!myq.nextline()) return;
	jreader.parse(myq.getRow()[0], memberList);
	if (getid[fd] != memberList[0].asInt())
	{
		std::cout << "不是群主不能添加成员\n";
		return;
	}
	if (findJsonArray(memberList, jroot["uid"].asInt()) >= 0)
	{
		std::cout << "已经是群成员不必再添加\n";
		return;
	}
	memberList.append(jroot["uid"].asInt());
	sqlstr = "update tRoom set members='"+JsonToString(memberList)+"' where id=" + jroot["rid"].asString();
	myq = cp.query(sqlstr);
	if (!myq.getState()) return;

	sqlstr = "select roomList from tUser where id=" + jroot["uid"].asString();
	myq = cp.query(sqlstr);
	if (!myq.nextline()) return;
	if(myq.getRow()[0] != nullptr) jreader.parse(myq.getRow()[0], roomList);
	roomList.append(jroot["rid"].asInt());
	sqlstr = "update tUser set roomList='" + JsonToString(roomList) + "' where id=" + jroot["uid"].asString();
	myq = cp.query(sqlstr);
	if (!myq.getState()) return;

	for (int i = 0; i < memberList.size(); i++)
	{
		if (getsock.count(memberList[i].asInt()))
		{
			sendMessage(getsock[memberList[i].asInt()], JsonToString(jroot));
		}
	}

}
void ChatToRoom(Json::Value& jroot, int sender)
{
	std::cout << "\ndo ChatToRoom ================================================================================== \n";
	int rid = jroot["id"].asInt();
	zwdbc::MysqlQuery myq;
	std::string sqlstr;
	Json::Reader jreader;
	Json::Value memberList;

	if (jroot["id"].asInt() == 0)
	{
		std::cout << "send World Message \n";
		jroot["sender"].append(sender);
		jroot["sender"].append(jroot["name"]);
		jroot.removeMember("name");
		for (auto i : getsock)
		{
			sendMessage(i.second, JsonToString(jroot));
		}

		return;
	}

	sqlstr = "select members from tRoom where id=" + jroot["id"].asString();
	myq = cp.query(sqlstr);
	if (!myq.nextline())
	{
		std::cerr << "错误的群号！\n";
		return;
	}
	if(myq.getRow()[0] != nullptr) jreader.parse(myq.getRow()[0], memberList);
	if (findJsonArray(memberList, sender) < 0)
	{
		std::cerr << "发送者不在该群中！\n";
		// TODO after del member
		return;
	}

	jroot["sender"].append(sender);
	jroot["sender"].append(jroot["name"]);
	jroot.removeMember("name");

	std::string ts = jroot["text"].asString();
	sqlstr = "insert into roomlog" + jroot["id"].asString() + 
		" (sender, text) values ( "+std::to_string(sender)+", '"+replaceSinglequote(ts)+"')";
	myq = cp.query(sqlstr);
	if (!myq.getState()) return;
	sqlstr = "select now()";
	myq = cp.query(sqlstr);
	if (!myq.nextline()) return;
	jroot["sendtime"] = myq.getRow()[0];

	for (int i = 0; i < memberList.size(); i++)
	{
		int target = memberList[i].asInt();
		if (getsock.count(target) > 0)
		{
			sendMessage(getsock[target], JsonToString(jroot));
		}
	}
}
void SendChatLog(std::string timestamp, int rid, int fd)
{
	std::cout << "\ndo SendChatLog ================================================================================== \n";

	if (timestamp == "")
	{
		std::cout << "没有给出时间戳!\n";
		return;
	}

	Json::Value rejroot, message;
	zwdbc::MysqlQuery myq;
	std::string sqlstr;

	rejroot["act"] = Act::ACT_GETLOG;
	rejroot["id"] = rid;

	sqlstr = "select sender, text from roomlog" + std::to_string(rid) + " where sendtime> '" + timestamp+"'";
	myq = cp.query(sqlstr);
	if (!myq.getState()) return;
	if (myq.rowNum() == 0)
	{
		std::cout << "群" << rid << " 没有记录\n";
		return;
	}
	while (myq.nextline())
	{
		message.append(atoi(myq.getRow()[0]));
		message.append(myq.getRow()[1]);
		rejroot["chatlog"].append(message);
		message.clear();
	}

	myq = cp.query("select now()");
	myq.nextline();
	rejroot["time"] = myq.getRow()[0];

	sendMessage(fd, JsonToString(rejroot));
}

void SendName(Json::Value& jroot, int fd)
{
	zwdbc::MysqlQuery myq = cp.query("select uname from tUser where id=" + jroot["id"].asString());
	if (!myq.nextline()) return;
	jroot["name"] = myq.getRow()[0];
	sendMessage(fd, JsonToString(jroot));
}
void SendRName(Json::Value& jroot, int fd) 
{
	zwdbc::MysqlQuery myq = cp.query("select name from tRoom where id=" + jroot["id"].asString());
	if (!myq.nextline()) return;
	jroot["name"] = myq.getRow()[0];
	sendMessage(fd, JsonToString(jroot));
}
void GetMember(Json::Value& jroot, int fd)
{
	std::cout << "\ndo GetMember ================================================================================== \n";
	zwdbc::MysqlQuery myq;
	std::string sqlstr;
	Json::Value member;

	sqlstr = "select members from tRoom where id=" + jroot["id"].asString();
	myq = cp.query(sqlstr);
	if (!myq.nextline()) return;
	sqlstr = "select id, uname from tUser where id in (";
	std::string memberList = std::string(myq.getRow()[0]);
	memberList[0] = ' ';
	memberList[memberList.size() - 1] = ' ';

	//  取群主放第一位
	int pos = memberList.size();
	for (int i = 0; i < pos; i++)
	{
		if (memberList[i] == ',') {
			pos = i;
			break;
		}
	}

	member.append(atoi(memberList.substr(0, pos).c_str()));
	jroot["members"].append(member);

	sqlstr += memberList + ")";
	myq = cp.query(sqlstr);
	if (!myq.getState()) return;
	while (myq.nextline())
	{
		int id = atoi(myq.getRow()[0]);
		if (id == jroot["members"][0][0].asInt())
		{
			jroot["members"][0].append(myq.getRow()[1]);
			continue;
		}
		member.clear();
		
		member.append(id);
		member.append(myq.getRow()[1]);
		jroot["members"].append(member);
		
	}
	sendMessage(fd, JsonToString(jroot));
}

void GetStatus(Json::Value& jroot, int fd)
{
	std::cout << "\ndo GetStatus ================================================================================== \n";
	zwdbc::MysqlQuery myq;
	std::string sqlstr;

	sqlstr = "select uaccount, uname, email, creat_time, lastlogin_time from tUser where id =" + jroot["id"].asString();
	myq = cp.query(sqlstr);
	if (!myq.nextline()) return;
	jroot["account"] = myq.getRow()[0];
	jroot["name"] = myq.getRow()[1];
	if (myq.getRow()[2] != nullptr) jroot["email"] = myq.getRow()[2];
	else jroot["email"] = "";
	jroot["createtime"] = myq.getRow()[3];
	jroot["lastsignin"] = myq.getRow()[4];
	sendMessage(fd, JsonToString(jroot));
}
void CreateRoom(Json::Value& jroot, int fd)
{
	std::cout << "\ndo CreateRoom ================================================================================== \n";
	Json::Value roomList;
	Json::Reader jreader;
	zwdbc::MysqlQuery myq;
	std::string sqlstr;
	int rid, uid = getid[fd];
	std::string name = jroot["name"].asString();
	if (uid == -1)
	{
		std::cout << "未登录不能创群\n";
		return;
	}
	sqlstr = "SELECT Auto_increment FROM information_schema.`TABLES` WHERE Table_Schema='Gchatpro' AND table_name = 'tRoom'";
	{
		std::unique_lock<std::mutex> lock{ _createroom };
		myq = cp.query(sqlstr);
		if (!myq.nextline()) return;
		rid = atoi(myq.getRow()[0]);
		sqlstr = "insert into tRoom (name, members) values ('" + replaceSinglequote(name) + "', '["+std::to_string(uid)+"]')";
		myq = cp.query(sqlstr);
		if (!myq.getState()) return;
	}

	// 更新群主用户的群列表
	sqlstr = "select roomList from tUser where id=" + std::to_string(uid);
	myq = cp.query(sqlstr);
	if (!myq.nextline()) return;
	if (myq.getRow()[0]) jreader.parse(myq.getRow()[0], roomList);
	roomList.append(rid);
	sqlstr = "update tUser set roomList='"+JsonToString(roomList)+"' where id=" + std::to_string(uid);
	myq = cp.query(sqlstr);
	if (!myq.getState()) return;

	// 创建群消息记录表
	sqlstr = "CREATE  TABLE roomlog" + std::to_string(rid) + " (LIKE roomlog_origin)";
	myq = cp.query(sqlstr);
	if (!myq.getState()) return;

	jroot["id"] = rid;
	sendMessage(fd, JsonToString(jroot));
}
void DelRoom(Json::Value& jroot, int fd)
{
	std::cout << "\ndo DelRoom ================================================================================== \n";
	Json::Value memberList;
	Json::Reader jreader;
	zwdbc::MysqlQuery myq;
	std::string sqlstr;
	
	//  通知在线成员群解散了
	sqlstr = "select members from tRoom where id="+jroot["rid"].asString();
	myq = cp.query(sqlstr);
	if (!myq.nextline()) return;
	jreader.parse(myq.getRow()[0], memberList);

	if (getid[fd] != memberList[0].asInt())
	{
		std::cout << "不是群主，不能解散该群\n";
		return;
	}
	for (int i = 0; i < memberList.size(); i++)
		if (getsock.count(memberList[i].asInt()))
			sendMessage(getsock[memberList[i].asInt()], JsonToString(jroot));

	//  删除群和存储群聊天记录的表
	sqlstr = "delete from tRoom where id=" + jroot["rid"].asString();
	myq = cp.query(sqlstr);
	if (!myq.getState()) return;
	sqlstr = "drop table roomlog"+jroot["rid"].asString();
	myq = cp.query(sqlstr);
	if (!myq.getState()) return;
}
void RemoveMember(Json::Value& jroot, int fd)
{
	std::cout << "\ndo RemoveMember ================================================================================== \n";
	Json::Value memberList;
	Json::Reader jreader;
	zwdbc::MysqlQuery myq;
	std::string sqlstr;
	int rid = jroot["rid"].asInt(), uid = jroot["uid"].asInt();

	//  通知在线成员某个成员被删除
	sqlstr = "select members from tRoom where id=" + jroot["rid"].asString();
	myq = cp.query(sqlstr);
	if (!myq.nextline()) return;
	jreader.parse(myq.getRow()[0], memberList);

	if (getid[fd] != memberList[0].asInt())
	{
		std::cout << "不是群主，不能删除成员\n";
		return;
	}
	else if (uid == getid[fd])
	{
		std::cout << "不能删除群主\n";
		return;
	}
	int targetidx = -1;
	for (int i = 0; i < memberList.size(); i++)
	{
		if (getsock.count(memberList[i].asInt()))
		{
			sendMessage(getsock[memberList[i].asInt()], JsonToString(jroot));
		}
		if (memberList[i].asInt() == uid)
		{
			targetidx = i;
		}
	}
	if(targetidx >= 0)memberList.removeIndex(targetidx, nullptr);

	//  把数据库中群成员更新，并更新被删除的成员的群列表
	sqlstr = "update tRoom set members ='"+JsonToString(memberList)+"' where id=" + jroot["rid"].asString();
	myq = cp.query(sqlstr);
	if (!myq.getState()) return;

	sqlstr = "select roomList from tUser where id=" + jroot["uid"].asString();
	myq = cp.query(sqlstr);
	if (!myq.nextline()) return;
	if (myq.getRow()[0]) jreader.parse(myq.getRow()[0], memberList);
	int idx = findJsonArray(memberList, rid);
	if (idx >= 0) memberList.removeIndex(idx, nullptr);
	sqlstr = "update tUser set roomList ='" + JsonToString(memberList) + "' where id=" + jroot["uid"].asString();
	myq = cp.query(sqlstr);
	if (!myq.getState()) return;

}