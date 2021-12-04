#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#include<iostream>
#include <winsock2.h>
#include<time.h>
#include <Winsock2.h>
#include <stdio.h>
#include<fstream>
#include <string>
#include <vector>
#include <io.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

#define BUFFER_SIZE 0xFFFF
#define SEND_TOP 0x1C


char sendBuf[BUFFER_SIZE];
char recvBuf[BUFFER_SIZE];

int main()
{
	//加载套接字库
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD(1, 1);

	err = WSAStartup(wVersionRequested, &wsaData);//错误会返回WSASYSNOTREADY
	if (err != 0)
	{
		return 1;
	}

	if (LOBYTE(wsaData.wVersion) != 1 || HIBYTE(wsaData.wVersion) != 1)      //高字节为副版本
	{
		WSACleanup();
		return 1;
	}

	printf("server is operating!\n\n");
	//创建用于监听的套接字
	SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, 0);//失败会返回 INVALID_SOCKET
	SOCKADDR_IN addrSrv;

	addrSrv.sin_addr.s_addr = inet_addr("127.0.0.1");//输入你想通信的她（此处是本机内部）
	addrSrv.sin_family = AF_INET;
	addrSrv.sin_port = htons(1234);


	//绑定套接字, 绑定到端口
	bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR));//会返回一个SOCKET_ERROR


	SOCKADDR_IN addrClient;   //用来接收客户端的地址信息
	int len = sizeof(SOCKADDR);

	int i=1;

	while (i--)
	{
		recvfrom(sockSrv, recvBuf, sizeof(recvBuf), 0, (SOCKADDR*)&addrClient, &len);

		cout << "received." << endl;

	}


	//接收数据报文

	closesocket(sockSrv);
	WSACleanup();
	return 0;
}