#include<stdio.h>
#include<iostream>
#include<WinSock2.h>
#include<windows.h>
//#include<string.h>
#define CMD_PORT 5021
#define DATA_PORT 5020
#define DATA_BUFSIZE 4096
using namespace std;
const int CMD_PARAM_SIZE = 256;
const int RSPNS_TEXT_SIZE = 256;
typedef enum {
	LS,PWD,CD,GET,PUT,QUIT
}CmdID;
typedef struct _CmdPacket {
	CmdID cmdid;
	char param[CMD_PARAM_SIZE];
}CmdPacket;
typedef enum {
	OK,ERR
}RspnsID;
typedef struct _RspnsPacket {
	RspnsID respnsid;
	char text[RSPNS_TEXT_SIZE];
}RspnsPacket;
//读取回复报文
void do_read_rspns(SOCKET fd, RspnsPacket *ptr) {
	int cnt = 0;
	int size = sizeof(RspnsPacket);
	while (cnt < size) {
		int nRead = recv(fd, (char*)ptr + cnt, size - cnt, 0);
		if (nRead <= 0) {
			cout << "读取回复失败" << endl;
			closesocket(fd);
			exit(1);
		}
		cnt += nRead;
	}
}
//发送命令报文
void do_write_cmd(SOCKET fd, CmdPacket* ptr) {
	int size = sizeof(CmdPacket);
	int flag = send(fd, (char*)ptr, size, 0);
	if (flag == SOCKET_ERROR) {
		cout << "发送命令失败" <<endl;
		closesocket(fd);
		WSACleanup();
		exit(1);
	}
}
//创建数据连接套接字并进入侦听状态
SOCKET create_data_socket() {
	SOCKET sockfd;
	struct sockaddr_in my_addr;
	//创建用于数据连接的套接字
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		cout << "创建socket失败" << endl;
		WSACleanup();
		exit(1);
	}
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(DATA_PORT);
	my_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	memset(&(my_addr.sin_zero), 0, sizeof(my_addr.sin_zero));

	//绑定
	if (bind(sockfd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == SOCKET_ERROR) {
		int err = WSAGetLastError();
		cout << "绑定错误" << err << endl;
		closesocket(sockfd);
		WSACleanup();
		exit(1);
	}
	//侦听数据连接请求
	if (listen(sockfd, 1) == SOCKET_ERROR) {
		cout << "侦听错误" << endl;
		closesocket(sockfd);
		WSACleanup();
		exit(1);
	}
	return sockfd;
}
//处理list命令
void list(SOCKET sockfd) {
	int sin_size;
	int nRead;
	CmdPacket cmd_packet;
	SOCKET newsockfd, data_sockfd;
	struct sockaddr_in their_addr;
	char data_buf[DATA_BUFSIZE];

	//创建数据连接
	newsockfd = create_data_socket();
	//构建命令报文并发送至服务器
	cmd_packet.cmdid = LS; do_write_cmd(sockfd, &cmd_packet);

	sin_size = sizeof(struct sockaddr_in);
	//接收服务器的数据连接请求
	if ((data_sockfd = accept(newsockfd, (struct sockaddr*)&their_addr, &sin_size)) == INVALID_SOCKET) {
		cout << "获取文件时接收错误" << endl;
		closesocket(sockfd);
		closesocket(newsockfd);
		WSACleanup();
		exit(1);
	}

	//显示读取的数据
	while (true) {
		nRead=recv(data_sockfd, data_buf, DATA_BUFSIZE - 1, 0);
		if (nRead == SOCKET_ERROR) {
			cout << "读取回复失败"<<endl;
			closesocket(data_sockfd);
			closesocket(newsockfd);
			closesocket(sockfd);
			WSACleanup();
			exit(1);
		}
		if (nRead == 0)
			break;
		//显示数据
		data_buf[nRead] = '\0';
		cout << data_buf << endl;
	}
	closesocket(data_sockfd);
	closesocket(newsockfd);
}

//处理pwd命令
void pwd(int sockfd) {
	CmdPacket cmd_packet;
	RspnsPacket rspns_packet;
	cmd_packet.cmdid = PWD;
	//发送命令报文并且读取回复
	do_write_cmd(sockfd, &cmd_packet);
	do_read_rspns(sockfd, &rspns_packet);
	cout << rspns_packet.text << endl;
}

//处理cd命令
void cd(int sockfd) {
	CmdPacket cmd_packet;
	RspnsPacket rspns_packet;
	cmd_packet.cmdid = CD;
	scanf("%s", cmd_packet.param);
	//发送命令报文并且读取回复
	do_write_cmd(sockfd, &cmd_packet);
	do_read_rspns(sockfd, &rspns_packet);
	if(rspns_packet.respnsid!=ERR)
		cout << rspns_packet.text << endl;
}

