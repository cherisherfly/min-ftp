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
//��ȡ�ظ�����
void do_read_rspns(SOCKET fd, RspnsPacket *ptr) {
	int cnt = 0;
	int size = sizeof(RspnsPacket);
	while (cnt < size) {
		int nRead = recv(fd, (char*)ptr + cnt, size - cnt, 0);
		if (nRead <= 0) {
			cout << "��ȡ�ظ�ʧ��" << endl;
			closesocket(fd);
			exit(1);
		}
		cnt += nRead;
	}
}
//���������
void do_write_cmd(SOCKET fd, CmdPacket* ptr) {
	int size = sizeof(CmdPacket);
	int flag = send(fd, (char*)ptr, size, 0);
	if (flag == SOCKET_ERROR) {
		cout << "��������ʧ��" <<endl;
		closesocket(fd);
		WSACleanup();
		exit(1);
	}
}
//�������������׽��ֲ���������״̬
SOCKET create_data_socket() {
	SOCKET sockfd;
	struct sockaddr_in my_addr;
	//���������������ӵ��׽���
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		cout << "����socketʧ��" << endl;
		WSACleanup();
		exit(1);
	}
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(DATA_PORT);
	my_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	memset(&(my_addr.sin_zero), 0, sizeof(my_addr.sin_zero));

	//��
	if (bind(sockfd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == SOCKET_ERROR) {
		int err = WSAGetLastError();
		cout << "�󶨴���" << err << endl;
		closesocket(sockfd);
		WSACleanup();
		exit(1);
	}
	//����������������
	if (listen(sockfd, 1) == SOCKET_ERROR) {
		cout << "��������" << endl;
		closesocket(sockfd);
		WSACleanup();
		exit(1);
	}
	return sockfd;
}
//����list����
void list(SOCKET sockfd) {
	int sin_size;
	int nRead;
	CmdPacket cmd_packet;
	SOCKET newsockfd, data_sockfd;
	struct sockaddr_in their_addr;
	char data_buf[DATA_BUFSIZE];

	//������������
	newsockfd = create_data_socket();
	//��������Ĳ�������������
	cmd_packet.cmdid = LS; do_write_cmd(sockfd, &cmd_packet);

	sin_size = sizeof(struct sockaddr_in);
	//���շ�������������������
	if ((data_sockfd = accept(newsockfd, (struct sockaddr*)&their_addr, &sin_size)) == INVALID_SOCKET) {
		cout << "��ȡ�ļ�ʱ���մ���" << endl;
		closesocket(sockfd);
		closesocket(newsockfd);
		WSACleanup();
		exit(1);
	}

	//��ʾ��ȡ������
	while (true) {
		nRead=recv(data_sockfd, data_buf, DATA_BUFSIZE - 1, 0);
		if (nRead == SOCKET_ERROR) {
			cout << "��ȡ�ظ�ʧ��"<<endl;
			closesocket(data_sockfd);
			closesocket(newsockfd);
			closesocket(sockfd);
			WSACleanup();
			exit(1);
		}
		if (nRead == 0)
			break;
		//��ʾ����
		data_buf[nRead] = '\0';
		cout << data_buf << endl;
	}
	closesocket(data_sockfd);
	closesocket(newsockfd);
}

//����pwd����
void pwd(int sockfd) {
	CmdPacket cmd_packet;
	RspnsPacket rspns_packet;
	cmd_packet.cmdid = PWD;
	//��������Ĳ��Ҷ�ȡ�ظ�
	do_write_cmd(sockfd, &cmd_packet);
	do_read_rspns(sockfd, &rspns_packet);
	cout << rspns_packet.text << endl;
}

//����cd����
void cd(int sockfd) {
	CmdPacket cmd_packet;
	RspnsPacket rspns_packet;
	cmd_packet.cmdid = CD;
	scanf("%s", cmd_packet.param);
	//��������Ĳ��Ҷ�ȡ�ظ�
	do_write_cmd(sockfd, &cmd_packet);
	do_read_rspns(sockfd, &rspns_packet);
	if(rspns_packet.respnsid!=ERR)
		cout << rspns_packet.text << endl;
}

