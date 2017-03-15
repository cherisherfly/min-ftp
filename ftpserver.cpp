#include<iostream>
#include<cstdio>
#include<winsock2.h>
//#include<string>
using namespace std;
#define SERVER_PORT 5021//服务器侦听端口号
#define DATA_PORT 5020//服务器侦听数据连接套接字
#define RESPONSE_SIZE 256//发送数据大小
#define CMD_PACKET_SIZE 256//命令报文大小为256
#define RSPNS_TEXT_SIZE 256//回复报文缓存大小
//用枚举类型保存命令
typedef enum {
	//ls,pwd,cd,get,put,quit
	LS,PWD,CD,GET,PUT,QUIT
}CmdID;
//命令报文，从客户端发往服务器
typedef struct _cmdPacket{
	CmdID cmdid;
	char param[CMD_PACKET_SIZE];
}CmdPacket;
//用枚举类型保存回复的类型
typedef enum {
	OK,ERR
}RspnsID;
//回复报文，从客户端发往服务器
typedef struct _RspnsPacket {
	RspnsID rspnsid;
	char text[RSPNS_TEXT_SIZE];
}RspnsPacket;

struct threadData {
	SOCKET sock;
	sockaddr_in clientaddr;
};
bool initFTP(SOCKET *pListenSock) {
	WORD wVersionRequested;
	WSADATA wsaData;
	int error;
	SOCKET listen_sock;
	wVersionRequested = MAKEWORD(2, 2);
	//启动
	error = WSAStartup(wVersionRequested, &wsaData);
	if (error != 0) {
		cout << "Winsock初始化发生错误!" << endl;
		return false;
	}
	//判断版本
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		WSACleanup();
		cout << "Winsock版本无效" << endl;
		return false;
	}
	//使用TCP协议创建socket
	listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_sock == INVALID_SOCKET) {
		WSACleanup();
		cout << "创建Socket失败" << endl;
		return false;
	}
	SOCKADDR_IN tcpaddr;
	tcpaddr.sin_family = AF_INET;
	tcpaddr.sin_port = htons(SERVER_PORT);
	tcpaddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	//绑定套接字
	error = bind(listen_sock, (SOCKADDR*)&tcpaddr, sizeof(tcpaddr));
	if (error != 0) {
		error = WSAGetLastError();
		WSACleanup();
		cout << "socket绑定时发生错误" << endl;
		return false;
	}
	//第二个参数是等待连接队列的最大长度,超过最大长度的部分请求会被拒绝
	error = listen(listen_sock, 3);
	if (error != 0) {
		WSACleanup();
		cout << "socket监听时发生错误" << endl;
		return false;
	}
	//返回值
	*pListenSock = listen_sock;
	return true;

}
//服务器端发送回复报文
int sendResponse(SOCKET sock, RspnsPacket* prspns) {
	if (send(sock, (char*)prspns, sizeof(RspnsPacket), 0) == SOCKET_ERROR) {
		cout << "与客户端失去连接" << endl;
		return 0;
	}
	return 1;
}

