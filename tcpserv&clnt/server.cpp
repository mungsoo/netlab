#include<stdio.h>
#include<WinSock2.h>
#include<Windows.h>
#include<string.h>
#include<time.h>
#include<list>
#include <stdlib.h>
#pragma comment (lib, "ws2_32.lib")

#define BACKLOG 20
#define MAXCLIENT 20
#define BUFFSIZE 4096
#define OPTSIZE 10

//客户端列表
typedef struct clntList{
	uint16_t No;
	SOCKADDR clntAddr;
	SOCKET clntSocket;
} clntList;
std::list<clntList> clnt;

//int nThreads = 0;
//HANDLE mutex = NULL;

int IsSocketClosed(SOCKET clientSocket)
{
	bool ret = false;
	HANDLE closeEvent = WSACreateEvent();
	WSAEventSelect(clientSocket, closeEvent, FD_CLOSE);

	DWORD dwRet = WaitForSingleObject(closeEvent, 0);

	if (dwRet == WSA_WAIT_EVENT_0)
		ret = true;
	else if (dwRet == WSA_WAIT_TIMEOUT)
		ret = false;

	WSACloseEvent(closeEvent);
	return ret;
}

void GetClntList(SOCKET s)
{
	int nSize = sizeof(IN_ADDR) + 2 * sizeof(uint16_t);

	//const int nSize = sizeof(IN_ADDR) + 2 * sizeof(uint16_t);
	//char buf[nSize];
	char *buf = new char[BUFFSIZE];
	char *temp = buf;
	std::list<clntList>::iterator iter = clnt.begin();
	int total_num = clnt.size();
	strcpy(buf, "LIST");
	memcpy(buf + strlen("LIST") + 1, (char *)&total_num, sizeof(total_num));
	send(s, buf, strlen("LIST") + 1 + sizeof(int), 0);
	memset(buf, 0, nSize);
	for (; iter != clnt.end(); iter++)
	{

		IN_ADDR addr = ((sockaddr_in *)(&(iter->clntAddr)))->sin_addr;
		uint16_t port = ((sockaddr_in *)(&(iter->clntAddr)))->sin_port;
		uint16_t No = iter->No;
		memcpy(temp, &addr, sizeof(addr));
		temp += sizeof(addr);
		memcpy(temp, &port, sizeof(uint16_t));
		temp += sizeof(uint16_t);
		memcpy(temp, &No, sizeof(uint16_t));
		send(s, buf, nSize, NULL);
		memset(buf, 0, nSize);
		temp = buf;
	}

	delete[] buf;
	
}

