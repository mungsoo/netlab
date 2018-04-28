#define _CRT_SECURE_NO_WARNINGS    
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include"webserver.h"
#include<winsock2.h>
#include<windows.h>
#include<iostream>
#include<string>
#include<fstream>
#pragma comment (lib, "Ws2_32.lib")


std::mutex list_mtx;
std::condition_variable cond;

//Indicate if server is terminated
//If it is true, connection request will be refused
std::atomic<bool> serverEnd = false;

std::list<clntInfo> clients;


void HandleInput()
{
	std::string input;
	do {
		std::cin >> input;
		if (input == "quit")
		{
			serverEnd = true;

			//Hold list_mtx
			std::unique_lock<std::mutex> lck(list_mtx);
			
			while (!clients.empty())
			{
				//Wait for all threads
				cond.wait(lck);

				//After being waken, check clients.empty() again
				//To make sure all threads have exited
			}
			lck.unlock();
			std::cout << "Server terminated\n";
			WSACleanup();
			exit(0);
		}
	} while (1);
}


void HandleHttpRequest(SOCKET clientSocket)
{
	list_mtx.lock();
	std::list<clntInfo>::iterator iter = clients.begin();
	
	for (; iter != clients.end(); iter++)
	{
		if (iter->clientSocket == clientSocket)
		{
			break;
		}
	}
	IN_ADDR addr = iter->clientAddr.sin_addr;

	unsigned short port = ntohs(iter->clientAddr.sin_port);
	//unsigned short port = iter->clientAddr.sin_port;

	std::cout << " Client IP: " << inet_ntoa(addr) << " PORT: " <<port <<  std::endl;
	list_mtx.unlock();



	//receive data
	char recvBuffer[BUF_SIZE];
	char sendBuffer[BUF_SIZE];
	memset(recvBuffer, 0, BUF_SIZE);
	memset(sendBuffer, 0, BUF_SIZE);
	int ret = 0;
	ret = recv(clientSocket, recvBuffer, BUF_SIZE, 0);
	if (ret == 0)
		std::cout << "Connection closeing" << std::endl;
	else if(ret == SOCKET_ERROR)
		std::cout << "receive failed with error: " << WSAGetLastError() << std::endl;

	else
	{
		//Extract request method
		std::string httpMethod;
		char *head = recvBuffer, *end = recvBuffer;
		while (*end != ' ')end++;
		httpMethod.append(head, 0, end - head);

		//Extract request address
		std::string urlToGet;
		head = ++end;
		while (*end != ' ')end++;
		urlToGet.append(head, 0, end - head);

		//Send data back
		if (httpMethod == "GET")
		{

			std::cout << "GET " << urlToGet << std::endl;
			char contentType[100];
			if (urlToGet.substr(0, 5) == "/img")
				strcpy(contentType, "Content-Type: image/jpeg;charset=ISO-8859-1\n");
			else if(urlToGet.substr(0, 6) == "/html/")
				strcpy(contentType, "Content-Type: text/html;charset=ISO-8859-1\n");

			//map url into the address in host
			urlToGet.insert(0, "C:\\Users\\บฦิด");
			std::fstream is(urlToGet, std::ios::in|std::ios::binary);
			
			//File does not exist
			if (!is.is_open())
			{
				strcpy(sendBuffer, "HTTP/1.1 404 Not Found\n");
				std::cout << "file \"" << urlToGet << "\" dosen't exist" << std::endl;
			}
			else
			{
				//send mutiple times when the buffer is not large enough
				
				//HEAD
				strcpy(sendBuffer, "HTTP/1.1 200 OK\n");
				strcat(sendBuffer, contentType); 
				strcat(sendBuffer, "Content-Length: ");
				is.seekg(0, is.end);
				std::streamoff cur_size = is.tellg();
				char contentLength[32] = { 0 };
				sprintf(contentLength, "%lld", cur_size);
				strcat(sendBuffer, contentLength);
				strcat(sendBuffer, "\n\n");
				//strcat(sendBuffer, "Content-Type: text/html; charset=utf-8\n");

				//Concatenate CONTENT and send
				is.seekg(0, is.beg);
				int maxSendSize;
				do
				{
					maxSendSize = BUF_SIZE - strlen(sendBuffer) < cur_size ? BUF_SIZE - strlen(sendBuffer) : cur_size;
					is.read(sendBuffer + strlen(sendBuffer), maxSendSize);
					ret = send(clientSocket, sendBuffer, BUF_SIZE, 0);
					if (ret == SOCKET_ERROR)
					{
						std::cout<< "send failed with error: %d\n" <<  WSAGetLastError() << std::endl;
						break;
					}
					cur_size -= maxSendSize;
					memset(sendBuffer, 0, BUF_SIZE);
				} while (cur_size > 0);


			}
		}
		else if (httpMethod == "POST")
		{
			std::cout << recvBuffer << std::endl;
			if (urlToGet != "/net/html/dopost")
			{
				strcpy(sendBuffer, "HTTP/1.1 404 Not Found\n");
				std::cout << "Invalid POST request" << std::endl;
			}
			else
			{
				//Extract uid and pwd
				std::string uid;
				std::string pwd;

				char contentLength[32];
				char content[1000];
				memset(contentLength, 0, 32);
				memset(content, 0, 1000);

				char *head = &recvBuffer[strlen(recvBuffer)], *end = &recvBuffer[strlen(recvBuffer)];

				while (*head != '&')head--;
				head += strlen("pass=");
				head++;
				pwd.append(head, 0, end - head);

				while (*head != '\n')head--;
				head += strlen("login=");
				head++;
				while (*end != '&')end--;
				uid.append(head, 0, end - head);

				if (uid == "3150104209" && pwd == "4209")
				{
					sprintf(content, "<html><body>Hello,%s</body></html>\n", uid.c_str());
					sprintf(sendBuffer, "HTTP/1.1 200 OK\nContent-Type: text/html;charset=gb2312\nContent-Length: %d\n\n", strlen(content));
				}
				else
				{
					strcpy(content, "<html><body>Wrong ID or Password</body></html>\n");
					sprintf(sendBuffer, "HTTP/1.1 200 OK\nContent-Type: text/html;charset=gb2312\nContent-Length: %d\n\n", strlen(content));
				}
				strcat(sendBuffer, content);
				ret = send(clientSocket, sendBuffer, BUF_SIZE, 0);
				if (ret == SOCKET_ERROR)
					std::cout << "send failed with error: %d\n" << WSAGetLastError() << std::endl;

			}
		}
	}

	//shutdown the connection
	ret = shutdown(clientSocket, SD_SEND);
	if (ret == SOCKET_ERROR)
		std::cout << "shutdown failed with error: "<< WSAGetLastError() <<std::endl;
	list_mtx.lock();
	for (iter = clients.begin(); iter != clients.end(); iter++)
	{
		if (iter->clientSocket == clientSocket)
		{
			closesocket(clientSocket);
			clients.erase(iter);
			break;
		}
	}
	list_mtx.unlock();
	std::cout << " Client IP: " << inet_ntoa(addr) << " PORT: " << port <<" Thread exit" << std::endl;
	return;

}