//处理get命令，下载文件
void get_file(SOCKET sockfd) {
	FILE* fd;
	char data_buf[DATA_BUFSIZE];
	CmdPacket cmd_packet;
	RspnsPacket rspns_packet;
	SOCKET newsockfd, data_sockfd;
	struct sockaddr_in their_addr;
	int sin_size;
	int cnt;
	//设置命令报文
	cmd_packet.cmdid = GET;
	scanf("%s", cmd_packet.param);
	//打开或者创建本地文件以供写数据
	fd = fopen(cmd_packet.param, "wb");
	if (fd == NULL) {
		cout << "读取文件失败" << endl;
		return;
	}
	//创建数据连接并侦听服务器的连接请求
	newsockfd = create_data_socket();
	//发送命令报文
	do_write_cmd(sockfd, &cmd_packet);
	//读取回复报文
	do_read_rspns(sockfd, &rspns_packet);
	if (rspns_packet.respnsid == ERR) {
		cout << rspns_packet.text << endl;
		closesocket(newsockfd);
		fclose(fd);
		DeleteFile(cmd_packet.param);
		return;
	}
	sin_size = sizeof(struct sockaddr_in);
	//等待接受服务器的连接请求
	if ((data_sockfd = accept(newsockfd, (struct sockaddr*)&their_addr, &sin_size)) == INVALID_SOCKET) {
		cout << "接收错误" << endl;
		closesocket(newsockfd);
		fclose(fd);
		DeleteFile(cmd_packet.param);
		return;
	}
	//读取网络数据并写入文件
	while ((cnt = recv(data_sockfd, data_buf, DATA_BUFSIZE, 0)) > 0)
		fwrite(data_buf, sizeof(char), cnt, fd);
	closesocket(data_sockfd);
	closesocket(newsockfd);
	fclose(fd);
}

//处理put命令，上传文件
void put_file(SOCKET sockfd) {
	FILE* fd;
	char data_buf[DATA_BUFSIZE];
	CmdPacket cmd_packet;
	RspnsPacket rspns_packet;
	SOCKET newsockfd, data_sockfd;
	struct sockaddr_in their_addr;
	int sin_size;
	int cnt;
	//设置命令报文
	cmd_packet.cmdid = PUT;
	scanf("%s", cmd_packet.param);
	//打开或者创建本地文件以供写数据
	fd = fopen(cmd_packet.param, "rb");
	if (fd == NULL) {
		cout << "读取文件失败" << endl;
		return;
	}
	//创建数据连接并侦听服务器的连接请求
	newsockfd = create_data_socket();
	//发送命令报文
	do_write_cmd(sockfd, &cmd_packet);
	//读取回复报文
	do_read_rspns(sockfd, &rspns_packet);
	if (rspns_packet.respnsid == ERR) {
		cout << rspns_packet.text << endl;
		closesocket(newsockfd);
		fclose(fd);
		//DeleteFile(cmd_packet.param);
		return;
	}
	sin_size = sizeof(struct sockaddr_in);
	//等待接受服务器的连接请求
	if ((data_sockfd = accept(newsockfd, (struct sockaddr*)&their_addr, &sin_size)) == INVALID_SOCKET) {
		cout << "接收错误" << endl;
		closesocket(newsockfd);
		fclose(fd);
		//DeleteFile(cmd_packet.param);
		return;
	}
	//读取网络数据并写入文件
	while (true) {
		cnt = fread(data_buf, sizeof(char), DATA_BUFSIZE, fd);
		send(data_sockfd, data_buf, cnt, 0);
		if (cnt < DATA_BUFSIZE) {
			break;
		}
	}
	closesocket(data_sockfd);
	closesocket(newsockfd);
	fclose(fd);
}
//处理退出命令
void quit(int sockfd) {
	CmdPacket cmd_packet;
	RspnsPacket rspns_packet;
	cmd_packet.cmdid = QUIT;
	//发送命令报文并且读取回复
	do_write_cmd(sockfd, &cmd_packet);
	do_read_rspns(sockfd, &rspns_packet);
	cout << rspns_packet.text << endl;
}
void main() {
	SOCKET sockfd;
	struct hostent* he;
	struct sockaddr_in their_addr;
	char cmd[10];
	RspnsPacket rspns_packet;
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	char ip[20] = { 0 };
	cin >> ip;

	wVersionRequested = MAKEWORD(2, 2);
	//winsock初始化
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		cout << "winsock初始化失败" << endl;
		return;
	}
	//确认winsock版本是2.2
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		cout << "winsock版本不是2.2" << endl;
		WSACleanup();
		return;
	}
	//创建用于控制连接的socket
	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd == INVALID_SOCKET) {
		cout << "创建socket失败" << endl;
		WSACleanup();
		exit(1);
	}
	if ((he = gethostbyname(ip)) == NULL) {
		cout << "获取ip失败" << endl;
		exit(1);
	}

	their_addr.sin_family = AF_INET;
	their_addr.sin_port = htons(CMD_PORT);
	their_addr.sin_addr = *(struct in_addr*)he->h_addr_list[0];
	memset(&(their_addr.sin_zero), 0, sizeof(their_addr.sin_zero));
	//连接服务器
	if(connect(sockfd,(struct sockaddr*)&their_addr,sizeof(struct sockaddr)) == SOCKET_ERROR) {
		cout << "连接错误" << endl;
		closesocket(sockfd);
		WSACleanup();
		exit(1);
	}
	//连接成功后，首先接收服务器发回的消息
	do_read_rspns(sockfd, &rspns_packet);
	cout << rspns_packet.text << endl;
	//主循环,读取用户输入并分派执行
	while (true) {
		scanf("%s", cmd);
		switch (cmd[0]) {
		case 'l'://list命令
			list(sockfd); break;
		case 'p'://pwd命令
			if (cmd[1] == 'w') {
				pwd(sockfd);
			}
			else {
				put_file(sockfd);
			}
			break;
		case 'c'://处理cd命令
			cd(sockfd); break;
		case 'g'://处理get命令
			get_file(sockfd); break;
		case 'q'://处理quit命令
			quit(sockfd); break;
		default:cout << "无效命令" << endl; break;
		}
		if (cmd[0] == 'q')
			break;
	}
	WSACleanup();
}