/**
 * 1911590 周安琪
 * 实验3-1
 * 服务端/接收端代码
 *
 * 设计思路：
 * 
 * 建立连接应该用socket就可以了，连接上了输出一个“conneted”应该就可以
 * 
 * 接收端自己需要一个定时器
 * 
 * 差错检测用校验和checkSum，双方都用校验和来判断是否出错
 * - 如果接收端发现校验和不对就再发一次ACK+lastNum，如果是对的就发ACK+nowNum
 * - 如果发送端发现校验和不对(ACK incorrect)/timeout就再发一次数据包，这会造成重传问题
 * - 如果接收端接到一个包发现这个序列号已经传过了，那就扔掉并且，再发一次ACK+nowNum
 * - 发送端正常接收到ACK就返回初始状态，下次被调用传下一个
 * 
 * 我想写一个用户选择传输文件的界面，这样就可以“传下一个”。
 * 
 * 
 */
#include <iostream>
#include <fstream>
#include <string.h>
#include <ctime>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#include<cmath>
using namespace std;

const int BUFFER_SIZE = 0xFFFF;
const int HEAD_SIZE=16;
#define DATA_SIZE (BUFFER_SIZE-HEAD_SIZE)


char sendBuffer[BUFFER_SIZE];
char recvBuffer[BUFFER_SIZE];

//-------------------------
string fileName;
int fileLength;
sockaddr_in serveraddr, clientaddr;
//-------------------------

int sourcePort=1235;
int destinationPort=1234;
int sequenceNumber;
int ackNumber;
int checkSum;

int expectedNum=0;

ofstream fout;

// setters
void setPort(){
    // 与0xff是为了把高过8位的都清空。
    sendBuffer[1]=(sourcePort>>8)&0xff;
    sendBuffer[2]=sourcePort&0xff;
    sendBuffer[3]=(destinationPort>>8)&0xff;
    sendBuffer[4]=destinationPort&0xff;
}
void setSeqNum(int& num){
    // 如果序列号超出范围，取模。
    if(num>0xffffffff){
        num%=0xffffffff;
    }
    sendBuffer[4]=(num>>24)&0xff;
    sendBuffer[5]=(num>>16)&0xff;
    sendBuffer[6]=(num>>8)&0xff;
    sendBuffer[7]=num&0xff;
}
void setAckNum(int num){
    if(num>0xffffffff){
        num%=0xffffffff;
    }
    sendBuffer[8]=(num>>24)&0xff;
    sendBuffer[9]=(num>>16)&0xff;
    sendBuffer[10]=(num>>8)&0xff;
    sendBuffer[11]=num&0xff;
}
void setSize(int num){
    // 在这里放了文件名长度，最多15，本来应该是放option长度的。
    if(num>>8){
        cout<<"+-------------------------------------------------+\n";
        cout<<"| This fileName is too long! We cannot handle it! |\n";
        cout<<"+-------------------------------------------------+\n";
    }
    sendBuffer[12] = (char)(num & 0xff);
}
void setAckBit(char a){
    if(a==0){
        // 1110 1111
        sendBuffer[13] &=0xef;
    }
    else{
        // 0001 0000
        sendBuffer[13] |=0x10;
    }
}
void setSynBit(char a){
    if(a==0){
        // 1111 1101
        sendBuffer[13] &=0xfd;
    }
    else{
        // 0000 0010
        sendBuffer[13] &=0x02;
    }
}
void setFinBit(char a){
    if(a==0){
        // 1111 1110
        sendBuffer[13] &=0xfe;
    }
    else{
        // 0000 0001
        sendBuffer[13] &=0x01;
    }
}
// 计算校验和
u_short cksum(u_short* buf) {
    int count=HEAD_SIZE;
    register u_long sum = 0;
    while (count--) {
        sum += *buf;
        buf++;
        // 如果校验和不止16bit，把高位清0，最低位加一。
        if (sum & 0xffff0000) {
            sum &= 0xffff;
            sum++;
        }
    }
    return ~(sum & 0xffff); //取反
}
void setCheckSum(){
    sendBuffer[15] = sendBuffer[16] = 0; // 校验和清零
    u_short x = cksum((u_short*)&sendBuffer[0]); //计算校验和
    cout<<"checkSum is "<<x<<endl;
    sendBuffer[15] = (char)((x >> 8) % 0x100);
    sendBuffer[16] = (char)(x % 0x100);
}

// getters
int getSeqNum(){
    int a=(recvBuffer[4]<<24)+(recvBuffer[5]<<16)+(recvBuffer[6]<<8)+recvBuffer[7];
    return a;
}
int getAckNum(){
    int a=(recvBuffer[8]<<24)+(recvBuffer[9]<<16)+(recvBuffer[10]<<8)+recvBuffer[11];
    return a;
}
int getSize(){
    int a=recvBuffer[12]*0x1;
    return a;
}
int getAckBit(){
    // 0001 0000
    return sendBuffer[13] & 0x10;
}
int getSynBit(){
    // 0000 0010
    return recvBuffer[13]&=0x2;
}