Server::Server(): serverSocket(INVALID_SOCKET)
{
	int ret;
	WSADATA wsaData;
	ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret != 0) {
		std::cout << "WSAStartup failed with error: " << ret << std::endl;
		return;
	}
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket == INVALID_SOCKET)
	{
		std::cout << "socket failed with error: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return;
	}

	ret = bind(serverSocket, (SOCKADDR *)&serverAddr, sizeof(SOCKADDR));
	if (ret == SOCKET_ERROR)
	{
		std::cout << "bind failed with error: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return;
	}
}

Server::~Server()
{

}


void Server::start()
{
	int ret;
	int i = 0;
	

	ret = listen(serverSocket, QUEUE_SIZE);
	if (ret == SOCKET_ERROR)
	{
		std::cout << "listen failed with error: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return;
	}

	//Handle the terminate request
	std::thread mainThread(HandleInput);
	
	int clntAddrLen = sizeof(SOCKADDR_IN);
	while (true)
	{
		clntInfo client;
		client.clientSocket = accept(serverSocket, (SOCKADDR *)&(client.clientAddr), &clntAddrLen);
		if (client.clientSocket == INVALID_SOCKET)
		{
			std::cout << "accpet failed with error: " << WSAGetLastError() << std::endl;
			//WSACleanup();
			//return;
		}
		if (serverEnd)
			closesocket(client.clientSocket);
		else
		{
			//Hold mutex lock
			list_mtx.lock();
			client.clientThread = new std::thread(HandleHttpRequest, client.clientSocket);
			clients.push_back(client);
			list_mtx.unlock();
		}

	
	}
}

