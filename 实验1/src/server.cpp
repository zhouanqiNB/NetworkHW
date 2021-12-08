#include <stdio.h>
#include <stdlib.h>
#include <WinSock2.h>
#include <iostream>
#include <Ws2tcpip.h>
#include <string.h>
#include <time.h>
#include"timeStamp.h"

#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
using namespace std;

#define BUFSIZE 100

int main()
{

	char timestamp[20];


	char sname[6];
	char cname[6];
    cout<<"Type in your name (limited in 5 letters): ";
	cin>>sname;		//稍后传给客户端我的名字




	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);



	SOCKET sockSrv = socket(AF_INET, SOCK_STREAM, 0);



	struct sockaddr_in server_addr;

	memset(&server_addr, 0, sizeof(server_addr));  //每个字节都用0填充
	server_addr.sin_family = AF_INET;  //使用IPv4地址
	server_addr.sin_port = htons(1234);  //端口
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  //具体的IP地址
	//inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr.S_un.S_addr);



	bind(sockSrv, (SOCKADDR*)&server_addr, sizeof(SOCKADDR));

	listen(sockSrv, 10);

//--------------------------------------------------------------------------------------
	
	bool firstSend=true;
	while (1) {

		bool isExit = false;


		SOCKADDR clntAddr;
		int nSize = sizeof(SOCKADDR);
		SOCKET sockConn = accept(sockSrv, (SOCKADDR*)&clntAddr, &nSize);


		/*********************连起来啦************************/

		cout<<"Connected successfully, waiting for the client.\n";

		//这是为了获取对方的id信息（在程序开始的时候收集）
		if(firstSend){
			send(sockConn, sname, 5, 0);
		    recv(sockConn, cname, 5, 0);	//经过测试成功获得了对方的id
			firstSend=false;
		}




		//chatting............

		//第一句是client发言，所以先recv

		bool firstToken=true;		

		char* recvBuf=new char;
		char* sendBuf=new char;


    	cout << "\033[34m\033[1m" << "-----------------------------------------------------\n"<<"\033[0m" ;
		//为啥写个循环呢，是因为好像读到空格就不读了。。。
		do {
			recv(sockConn, recvBuf, BUFSIZE, 0);

			if(firstToken){
				strcpy(timestamp,recvBuf);
                cout<<recvBuf+14<<' ';
                firstToken=false;
            }
            else{
                cout<<recvBuf<<' ';
            }


			if (*(recvBuf+14) == '#') {
				*recvBuf = '*';
				isExit = true;
			}

		} while (*recvBuf != '*');

		//输出姓名时间戳以及下边框

    	cout <<"\n"<< "\033[34m\033[1m";
    	cout<<"@"<<cname;
    	cout << "\033[0m" ;
    	timePrint(timestamp,1);
    	cout<<"\033[34m\033[1m-----------------------------------------------------\033[0m";

    	cout<<endl;




		//client结束之后server发言

		do {
			cout << "\033[32m\033[1m" << "-----------------------------------------------------\n"<< "\033[0m";

			//给发过去的消息加上时间戳。

			getTime(sendBuf);
			strcpy(timestamp,sendBuf);


			firstToken=true;
			bool newLine=true;
			bool newToken=false;
			

			//只要没有写结束符，就一直可以发
			do {
				if(newLine){
					cout<<"> ";
					newLine=false;
				}

				//不需要每次都发送时间戳，只在这一次开始就可以了
				if(firstToken){
					cin>>sendBuf+14;
					firstToken=false;
				}
				else{
					cin>>sendBuf;
				}


				send(sockConn, sendBuf, BUFSIZE, 0);


				if (*(sendBuf+14) == '#') {
					send(sockConn, sendBuf, BUFSIZE, 0);
					*sendBuf = '*';
					isExit = true;
				}

			} while (*sendBuf != '*');

			cout<<"> ";
			//输出姓名时间戳以及下边框

	    	cout <<"\n"<< "\033[32m\033[1m";
	    	cout<<"@"<<sname;
	    	cout << "\033[0m" ;
	    	timePrint(timestamp,0);
	    	cout<<"\033[32m\033[1m-----------------------------------------------------\033[0m"<<endl;





			firstToken=true;

			//直到接收了结束符停止。
    		cout << "\033[34m\033[1m" << "-----------------------------------------------------\n"<< "\033[0m";
			do {
				recv(sockConn, recvBuf, BUFSIZE, 0);

	            if(firstToken){
	            	strcpy(timestamp,recvBuf);
	                cout<<recvBuf+14<<' ';
	                firstToken=false;
	            }
	            else{
	                cout<<recvBuf<<' ';
	            }


	            if (*(recvBuf+14) == '#') {
	                *recvBuf = '*';
	                isExit = true;
	            }

			} while (*recvBuf != '*');


			//输出姓名时间戳以及下边框

	    	cout <<"\n"<< "\033[34m\033[1m";
	    	cout<<"@"<<cname;
	    	cout << "\033[0m" ;
	    	timePrint(timestamp,1);
	    	cout<<"\033[34m\033[1m-----------------------------------------------------\033[0m"<<endl;


		} while (!isExit);




		delete[] sendBuf;
		delete[] recvBuf;



		closesocket(sockConn);
		isExit = false;
		exit(1);

	}


	closesocket(sockSrv);
	WSACleanup();

	return 0;
}
