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


#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <time.h>
#include <Winsock2.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <io.h>

#define BUFFER_SIZE 0xf000
#define HEAD_SIZE 0x10
#define DATA_SIZE (BUFFER_SIZE-HEAD_SIZE)

#pragma comment(lib, "ws2_32.lib")
using namespace std;



char sendBuffer[BUFFER_SIZE];
char recvBuffer[BUFFER_SIZE];

int sourcePort=1234;
int destinationPort=1235;
int sequenceNumber=0;
bool fin=false;


string fileName;
int fileLength;

ofstream fout;


void setPort();
void setSeqNum(int& num);
void setAckNum(int num);
void setSize(int num);
void setAckBit(char a);
void setSynBit(char a);
void setFinBit(char a);
unsigned short calCheckSum(unsigned short* buf) ;//计算校验和
void setCheckSum();


// getters 之所以传参char* 是因为也要输出sendBuffer.
class Getter{
public:
    int getSeqNum(char* recvBuffer){
        int a=(recvBuffer[7]<<24)+(recvBuffer[6]<<16)+(recvBuffer[5]<<8)+recvBuffer[4];
        return a;
    }
    int getAckNum(char* recvBuffer){
        int a=(recvBuffer[11]<<24)+(recvBuffer[10]<<16)+(recvBuffer[9]<<8)+recvBuffer[8];
        return a;
    }
    int getSize(char* recvBuffer){
        int a=recvBuffer[12];
        return a;
    }
    int getAckBit(char* recvBuffer){
        // 0001 0000
        int a=(recvBuffer[13] & 0x10)>>4;
        return a;
    }
    int getSynBit(char* recvBuffer){
        // 0000 0010
        int a=(recvBuffer[13] & 0x2)>>1;
        return a;
    }
    int getFinBit(char* recvBuffer){
        // 0000 0010
        int a=(recvBuffer[13] & 0x1);
        return a;
    }
    unsigned short getCheckSum(char* recvBuffer){
        int a=(recvBuffer[15]<<8)+recvBuffer[14];
        return a;
    }
} getter;



void packSynDatagram(int sequenceNumber);
void packSynAckDatagram();
void packAckDatagram(int expectedNum);
void packAckFin(int expectedNum);
void packEmptyDatagram();

void printLogSendBuffer();
void printLogRecvBuffer();

bool checkSumIsRight();

void getFileName();

void printBindingErr();
void printLibErr();
void printCreateSocketErr();
void printFileErr();
void printRTOErr();


int main(){

// 设置套接字###############################################################################
    //加载套接字库
    WSADATA wsaData;
    WSAStartup(MAKEWORD(1, 1), &wsaData);

    //创建用于监听的套接字
    SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, 0);//失败会返回 INVALID_SOCKET
    SOCKADDR_IN addrSrv;
    addrSrv.sin_addr.s_addr = inet_addr("127.0.0.1");//输入你想通信的她（此处是本机内部）
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(1366);

    //绑定套接字, 绑定到端口
    bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR));//会返回一个SOCKET_ERROR

    SOCKADDR_IN addrClient;   //用来接收客户端的地址信息
    int len = sizeof(SOCKADDR);
    cout<<"listening..."<<endl;
// 设置套接字###############################################################################