//����get��������ļ�
void get_file(SOCKET sockfd) {
	FILE* fd;
	char data_buf[DATA_BUFSIZE];
	CmdPacket cmd_packet;
	RspnsPacket rspns_packet;
	SOCKET newsockfd, data_sockfd;
	struct sockaddr_in their_addr;
	int sin_size;
	int cnt;
	//���������
	cmd_packet.cmdid = GET;
	scanf("%s", cmd_packet.param);
	//�򿪻��ߴ��������ļ��Թ�д����
	fd = fopen(cmd_packet.param, "wb");
	if (fd == NULL) {
		cout << "��ȡ�ļ�ʧ��" << endl;
		return;
	}
	//�����������Ӳ���������������������
	newsockfd = create_data_socket();
	//���������
	do_write_cmd(sockfd, &cmd_packet);
	//��ȡ�ظ�����
	do_read_rspns(sockfd, &rspns_packet);
	if (rspns_packet.respnsid == ERR) {
		cout << rspns_packet.text << endl;
		closesocket(newsockfd);
		fclose(fd);
		DeleteFile(cmd_packet.param);
		return;
	}
	sin_size = sizeof(struct sockaddr_in);
	//�ȴ����ܷ���������������
	if ((data_sockfd = accept(newsockfd, (struct sockaddr*)&their_addr, &sin_size)) == INVALID_SOCKET) {
		cout << "���մ���" << endl;
		closesocket(newsockfd);
		fclose(fd);
		DeleteFile(cmd_packet.param);
		return;
	}
	//��ȡ�������ݲ�д���ļ�
	while ((cnt = recv(data_sockfd, data_buf, DATA_BUFSIZE, 0)) > 0)
		fwrite(data_buf, sizeof(char), cnt, fd);
	closesocket(data_sockfd);
	closesocket(newsockfd);
	fclose(fd);
}

//����put����ϴ��ļ�
void put_file(SOCKET sockfd) {
	FILE* fd;
	char data_buf[DATA_BUFSIZE];
	CmdPacket cmd_packet;
	RspnsPacket rspns_packet;
	SOCKET newsockfd, data_sockfd;
	struct sockaddr_in their_addr;
	int sin_size;
	int cnt;
	//���������
	cmd_packet.cmdid = PUT;
	scanf("%s", cmd_packet.param);
	//�򿪻��ߴ��������ļ��Թ�д����
	fd = fopen(cmd_packet.param, "rb");
	if (fd == NULL) {
		cout << "��ȡ�ļ�ʧ��" << endl;
		return;
	}
	//�����������Ӳ���������������������
	newsockfd = create_data_socket();
	//���������
	do_write_cmd(sockfd, &cmd_packet);
	//��ȡ�ظ�����
	do_read_rspns(sockfd, &rspns_packet);
	if (rspns_packet.respnsid == ERR) {
		cout << rspns_packet.text << endl;
		closesocket(newsockfd);
		fclose(fd);
		//DeleteFile(cmd_packet.param);
		return;
	}
	sin_size = sizeof(struct sockaddr_in);
	//�ȴ����ܷ���������������
	if ((data_sockfd = accept(newsockfd, (struct sockaddr*)&their_addr, &sin_size)) == INVALID_SOCKET) {
		cout << "���մ���" << endl;
		closesocket(newsockfd);
		fclose(fd);
		//DeleteFile(cmd_packet.param);
		return;
	}
	//��ȡ�������ݲ�д���ļ�
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
//�����˳�����
void quit(int sockfd) {
	CmdPacket cmd_packet;
	RspnsPacket rspns_packet;
	cmd_packet.cmdid = QUIT;
	//��������Ĳ��Ҷ�ȡ�ظ�
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
	//winsock��ʼ��
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		cout << "winsock��ʼ��ʧ��" << endl;
		return;
	}
	//ȷ��winsock�汾��2.2
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		cout << "winsock�汾����2.2" << endl;
		WSACleanup();
		return;
	}
	//�������ڿ������ӵ�socket
	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd == INVALID_SOCKET) {
		cout << "����socketʧ��" << endl;
		WSACleanup();
		exit(1);
	}
	if ((he = gethostbyname(ip)) == NULL) {
		cout << "��ȡipʧ��" << endl;
		exit(1);
	}

	their_addr.sin_family = AF_INET;
	their_addr.sin_port = htons(CMD_PORT);
	their_addr.sin_addr = *(struct in_addr*)he->h_addr_list[0];
	memset(&(their_addr.sin_zero), 0, sizeof(their_addr.sin_zero));
	//���ӷ�����
	if(connect(sockfd,(struct sockaddr*)&their_addr,sizeof(struct sockaddr)) == SOCKET_ERROR) {
		cout << "���Ӵ���" << endl;
		closesocket(sockfd);
		WSACleanup();
		exit(1);
	}
	//���ӳɹ������Ƚ��շ��������ص���Ϣ
	do_read_rspns(sockfd, &rspns_packet);
	cout << rspns_packet.text << endl;
	//��ѭ��,��ȡ�û����벢����ִ��
	while (true) {
		scanf("%s", cmd);
		switch (cmd[0]) {
		case 'l'://list����
			list(sockfd); break;
		case 'p'://pwd����
			if (cmd[1] == 'w') {
				pwd(sockfd);
			}
			else {
				put_file(sockfd);
			}
			break;
		case 'c'://����cd����
			cd(sockfd); break;
		case 'g'://����get����
			get_file(sockfd); break;
		case 'q'://����quit����
			quit(sockfd); break;
		default:cout << "��Ч����" << endl; break;
		}
		if (cmd[0] == 'q')
			break;
	}
	WSACleanup();
}