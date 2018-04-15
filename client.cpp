#include <stdio.h>
#include <WinSock2.h>
#include <Windows.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
#define BUFFSIZE 4096
#define OPTSIZE 10
int GetConOp()
{
	int opNum = 0;
	printf("Please enter op number\n");
	printf("=========================\n");
	printf("[1] CONNECT\n");
	printf("[2] DISCONNECT\n");
	printf("[3] TIME\n");
	printf("[4] NAME\n");
	printf("[5] LIST\n");
	printf("[6] MESSAGE\n");
	printf("[7] EXIT\n");
	printf("=========================\n");
	scanf("%d", &opNum);
	return opNum;
}

DWORD WINAPI RecvData(LPVOID param)
{
	SOCKET server = *(SOCKET *)param;
	char bufRecv[BUFFSIZE] = { 0 };
	char opt[OPTSIZE] = { 0 };
	char *temp = bufRecv;
	int ret = 0;
	while (1)
	{
		ret = recv(server, bufRecv, BUFFSIZE, 0);
		if (ret == SOCKET_ERROR)
		{
			printf("Receive failed!\n");
			break;
		}
		strcpy(opt, bufRecv);//这里没错把？
		if (strcmp(opt, "TIME") == 0)
		{
			time_t now;
			struct tm *tm_now;

			temp += (strlen(opt) + 1);
			memcpy(&now, temp, sizeof(now));
			tm_now = localtime(&now);
			printf("now datetime: %d-%d-%d %d:%d:%d\n",
				tm_now->tm_year + 1900,
				tm_now->tm_mon + 1,
				tm_now->tm_mday,
				tm_now->tm_hour,
				tm_now->tm_min,
				tm_now->tm_sec);
		}
		else if (strcmp(opt, "NAME") == 0)
		{
			temp += (strlen(opt) + 1);
			printf("Server Name: %s\n", temp);

		}
		else if (strcmp(opt, "LIST") == 0)
		{
			int i = 0;
			temp += (strlen("LIST") + 1);
			i = *(int *)temp;
			memset(bufRecv, 0, BUFFSIZE);
			temp = bufRecv;
			while (i > 0)
			{
				IN_ADDR addr;
				unsigned short port;
				unsigned short No;
				ret = recv(server, bufRecv, sizeof(unsigned short) * 2 + sizeof(IN_ADDR), 0);
				memcpy(&addr, temp, sizeof(addr));
				temp += sizeof(addr);
				memcpy(&port, temp, sizeof(port));
				temp += sizeof(port);
				memcpy(&No, temp, sizeof(No));
				temp = bufRecv;
				printf("Host %hu: IP %s  PORT %d  \n", No, inet_ntoa(addr), port);
				memset(bufRecv, 0, BUFFSIZE);
				i--;

			}
		}
		else if (strcmp(opt, "MSG") == 0)
		{
			temp += (strlen("MSG") + 1);
			unsigned short No = *(unsigned short *)temp;
			printf("Message from client #%hu \n", No);
			recv(server, bufRecv, BUFFSIZE, 0);
			printf("%s\n", bufRecv);

		}
		else
		{
			printf("Unknown message: %s\n", bufRecv);
		}

		memset(bufRecv, 0, BUFFSIZE);
		temp = bufRecv;
	}
	closesocket(server); //子线程的socket是拷贝了主线程的，这里不会产生重复关闭的错误把？
	return 0;
};


int main() {

	int ret = 0;
	WSADATA wsaData;
	int state = 0; // 0 未连接 1 已连接 
	char ip_address[16];
	unsigned short port;
	HANDLE Thread;

	sockaddr_in servAddr;
	memset(&servAddr, 0, sizeof(servAddr));



	//初始化WinSock MAKEWORD(2,2)表示使用2.2版本
	ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret != 0) {
		printf("WSAStartup failed with error: %d\n", ret);
		return 1;
	}

	//创建Socket
	SOCKET server = INVALID_SOCKET;

	while (1)
	{
		int opNum = 0;
		if (state == 0)
		{
			printf("Please enter op number\n");
			printf("=========================\n");
			printf("[1] CONNECT\n");
			printf("[2] EXIT\n");
			printf("=========================\n");
			scanf("%d", &opNum);
			if (opNum == 2)
				break;
			else if (opNum == 1)
			{
				server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (server == INVALID_SOCKET) {
					printf("socket failed with error: %ld\n", WSAGetLastError());
					WSACleanup();
					return 1;
				}
				printf("Please enter IP address\n");
				scanf("%s", ip_address);
				printf("Please enter PORT number\n");
				scanf("%hu", &port);
				servAddr.sin_family = AF_INET;
				servAddr.sin_addr.s_addr = inet_addr(ip_address);
				servAddr.sin_port = htons(port);
				ret = connect(server, (sockaddr *)&servAddr, sizeof(sockaddr));
				if (ret < 0)
				{
					printf("Connect failed\n");
					closesocket(server);
					server = INVALID_SOCKET;
					continue;
				}
				//连接成功
				state = 1;
				Thread = CreateThread(NULL, 0, RecvData, (LPVOID)&server, 0, NULL);

			}
			else
				printf("Invalid input, try again\n");
		}
		else
		{
			printf("Please enter op number\n");
			printf("=========================\n");
			printf("[1] DISCONNECT\n");
			printf("[2] TIME\n");
			printf("[3] NAME\n");
			printf("[4] LIST\n");
			printf("[5] MESSAGE\n");
			printf("[6] EXIT\n");
			printf("=========================\n");
			scanf("%d", &opNum);
			if (opNum == 6)
			{
				closesocket(server);//关闭套接字
				break;
			}
			else if (opNum == 1)
			{
				//说不要用这个函数，那该怎么通知子线程结束？用一个全局变量吗？
				TerminateThread(Thread, 0);
				closesocket(server);//关闭套接字
				state = 0;
				continue;
			}
			else if (opNum == 2)
			{
				char *buf = "TIME";
				send(server, buf, strlen(buf) + 1, 0);

			}
			else if (opNum == 3)
			{
				char *buf = "NAME";
				send(server, buf, strlen(buf) + 1, 0);
			}
			else if (opNum == 4)
			{
				char *buf = "LIST";
				send(server, buf, strlen(buf) + 1, 0);
			}
			else if (opNum == 5)
			{
				unsigned short No = 0;
				char *bufhead = new char[strlen("MSG") + 1 + sizeof(No)];
				char *buf = new char[BUFFSIZE];
				int count = 0;
				printf("Please enter host#: \n");
				scanf("%hu", &No);
				printf("Please enter your message : \n");
				memcpy(&bufhead[count], "MSG", 4);
				count += 4;
				memcpy(&bufhead[count], &No, sizeof(No));
				memset(buf, 0, BUFFSIZE);
				count = 0;
				getchar();
				while ((buf[count] = getchar()) != '\n')
				{
					count++;
					if (count == BUFFSIZE)
						break;
				}
				send(server, bufhead, 4 + sizeof(No), 0);
				send(server, buf, BUFFSIZE, 0);
				delete[] buf;
				delete[] bufhead;

			}
			else
				printf("Invalid input, try again\n");
		}

	}


	WSACleanup();  //终止使用 DLL
	return 0;
}