// 接收报文#################################################################################

    int expectedNum=0;

    //建立连接，接收数据，判断数据校验和，回应报文
    while (1){
        recvfrom(sockSrv, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&addrClient, &len);
        cout<<"received."<<endl;
        printLogRecvBuffer();

    // 如果是SYN报文#######################################################################
        if(getter.getSynBit(recvBuffer)){
            cout<<"Got an SYN datagram!"<<endl;
            
            //如果校验和没问题就返回一个SYN ACK报文，否则返回空报文
            if(checkSumIsRight()){
                // SYN报文协商起始的序列号。
                expectedNum=getter.getSeqNum(recvBuffer);
                setAckNum(getter.getSeqNum(recvBuffer));
                packSynAckDatagram();
                sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrClient, len);
                printLogSendBuffer();
                cout<<"Sent SYN ACK."<<endl;
            }
            else{
                packEmptyDatagram();
                sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrClient, len);
                printLogSendBuffer();
                cout<<"sent."<<endl;
            }
        }
    // 如果是SYN报文#######################################################################


    // 如果是普通数据报文###################################################################
        else{
            cout<<"Got an File datagram!"<<endl;

            if(checkSumIsRight()){
                //如果校验和没问题就返回一个ACK报文+expected序列号，否则上次的ACK再发一遍
                //（这里是默认SYN只发一遍，之后的全部都是File）

                // 如果是第一个
                int fileNameLength=0;
                if(getter.getSize(recvBuffer)!=0){
                    fileName="";
                    fileNameLength=getter.getSize(recvBuffer);

                    for(int i=0;i<getter.getSize(recvBuffer);i++){
                        fileName+=recvBuffer[HEAD_SIZE+i];
                    }
                    cout<<"fileName: "<<fileName<<endl;
                    fout.open(fileName,ios_base::out | ios_base::app | ios_base::binary);
                }

                // 如果发过来的序列号正是想要的，就++，否则再发一遍之前的。
                if(expectedNum==getter.getSeqNum(recvBuffer)){
                    expectedNum++;
                    if(expectedNum>=128){
                        expectedNum%=128;
                    }
                    int len = 0;

                    fout.write(&recvBuffer[HEAD_SIZE + fileNameLength],DATA_SIZE);
                
                }
                else cout<<"Not expectedNum!"<<endl;

                packAckDatagram(expectedNum);

                if(getter.getFinBit(recvBuffer)){//如果收到了fin，挥手。
                    setFinBit(1);
                    setCheckSum();
                    fout.close();
                    cout<<"file receiving ends."<<endl;
                }

                sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrClient, len);
                printLogSendBuffer();
                cout<<"sent."<<endl;
            }

            else{
                sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrClient, len);
                printLogSendBuffer();
                cout<<"sent."<<endl;
            }
        }
    // 如果是普通数据报文###################################################################
    
    }
// 接收报文################################################################################


// 结束任务################################################################################
    closesocket(sockSrv);
    WSACleanup();
    return 0;
// 结束任务################################################################################
}

void setPort(){
    // 与0xff是为了把高过8位的都清空。
    sendBuffer[1]=(sourcePort>>8)&0xff;
    sendBuffer[0]=sourcePort&0xff;
    sendBuffer[3]=(destinationPort>>8)&0xff;
    sendBuffer[2]=destinationPort&0xff;
}

void setSeqNum(int& num){
    // 如果序列号超出范围，取模。
    if(num>0xffffffff){
        num%=0xffffffff;
    }
    sendBuffer[7]=(num>>24)&0xff;
    sendBuffer[6]=(num>>16)&0xff;
    sendBuffer[5]=(num>>8)&0xff;
    sendBuffer[4]=num&0xff;
}

void setAckNum(int num){
    if(num>0xffffffff){
        num%=0xffffffff;
    }
    sendBuffer[11]=(num>>24)&0xff;
    sendBuffer[10]=(num>>16)&0xff;
    sendBuffer[9]=(num>>8)&0xff;
    sendBuffer[8]=num&0xff;
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
        sendBuffer[13] |=0x2;
    }
}

void setFinBit(char a){
    if(a==0){
        // 1111 1110
        sendBuffer[13] &=0xfe;
    }
    else{
        // 0000 0001
        sendBuffer[13] |=0x01;
    }
}

unsigned short calCheckSum(unsigned short* buf) {
    int count=HEAD_SIZE/2;
    register unsigned long sum = 0;
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
    sendBuffer[14] = sendBuffer[15] = 0; // 校验和清零
    unsigned short x = calCheckSum((unsigned short*)&sendBuffer[0]); //计算校验和
    sendBuffer[15] = (char)((x >> 8) % 0x100);
    sendBuffer[14] = (char)(x % 0x100);
}