// 检验头加起来是不是0xffff
bool isCorrupt() {
    u_short now_cksum = cksum((u_short*)&sendBuffer[0]);
    return now_cksum != 0xffff;
}

void packSynDatagram(int sequenceNumber){
    setPort();
    setSeqNum(sequenceNumber);
    setSynBit(1);
    setCheckSum();
}







void getFileName();

void printBindingErr();
void printLibErr();
void printCreateSocketErr();
void printFileErr();

void shakeHand();
void printRTOErr();


int main() {
    string clientAddr="127.0.0.1";
    string serverAddr="127.0.0.1";



// 加载库文件
    WORD wVersionRequested = MAKEWORD(2, 0);
    WSADATA wsaData;
    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
        printLibErr();
    }
    else cout << "We successfully loaded the libs." << endl;
// 加载库文件    

    SOCKET ser_socket = socket(AF_INET, SOCK_DGRAM, 0);

    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(1234);

    // 设置定时器的时间
        // rdt3.0，ms
        struct timeval timeout;
        timeout.tv_sec = 5000;
        timeout.tv_usec = 0;

        setsockopt(ser_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        cout << "We successfully set the RTO." << endl;
    // 设置定时器的时间

    int ercode = bind(ser_socket, (SOCKADDR*)&serveraddr, sizeof(sockaddr));
    if (ercode != 0) {
        printBindingErr();
        return -1;
    }
    else cout << "We successfully bind the addr to socket." << endl;

    cout<<"listening....\n";

    // recvfrom & sendto
    int tot = 0;
    // while (1) {
    // 接收SYN
        int sockaddrInLength=sizeof(sockaddr_in);
        while(recvfrom(ser_socket, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&clientaddr, &sockaddrInLength)==SOCKET_ERROR){
             if (WSAGetLastError() == 10060)     printRTOErr();
        };
        cout<<"received."<<endl;

        bool isCorrupted = isCorrupt(); //查看校验和
        while (isCorrupted) {
            // 再发一次上次的ack。
            sendto(ser_socket, sendBuffer, sizeof(sendBuffer), 0, (SOCKADDR*)&clientaddr, sockaddrInLength);
            cout<<"this package is corrupted."<<endl;

            // recieve a retransmission(maybe redundant) datagram
            recvfrom(ser_socket, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&clientaddr, &sockaddrInLength);
            isCorrupted = isCorrupt();
        }
    // 接收SYN

        cout<<getSynBit()<<endl;
        if(getSynBit()){
            cout << "Got a SYN datagram." << endl;
            setPort();
            setAckBit(1);
            setSynBit(1);
            setCheckSum();
        }
        else{
            cout << "Got a file datagram." << endl;
            setPort();
            expectedNum=getSeqNum()+1;
            setAckNum(expectedNum);
            setAckBit(1);
            setSynBit(0);
        }

        sendto(ser_socket, sendBuffer, sizeof(sendBuffer), 0, (SOCKADDR*)&clientaddr, sockaddrInLength);
        memset(sendBuffer, 0, sizeof(sendBuffer));

        // if the datagram is a syn
        if (getSynBit()) {
            // continue;
        }

        // if the seq not fits
        if (getSeqNum()!=expectedNum) {
            cout << "Not the expectedNum." << endl;
            // continue;
        }
        
        // analyze();
    // }

    closesocket(ser_socket);
    WSACleanup();
    cout<<"closed."<<endl;
}

void getFileName(){
    cout<<"Tell me which file you want to send.\n";
    cin>>fileName;
}

void printBindingErr(){
    cout << "+-------------------------------+\n" << endl;
    cout << "| We met problems when binding. |\n" << endl;
    cout << "+-------------------------------+\n" << endl;
}

void printLibErr(){
    cout << "+------------------------------------+\n" ;
    cout << "| We met problems when loading libs. |\n" ;
    cout << "+------------------------------------+\n" ;            
}

void printCreateSocketErr(){
    cout << "+--------------------------------+\n" ;
    cout << "| We cannot create a new socket. |\n" ;
    cout << "+--------------------------------+\n" ;            
}

void printFileErr(){
    cout << "+--------------------------------+\n";
    cout << "| Sorry we cannot open the file. |\n";
    cout << "+--------------------------------+\n";                
}


void printRTOErr(){
    cout << "+----------------------+\n";
    cout << "| Over RTO. No client. |\n";
    cout << "+----------------------+\n";
}