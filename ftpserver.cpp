#include<iostream>
#include<cstdio>
#include<winsock2.h>
//#include<string>
using namespace std;
#define SERVER_PORT 5021//�����������˿ں�
#define DATA_PORT 5020//�������������������׽���
#define RESPONSE_SIZE 256//�������ݴ�С
#define CMD_PACKET_SIZE 256//����Ĵ�СΪ256
#define RSPNS_TEXT_SIZE 256//�ظ����Ļ����С
//��ö�����ͱ�������
typedef enum {
	//ls,pwd,cd,get,put,quit
	LS,PWD,CD,GET,PUT,QUIT
}CmdID;
//����ģ��ӿͻ��˷���������
typedef struct _cmdPacket{
	CmdID cmdid;
	char param[CMD_PACKET_SIZE];
}CmdPacket;
//��ö�����ͱ���ظ�������
typedef enum {
	OK,ERR
}RspnsID;
//�ظ����ģ��ӿͻ��˷���������
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
	//����
	error = WSAStartup(wVersionRequested, &wsaData);
	if (error != 0) {
		cout << "Winsock��ʼ����������!" << endl;
		return false;
	}
	//�жϰ汾
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		WSACleanup();
		cout << "Winsock�汾��Ч" << endl;
		return false;
	}
	//ʹ��TCPЭ�鴴��socket
	listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_sock == INVALID_SOCKET) {
		WSACleanup();
		cout << "����Socketʧ��" << endl;
		return false;
	}
	SOCKADDR_IN tcpaddr;
	tcpaddr.sin_family = AF_INET;
	tcpaddr.sin_port = htons(SERVER_PORT);
	tcpaddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	//���׽���
	error = bind(listen_sock, (SOCKADDR*)&tcpaddr, sizeof(tcpaddr));
	if (error != 0) {
		error = WSAGetLastError();
		WSACleanup();
		cout << "socket��ʱ��������" << endl;
		return false;
	}
	//�ڶ��������ǵȴ����Ӷ��е���󳤶�,������󳤶ȵĲ�������ᱻ�ܾ�
	error = listen(listen_sock, 3);
	if (error != 0) {
		WSACleanup();
		cout << "socket����ʱ��������" << endl;
		return false;
	}
	//����ֵ
	*pListenSock = listen_sock;
	return true;

}
//�������˷��ͻظ�����
int sendResponse(SOCKET sock, RspnsPacket* prspns) {
	if (send(sock, (char*)prspns, sizeof(RspnsPacket), 0) == SOCKET_ERROR) {
		cout << "��ͻ���ʧȥ����" << endl;
		return 0;
	}
	return 1;
}