void packSynDatagram(int sequenceNumber){
    setPort();
    setSeqNum(sequenceNumber);
    setSynBit(1);
    setAckBit(0);
    setFinBit(0);
    setCheckSum();
}

void packSynAckDatagram(){
    setPort();
    setSeqNum(sequenceNumber);
    setSynBit(1);
    setAckBit(1);
    setFinBit(0);
    setCheckSum();
}

void packAckDatagram(int expectedNum){
    setPort();
    setSeqNum(sequenceNumber);
    setAckNum(expectedNum);
    setSynBit(0);
    setAckBit(1);
    setFinBit(0);
    setCheckSum();
}

void packAckFin(int expectedNum){
    setPort();
    setSeqNum(sequenceNumber);
    setAckNum(expectedNum);
    setSynBit(0);
    setAckBit(1);
    setFinBit(1);
    setCheckSum();
}

void packEmptyDatagram(){
    setPort();
    setSeqNum(sequenceNumber);
    setSynBit(0);
    setAckBit(0);
    setFinBit(0);
    setCheckSum();
}

void printLogSendBuffer(){
    cout<<"sendBuffer: ";
    cout<<"SeqNum: "<<getter.getSeqNum(sendBuffer)<<", ";
    cout<<"AckNum: "<<getter.getAckNum(sendBuffer)<<", ";
    cout<<"Size: "<<getter.getSize(sendBuffer)<<", ";
    cout<<"SYN: "<<getter.getSynBit(sendBuffer)<<", ";
    cout<<"ACK: "<<getter.getAckBit(sendBuffer)<<", ";
    cout<<"FIN: "<<getter.getFinBit(sendBuffer)<<", ";
    cout<<"CheckSum: "<<getter.getCheckSum(sendBuffer)<<endl;
}

void printLogRecvBuffer(){
    cout<<"recvBuffer: ";
    cout<<"SeqNum: "<<getter.getSeqNum(recvBuffer)<<", ";
    cout<<"AckNum: "<<getter.getAckNum(recvBuffer)<<", ";
    cout<<"Size: "<<getter.getSize(recvBuffer)<<", ";
    cout<<"SYN: "<<getter.getSynBit(recvBuffer)<<", ";
    cout<<"ACK: "<<getter.getAckBit(recvBuffer)<<", ";
    cout<<"FIN: "<<getter.getFinBit(recvBuffer)<<", ";
    cout<<"CheckSum: "<<getter.getCheckSum(recvBuffer)<<endl;
}

bool checkSumIsRight(){
    unsigned short* buf=(unsigned short*)&recvBuffer[0];
    register unsigned long sum = 0;
    // 把每个字节加一遍
    for(int i=0;i<HEAD_SIZE/2;i++){
        sum += *buf;
        // cout<<*buf<<" ";
        buf++;
        // 如果校验和不止16bit，把高位清0，最低位加一。
        if (sum & 0xffff0000) {
            // cout<<"把高位清零\n";
            sum &= 0xffff;
            sum++;
        }
    }

    // 全加起来应该是0xffff

    // if(sum==0xffff){
    //     cout<<"CheckSum: "<<sum<<", ";
    //     cout<<"CheckSum right!"<<endl;
    // }
    // else{
    //     cout<<"CheckSum: "<<sum<<", ";
    //     cout<<"CheckSum wrong!"<<endl;
    // }
    return sum==0xffff;
}

void getFileName(){
    cout<<"Tell me which file you want to send.\n";
    cin>>fileName;
}

void printBindingErr(){
    cout << "+-------------------------------+\n" ;
    cout << "| We met problems when binding. |\n" ;
    cout << "+-------------------------------+\n" ;
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
    cout << "+------------------------------------------+\n";
    cout << "| Over RTO. The server did not respond us. |\n";
    cout << "+------------------------------------------+\n";
}