DWORD WINAPI ServAction(LPVOID param)
{
	std::list<clntList>::iterator iter = *(std::list<clntList>::iterator *)param;
	SOCKET s = iter->clntSocket;
	char bufRecv[BUFFSIZE];
	char bufSend[BUFFSIZE];
	char *temp = bufSend;
	char opt[OPTSIZE];
	char *mesg = NULL;
	
	char *clntIP = new char[16];
	strcpy(clntIP, inet_ntoa(((sockaddr_in *)(&(iter->clntAddr)))->sin_addr));

	memset(bufRecv, 0, BUFFSIZE);
	memset(opt, 0, OPTSIZE);
	memset(bufSend, 0, BUFFSIZE);

	//strcpy(buf, "Hello\n");
	//char *c = "Hello!\n";
	//send(s, c, strlen(c) + sizeof(char), NULL);

	while (1)
	{	
		//if (IsSocketClosed(s) == TRUE)
			//break;
		
		int ret = recv(s, bufRecv, BUFFSIZE, NULL);
		if (ret == SOCKET_ERROR)
		{
			printf("Receive failed!\n");
			break;
		}
		else
		{
			int i;
			for (i = 0; bufRecv[i] != ' ' && bufRecv[i] != '\0'; i++)
				opt[i] = bufRecv[i];
			opt[i] = '\0';

			if (strcmp(opt, "TIME") == 0)
			{
				
				time_t sec;
				time(&sec);
				strcpy(bufSend, "TIME");
				temp += (strlen("TIME") + 1);
				memcpy(temp, &sec, sizeof(sec));
				send(s, bufSend, BUFFSIZE, NULL);
			}
			else if (strcmp(opt, "NAME") == 0)
			{	
				DWORD nameLen = BUFFSIZE; 
				strcpy(bufSend, "NAME");
				temp += (strlen("NAME") + 1);

				GetComputerName(temp, &nameLen);
				send(s, bufSend, BUFFSIZE, NULL);
			}
			else if (strcmp(opt, "LIST") == 0)
			{
				GetClntList(s);

			}
			else if (strcmp(opt, "MSG") == 0)
			{
				//bufRecv[i]指向opt结尾的\0
				i++;
				int NoToSend;
				memcpy(&NoToSend, &bufRecv[i], sizeof(NoToSend));
				memset(bufRecv, 0, BUFFSIZE);
				std::list<clntList>::iterator t_iter = clnt.begin();
				for (; t_iter != clnt.end(); t_iter++)
				{
					if (t_iter->No == NoToSend)
						break;
				}
				
				//发送消息头
				int nSize = sizeof(iter->No) + strlen("MSG");
				memcpy(bufSend, "MSG", 4);
				memcpy(bufSend + 4, &iter->No, sizeof(unsigned short));

				send(t_iter->clntSocket, bufSend, nSize, 0);

				recv(s, bufRecv, BUFFSIZE, 0);
				memcpy(bufSend, bufRecv, BUFFSIZE);
				send(t_iter->clntSocket, bufSend, BUFFSIZE, 0);
			}
			else
			{
				mesg = "Invalid Request\n";
				send(s, mesg, strlen(mesg) + sizeof(char), NULL);
				printf("Invalid Request from %s\n", clntIP);
			}
		}
		memset(bufRecv, 0, BUFFSIZE);
		memset(bufSend, 0, BUFFSIZE);
		temp = bufSend;
	}
	clnt.erase(iter);
	closesocket(s);
	delete[] clntIP;
};

int main() {

	int ret = 0;
	WSADATA wsaData;
	uint16_t No = 0;
	//std::list<HANDLE> hThread;
	//mutex = CreateMutex(NULL, 0, NULL);

	//初始化WinSock MAKEWORD(2,2)表示使用2.2版本
	ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret != 0) {
		printf("WSAStartup failed with error: %d\n", ret);
		return 1;
	}

	//创建Socket
	SOCKET servSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (servSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	//将IP地址和端口与Socket绑定
	sockaddr_in servAddr;
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(4209);
	servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	ret = bind(servSocket, (sockaddr*)&servAddr, sizeof(servAddr));
	if (ret == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		closesocket(servSocket);
		WSACleanup();
		return 1;
	}


	//进入监听状态
	ret = listen(servSocket, BACKLOG);
	if (ret == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(servSocket);
		WSACleanup();
		return 1;
	}
	while (1)
	{
		clntList tempclnt;
		tempclnt.No = No;
		int nSize = sizeof(tempclnt.clntAddr);
		//接受客户端请求并且创建Socket
		tempclnt.clntSocket = accept(servSocket, (sockaddr *)&tempclnt.clntAddr, &nSize);
		if (tempclnt.clntSocket == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			closesocket(servSocket);
			WSACleanup();
			return 1;
		}
		clnt.push_back(tempclnt);
		std::list<clntList>::iterator iter = clnt.end();
		iter--;
		char iterbuf[sizeof(std::list<clntList>::iterator)];
		memcpy(iterbuf, &iter, sizeof(std::list<clntList>::iterator));
		HANDLE Thread = CreateThread(NULL, 0, ServAction, (LPVOID)iterbuf, 0, NULL);
		CloseHandle(Thread);
		No++;
		//这个句柄要不要留得？
		//hThread.push_back(Thread);

	}
	WSACleanup();

}