//接收命令报文
int RecvCmd(SOCKET sock, char*pCmd) {
	int nRet;
	int left = sizeof(CmdPacket);
	//从控制连接中读取数据，大小为CmdPacket
	while (left) {
		nRet = recv(sock, pCmd, left, 0);
		if (nRet == SOCKET_ERROR) {
			cout << "从客户端接收命令时出错" << endl;
			return 0;
		}
		if (nRet == 0) {
			cout << "客户端关闭了连接" << endl;
			return 0;
		}
		left -= nRet;
		pCmd += nRet;
	}
	return 1;
}
//建立数据连接
int InitDataSocket(SOCKET *pDataSock, SOCKADDR_IN *pClientAddr) {
	SOCKET datasock;
	//创建socket
	datasock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (datasock == INVALID_SOCKET) {
		cout << "创建socket失败" << endl;
		return 0;
	}
	SOCKADDR_IN tcpaddr;
	memcpy(&tcpaddr, pClientAddr, sizeof(SOCKADDR_IN));
	tcpaddr.sin_port = htons(DATA_PORT);//只需要修改端口值
	//请求连接客户端
	if (connect(datasock, (SOCKADDR*)&tcpaddr, sizeof(tcpaddr)) == SOCKET_ERROR) {
		cout << "连接客户端失败" << endl;
		closesocket(datasock);
		return 0;
	}
	*pDataSock = datasock;
	return 1;
}
//发送一项文件信息
int SendFileRecord(SOCKET datasock, WIN32_FIND_DATA* pfd) {
	char filerecord[MAX_PATH + 32];
	FILETIME ft;
	FileTimeToLocalFileTime(&pfd->ftLastWriteTime, &ft);
	SYSTEMTIME lastwtime;
	FileTimeToSystemTime(&ft, &lastwtime);
	char* dir = pfd->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY ? "<DIR>" : "";
	sprintf(filerecord, "%04d-%02d-%02d%02d%02d	%5s	%10d	%-20s\n",
		lastwtime.wYear, lastwtime.wMonth, lastwtime.wDay, lastwtime.wHour, lastwtime.wMinute, dir, pfd->nFileSizeLow, pfd->cFileName);
	if (send(datasock, filerecord, strlen(filerecord), 0) == SOCKET_ERROR) {
		cout << "发送文件列表失败" << endl;
		return 0;
	}
	return 1;
}
//发送文件列表信息
int SendFileList(SOCKET datasock){
	HANDLE hff;
	WIN32_FIND_DATA fd;

	//搜索文件
	hff = FindFirstFile("*", &fd);
	if (hff == INVALID_HANDLE_VALUE) {//发生错误
		string errstr = "无法列举文件";
		cout << "列举文件失败" << endl;
		if (send(datasock, (char*)&errstr, errstr.size(), 0) == SOCKET_ERROR) {
			cout << "发送文件列表时出错" << endl;
			closesocket(datasock);
			return 0;
		}
	}
	bool fMoreFiles = true;
	while (fMoreFiles) {
		//发送此项文件信息
		if (!SendFileRecord(datasock, &fd)) {
			closesocket(datasock);
			return 0;
		}
		//搜索下一个文件
		fMoreFiles = FindNextFile(hff, &fd);
	}
	closesocket(datasock);
	return 1;
}
int SendFile(SOCKET datasock, FILE* file) {
	char buf[1024];
	cout << "发送文件数据" << endl;
	for (;;) {
		int r = fread(buf, 1, 1024, file);
		if (send(datasock, buf, r, 0) == SOCKET_ERROR) {
			cout << "和客户端失去连接" << endl;
			closesocket(datasock);
			return 0;
		}
		if (r < 1024) {
			break;
		}
	}
	closesocket(datasock);
	cout << "文件传送完成" << endl;
	return 1;
}
int FileExists(const char* filename) {
	WIN32_FIND_DATA fd;
	if (FindFirstFile(filename, &fd) == INVALID_HANDLE_VALUE) {
		return 0;
	}
	return 1;
}
int RecvFile(SOCKET datasock, char* filename) {
	char buf[1024];
	FILE* file = fopen(filename, "wb");
	if (!file) {
		cout << "写文件失败" << endl;
		fclose(file);
		closesocket(datasock);
		return 0;
	}
	//接收数据
	cout << "接收文件数据" << endl;
	while (true) {
		int r = recv(datasock, buf, 1024, 0);
		if (r == SOCKET_ERROR) {
			cout << "接收文件失败" << endl;
			fclose(file);
			closesocket(datasock);
			return 0;
		}
		if (!r) {
			break;
		}
		fwrite(buf, 1, r, file);
	}
	fclose(file);
	closesocket(datasock);
	cout << "文件接收完毕" << endl;
	return 1;
}
//处理命令报文
int ProcessCmd(SOCKET sock, CmdPacket* pCmd, SOCKADDR_IN *pClientAddr) {
	SOCKET datasock;//数据连接套接字
	RspnsPacket  rspns;//回复报文

	FILE* file;
	//根据命令分类处理
	switch (pCmd->cmdid)
	{
	case LS:
		//首先建立数据连接
		if (!InitDataSocket(&datasock, pClientAddr)) {
			return 0;
		}
		//发送文件列表信息
		if (!SendFileList(datasock)) {
			return 0;
		}
		break;
	case PWD:
		//获取当前目录，并放入到回复报文中
		if (!GetCurrentDirectory(RESPONSE_SIZE, rspns.text)) {
			strcpy(rspns.text,"无法获取当前目录");
		}
		if (!sendResponse(sock, &rspns)) {
			return 0;
		}
		break;
	case CD:
		//设置当前目录，使用win32API
		if (SetCurrentDirectory(pCmd->param)) {
			if (!GetCurrentDirectory(RESPONSE_SIZE, rspns.text)) {
				strcpy(rspns.text, "无法获取当前目录");
				
			}
		}
		else {
			strcpy(rspns.text, "无法改变到目标目录");
			
		}
		if (!sendResponse(sock, &rspns)) {
			return 0;
		}
		break;
	case GET:
		//处理下载文件请求
		file = fopen(pCmd->param, "rb");//打开要下载的文件
		if (file) {
			rspns.rspnsid = OK;
			sprintf(rspns.text, "get file %s\n", pCmd->param);
			if (!sendResponse(sock, &rspns)) {
				fclose(file);
				return 0;
			}
			else {
				//创建额外的数据连接来传送数据
				if (!InitDataSocket(&datasock, pClientAddr)) {
					fclose(file);
					return 0;
				}
				if (!SendFile(datasock, file)) {
					return 0;
				}
				fclose(file);
			}
		}
		else {//打开文件失败
			rspns.rspnsid = ERR;
			strcpy(rspns.text, "无法打开文件");
			if (!sendResponse(sock, &rspns)) {
				return 0;
			}
		}
		break;
	case PUT:
		//处理上传文件请求
		//首先发送回复报文
		char filename[64];
		strcpy(filename, pCmd->param);
		if (FileExists(filename)) {
			rspns.rspnsid = ERR;
			strcpy(rspns.text, "存在同名文件");

			if (!sendResponse(sock, &rspns)) {
				return 0;
			}
		}
		else {
			//建立一个连接接收数据
			rspns.rspnsid = OK;
			if (!sendResponse(sock, &rspns))
				return 0;
			if (!InitDataSocket(&datasock, pClientAddr)) {
				return 0;
			}
			if (!RecvFile(datasock, filename)) {
				return 0;
			}
		}
		break;
	case QUIT:
		//退出
		cout << "客户端关闭连接" << endl;
		rspns.rspnsid = OK;
		strcpy(rspns.text, "欢迎下次访问");
		sendResponse(sock, &rspns);
		return 0;
	default:break;
	}
	return 1;
}
//线程函数，参数包括相应控制连接的套接字
DWORD WINAPI ThreadFunc(LPVOID lpParam) {
	SOCKET sock;
	SOCKADDR_IN clientaddr;

	sock = ((threadData*)lpParam)->sock;
	clientaddr = ((threadData*)lpParam)->clientaddr;
	cout << "socket编号是" << sock << endl;
	//发送回复报文给客户端，内含命令使用说明
	cout << "客户端的ip地址为" << inet_ntoa(clientaddr.sin_addr) << "，端口号为" << ntohs(clientaddr.sin_port);
	RspnsPacket rspns = { OK, "欢迎使用FTP服务器!\n1.LS\t(无参数);\n2.PWD\t(无参数);\n3.CD\t(路径);\n4.GET\t(文件);\n5.PUT\t(文件);\n6.QUIT\t(无参数)\n" };

	//string response = "欢迎使用FTP服务器!\n";
	//response+="1.LS\t(无参数);\n2.PWD\t(无参数);\n3.CD\t(路径);\n4.GET\t(文件);\n5.PUT\t(文件);\n6.QUIT\t(无参数)\n";
	sendResponse(sock, &rspns);

	//循环获取客户端命令报文并进行处理
	while (true) {
		CmdPacket cmd;
		if (!RecvCmd(sock, (char*)&cmd)) {
			break;
		}
		if (!ProcessCmd(sock, &cmd, &clientaddr)) {
			break;
		}
	}
	closesocket(sock);
	delete lpParam;
	return 0;

}
int main() {
	SOCKET listen_sock;//服务器控制连接侦听套接字
	struct threadData *pThreadInfo;//指向线程的指针
	//初始化ftp
	if (!initFTP(&listen_sock)) {
		//ftp初始化失败
		cout << "ftp初始化失败!" << endl;
		exit(0);
	}
	//循环接受客户端连接请求，并生成线程去处理
	while (true) {
		pThreadInfo = NULL;
		pThreadInfo = new threadData;//新建一个线程指针
		if (pThreadInfo == NULL) {
			cout << "内存分配失败" << endl;
			continue;
		}
		int len = sizeof(struct threadData);
		pThreadInfo->sock = accept(listen_sock, (SOCKADDR*)&pThreadInfo->clientaddr, &len);
		HANDLE hThread;
		DWORD dwThreadId;
		//创建线程处理客户端请求
		hThread = CreateThread(NULL, 0, ThreadFunc, pThreadInfo, 0, &dwThreadId);
		//检查线程是否创建成功
		if (hThread == NULL) {
			cout << "线程创建失败" << endl;
			closesocket(pThreadInfo->sock);
			delete pThreadInfo;
		}
	}
	return 0;
}