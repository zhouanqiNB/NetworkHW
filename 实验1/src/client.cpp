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

    //char* sname=new char;
    //char* cname=new char;
    char sname[6];
    char cname[6];
    cout<<"Type in your name (limited in 5 letters): ";
    cin>>cname;     //稍后传给服务器端我的名字




	WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);//*



    SOCKET sockClient = socket(AF_INET, SOCK_STREAM, 0);



    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));  //每个字节都用0填充
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1234);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  //具体的IP地址

    //inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr.S_un.S_addr);



    connect(sockClient, (SOCKADDR*)&server_addr, sizeof(SOCKADDR));

    /*********************连起来啦************************/



    cout<<"Connected successfully, you can send message now.\n";



    //这是为了获取对方的id信息（在程序开始的时候收集）
    send(sockClient, cname, 5, 0);    //先send因为经过我的测试先resv再send对方接收不到
    recv(sockClient, sname, 5, 0);    //经过测试成功获得了对方的id



	char* recvBuf=new char;
	char* sendBuf=new char;



    bool isExit = false;
    do {


        //client先发言
        cout << "\033[34m\033[1m" << "-----------------------------------------------------\n"<<"\033[0m" ;

        
        getTime(sendBuf);
        getTime(timestamp);


        bool firstToken=true;
        bool newLine=true;
        bool newToken=false;

        do {
            //轮到你发言的回合的时候，在你的光标前面会有一个尖头。
            if(newLine){
                cout<<"> ";
                newLine=false;
            }

            if(firstToken){
                cin>>sendBuf+14;
                firstToken=false;
            }
            else{
                cin>>sendBuf;
            }



            send(sockClient, sendBuf, BUFSIZE, 0);


            if (*(sendBuf+14) == '#') {
                send(sockClient, sendBuf, BUFSIZE, 0);
                *sendBuf = '*';
                isExit = true;
            }

        } while (*sendBuf != 42);

        cout<<"> ";

        //输出姓名时间戳以及下边框

        cout <<"\n"<< "\033[34m\033[1m";
        cout<<"@"<<cname;
        cout << "\033[0m" ;
        timePrint(timestamp,1);
        cout<<"\033[34m\033[1m-----------------------------------------------------\033[0m";

        cout<<endl;





        //接受server的消息

        firstToken=true;

        cout << "\033[32m\033[1m" << "-----------------------------------------------------\n"<< "\033[0m";

        do {    


            recv(sockClient, recvBuf, BUFSIZE, 0);

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

        } while (*recvBuf != 42);


        //输出姓名时间戳以及下边框

        cout <<"\n"<< "\033[32m\033[1m";
        cout<<"@"<<sname;
        cout << "\033[0m" ;
        timePrint(timestamp,0);
        cout<<"\033[32m\033[1m-----------------------------------------------------\033[0m"<<endl;

    } while (!isExit);

    //关闭连接
    delete[] sendBuf;
    delete[] recvBuf;
        
    closesocket(sockClient);
    WSACleanup();


    return 0;
}
