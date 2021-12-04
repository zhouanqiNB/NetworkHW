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

#define BUFFER_SIZE 0x4000
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

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		return 1;
	}

	if (LOBYTE(wsaData.wVersion) != 1 ||     //低字节为主版本
		HIBYTE(wsaData.wVersion) != 1)      //高字节为副版本
	{
		WSACleanup();
		return 1;
	}

	printf("Client is operating!\n\n");
	//创建用于监听的套接字
	SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, 0);


	SOCKADDR_IN  addrServer;
	addrServer.sin_addr.s_addr = inet_addr("127.0.0.1");//输入你想通信的她（此处是本机内部）
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(1234);

	SOCKADDR_IN  addrClient;
	int len = sizeof(SOCKADDR);

	//将IP，port，seq=0，SYN=1，ACK=0

	int i=1;

	while (i--)
	{
		//发送第一个报文连接，计算校验和

		sendto(sockSrv, sendBuf, sizeof(sendBuf), 0, (sockaddr*)&addrServer, len);
		cout << "sent" << endl;


	}


	closesocket(sockSrv);
	WSACleanup();
	return 0;
}


//UdpNetClient.cpp

//#define _WINSOCK_DEPRECATED_NO_WARNINGS
//#include <Winsock2.h>
//#include <stdio.h>
//#pragma comment(lib, "ws2_32.lib")
//
//void main()
//{
//	//加载套接字库
//	WORD wVersionRequested;
//	WSADATA wsaData;
//	int err;
//
//	wVersionRequested = MAKEWORD(1, 1);
//
//	err = WSAStartup(wVersionRequested, &wsaData);
//	if (err != 0)
//	{
//		return;
//	}
//
//	if (LOBYTE(wsaData.wVersion) != 1 ||     //低字节为主版本
//		HIBYTE(wsaData.wVersion) != 1)      //高字节为副版本
//	{
//		WSACleanup();
//		return;
//	}
//
//	printf("Client is operating!\n\n");
//	//创建用于监听的套接字
//	SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, 0);
//
//	sockaddr_in  addrSrv;
//	addrSrv.sin_addr.S_un.S_addr = inet_addr("127.0.0.2");//输入你想通信的她（此处是本机内部）
//	addrSrv.sin_family = AF_INET;
//	addrSrv.sin_port = htons(6000);
//
//
//	int len = sizeof(SOCKADDR);
//
//	char recvBuf[100];    //收
//	char sendBuf[100];    //发
//	char tempBuf[100];    //存储中间信息数据
//
//	while (1)
//	{
//
//		printf("Please input data: \n");
//		gets_s(sendBuf);
//		sendto(sockSrv, sendBuf, strlen(sendBuf) + 1, 0, (SOCKADDR*)&addrSrv, len);
//		//等待并数据
//		recvfrom(sockSrv, recvBuf, 100, 0, (SOCKADDR*)&addrSrv, &len);
//
//		if ('q' == recvBuf[0])
//		{
//			sendto(sockSrv, "q", strlen("q") + 1, 0, (SOCKADDR*)&addrSrv, len);
//			printf("Chat end!\n");
//			break;
//		}
//		sprintf_s(tempBuf, "%s say : %s", inet_ntoa(addrSrv.sin_addr), recvBuf);
//		printf("%s\n", tempBuf);
//
//		//发送数据
//
//	}
//	closesocket(sockSrv);
//	WSACleanup();
//}