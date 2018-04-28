#include<iostream>
#include<thread>
#include<winsock2.h>
#include<list>
#include<mutex>
#include<atomic>
#define BUF_SIZE 10240
#define SERVER_PORT 4209
#define QUEUE_SIZE 10

struct clntInfo
{
	clntInfo() :clientThread(NULL) {};
	SOCKET clientSocket;
	SOCKADDR_IN clientAddr;
	std::thread *clientThread;
};

class Server
{
public:
	Server();
	~Server();

	void start();

private:

	SOCKET serverSocket;
	SOCKADDR_IN serverAddr;

	char buf[BUF_SIZE];   

	friend void HandleHttpRequest(SOCKET);
};