//���������
int RecvCmd(SOCKET sock, char*pCmd) {
	int nRet;
	int left = sizeof(CmdPacket);
	//�ӿ��������ж�ȡ���ݣ���СΪCmdPacket
	while (left) {
		nRet = recv(sock, pCmd, left, 0);
		if (nRet == SOCKET_ERROR) {
			cout << "�ӿͻ��˽�������ʱ����" << endl;
			return 0;
		}
		if (nRet == 0) {
			cout << "�ͻ��˹ر�������" << endl;
			return 0;
		}
		left -= nRet;
		pCmd += nRet;
	}
	return 1;
}
//������������
int InitDataSocket(SOCKET *pDataSock, SOCKADDR_IN *pClientAddr) {
	SOCKET datasock;
	//����socket
	datasock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (datasock == INVALID_SOCKET) {
		cout << "����socketʧ��" << endl;
		return 0;
	}
	SOCKADDR_IN tcpaddr;
	memcpy(&tcpaddr, pClientAddr, sizeof(SOCKADDR_IN));
	tcpaddr.sin_port = htons(DATA_PORT);//ֻ��Ҫ�޸Ķ˿�ֵ
	//�������ӿͻ���
	if (connect(datasock, (SOCKADDR*)&tcpaddr, sizeof(tcpaddr)) == SOCKET_ERROR) {
		cout << "���ӿͻ���ʧ��" << endl;
		closesocket(datasock);
		return 0;
	}
	*pDataSock = datasock;
	return 1;
}
//����һ���ļ���Ϣ
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
		cout << "�����ļ��б�ʧ��" << endl;
		return 0;
	}
	return 1;
}
//�����ļ��б���Ϣ
int SendFileList(SOCKET datasock){
	HANDLE hff;
	WIN32_FIND_DATA fd;

	//�����ļ�
	hff = FindFirstFile("*", &fd);
	if (hff == INVALID_HANDLE_VALUE) {//��������
		string errstr = "�޷��о��ļ�";
		cout << "�о��ļ�ʧ��" << endl;
		if (send(datasock, (char*)&errstr, errstr.size(), 0) == SOCKET_ERROR) {
			cout << "�����ļ��б�ʱ����" << endl;
			closesocket(datasock);
			return 0;
		}
	}
	bool fMoreFiles = true;
	while (fMoreFiles) {
		//���ʹ����ļ���Ϣ
		if (!SendFileRecord(datasock, &fd)) {
			closesocket(datasock);
			return 0;
		}
		//������һ���ļ�
		fMoreFiles = FindNextFile(hff, &fd);
	}
	closesocket(datasock);
	return 1;
}
int SendFile(SOCKET datasock, FILE* file) {
	char buf[1024];
	cout << "�����ļ�����" << endl;
	for (;;) {
		int r = fread(buf, 1, 1024, file);
		if (send(datasock, buf, r, 0) == SOCKET_ERROR) {
			cout << "�Ϳͻ���ʧȥ����" << endl;
			closesocket(datasock);
			return 0;
		}
		if (r < 1024) {
			break;
		}
	}
	closesocket(datasock);
	cout << "�ļ��������" << endl;
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
		cout << "д�ļ�ʧ��" << endl;
		fclose(file);
		closesocket(datasock);
		return 0;
	}
	//��������
	cout << "�����ļ�����" << endl;
	while (true) {
		int r = recv(datasock, buf, 1024, 0);
		if (r == SOCKET_ERROR) {
			cout << "�����ļ�ʧ��" << endl;
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
	cout << "�ļ��������" << endl;
	return 1;
}
//���������
int ProcessCmd(SOCKET sock, CmdPacket* pCmd, SOCKADDR_IN *pClientAddr) {
	SOCKET datasock;//���������׽���
	RspnsPacket  rspns;//�ظ�����

	FILE* file;
	//����������ദ��
	switch (pCmd->cmdid)
	{
	case LS:
		//���Ƚ�����������
		if (!InitDataSocket(&datasock, pClientAddr)) {
			return 0;
		}
		//�����ļ��б���Ϣ
		if (!SendFileList(datasock)) {
			return 0;
		}
		break;
	case PWD:
		//��ȡ��ǰĿ¼�������뵽�ظ�������
		if (!GetCurrentDirectory(RESPONSE_SIZE, rspns.text)) {
			strcpy(rspns.text,"�޷���ȡ��ǰĿ¼");
		}
		if (!sendResponse(sock, &rspns)) {
			return 0;
		}
		break;
	case CD:
		//���õ�ǰĿ¼��ʹ��win32API
		if (SetCurrentDirectory(pCmd->param)) {
			if (!GetCurrentDirectory(RESPONSE_SIZE, rspns.text)) {
				strcpy(rspns.text, "�޷���ȡ��ǰĿ¼");
				
			}
		}
		else {
			strcpy(rspns.text, "�޷��ı䵽Ŀ��Ŀ¼");
			
		}
		if (!sendResponse(sock, &rspns)) {
			return 0;
		}
		break;
	case GET:
		//���������ļ�����
		file = fopen(pCmd->param, "rb");//��Ҫ���ص��ļ�
		if (file) {
			rspns.rspnsid = OK;
			sprintf(rspns.text, "get file %s\n", pCmd->param);
			if (!sendResponse(sock, &rspns)) {
				fclose(file);
				return 0;
			}
			else {
				//���������������������������
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
		else {//���ļ�ʧ��
			rspns.rspnsid = ERR;
			strcpy(rspns.text, "�޷����ļ�");
			if (!sendResponse(sock, &rspns)) {
				return 0;
			}
		}
		break;
	case PUT:
		//�����ϴ��ļ�����
		//���ȷ��ͻظ�����
		char filename[64];
		strcpy(filename, pCmd->param);
		if (FileExists(filename)) {
			rspns.rspnsid = ERR;
			strcpy(rspns.text, "����ͬ���ļ�");

			if (!sendResponse(sock, &rspns)) {
				return 0;
			}
		}
		else {
			//����һ�����ӽ�������
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
		//�˳�
		cout << "�ͻ��˹ر�����" << endl;
		rspns.rspnsid = OK;
		strcpy(rspns.text, "��ӭ�´η���");
		sendResponse(sock, &rspns);
		return 0;
	default:break;
	}
	return 1;
}
//�̺߳���������������Ӧ�������ӵ��׽���
DWORD WINAPI ThreadFunc(LPVOID lpParam) {
	SOCKET sock;
	SOCKADDR_IN clientaddr;

	sock = ((threadData*)lpParam)->sock;
	clientaddr = ((threadData*)lpParam)->clientaddr;
	cout << "socket�����" << sock << endl;
	//���ͻظ����ĸ��ͻ��ˣ��ں�����ʹ��˵��
	cout << "�ͻ��˵�ip��ַΪ" << inet_ntoa(clientaddr.sin_addr) << "���˿ں�Ϊ" << ntohs(clientaddr.sin_port);
	RspnsPacket rspns = { OK, "��ӭʹ��FTP������!\n1.LS\t(�޲���);\n2.PWD\t(�޲���);\n3.CD\t(·��);\n4.GET\t(�ļ�);\n5.PUT\t(�ļ�);\n6.QUIT\t(�޲���)\n" };

	//string response = "��ӭʹ��FTP������!\n";
	//response+="1.LS\t(�޲���);\n2.PWD\t(�޲���);\n3.CD\t(·��);\n4.GET\t(�ļ�);\n5.PUT\t(�ļ�);\n6.QUIT\t(�޲���)\n";
	sendResponse(sock, &rspns);

	//ѭ����ȡ�ͻ�������Ĳ����д���
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
	SOCKET listen_sock;//�������������������׽���
	struct threadData *pThreadInfo;//ָ���̵߳�ָ��
	//��ʼ��ftp
	if (!initFTP(&listen_sock)) {
		//ftp��ʼ��ʧ��
		cout << "ftp��ʼ��ʧ��!" << endl;
		exit(0);
	}
	//ѭ�����ܿͻ����������󣬲������߳�ȥ����
	while (true) {
		pThreadInfo = NULL;
		pThreadInfo = new threadData;//�½�һ���߳�ָ��
		if (pThreadInfo == NULL) {
			cout << "�ڴ����ʧ��" << endl;
			continue;
		}
		int len = sizeof(struct threadData);
		pThreadInfo->sock = accept(listen_sock, (SOCKADDR*)&pThreadInfo->clientaddr, &len);
		HANDLE hThread;
		DWORD dwThreadId;
		//�����̴߳���ͻ�������
		hThread = CreateThread(NULL, 0, ThreadFunc, pThreadInfo, 0, &dwThreadId);
		//����߳��Ƿ񴴽��ɹ�
		if (hThread == NULL) {
			cout << "�̴߳���ʧ��" << endl;
			closesocket(pThreadInfo->sock);
			delete pThreadInfo;
		}
	}
	return 0;
}