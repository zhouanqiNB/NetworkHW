/**
 * 1911590 周安琪
 * 实验3-1
 * 客户端/发送端代码
 */

/**
 * 
 * TCP报文头的格式，共20Bytes
 * 
 * +------------------------+------------------------+
 * |    2Byte sourcePort    |  2Byte destinationPort | 没有源IP和目的IP，因为那是IP层协议的事情
 * +------------------------+------------------------+
 * |                  sequenceNumber                 | 在SYN报文中，序列号用于交换彼此的初始序列号ISN；其他，序列号用于保证包的顺序。
 * +-------------------------------------------------+
 * |                    ackNumber                    | 在ACK报文中，这里填上已经收到的最新的序列号
 * +----+-------+-+-+-+-+-+-+------------------------+
 * |size|       |U|A|P|R|S|F|     recvWindowSize     | size是说option的长度，所以只有4位。
 * +----+-------+-+-+-+-+-+-+------------------------+
 * |        checkSum        |     ptrErgentData      |
 * +------------------------+------------------------+
 * |                     options                     |
 * +-------------------------------------------------+
 * 
 * 在本次实验中用不上recvWindowSize，也用不上ptrErgentData，所以可以缩减为16Bytes
 * 
 * +------------------------+------------------------+
 * |    2Byte sourcePort    |  2Byte destinationPort |
 * +------------------------+------------------------+
 * |                  sequenceNumber                 |
 * +-------------------------------------------------+
 * |                    ackNumber                    |
 * +---------+--+-+-+-+-+-+-+------------------------+
 * |  size   |  |U|A|P|R|S|F|       checkSum         |
 * +---------+--+-+-+-+-+-+-+------------------------+
 * |                   bufferSize                    |
 * +-------------------------------------------------+
 * 
 * 这里的size没用上，所以用来放文件名的长度，设置了8bit所以可以表示2^8个数字。
 * 
 * 
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
#include <windows.h>
#include <ctime>
#include <io.h>
#include <cmath>

#define BUFFER_SIZE 0x3000
#define HEAD_SIZE 0x14
#define DATA_SIZE (BUFFER_SIZE-HEAD_SIZE)

#pragma comment(lib, "ws2_32.lib")
using namespace std;

const int WINDOW_SIZE=22;

ofstream ccout;
char sendBuffer[BUFFER_SIZE];
char recvBuffer[BUFFER_SIZE];

int sourcePort=1234;
int destinationPort=1235;
unsigned int sequenceNumber=0;

string fileName="1.jpg";    //文件名称  
int fileLength;     //文件长度，用于计算sendTimes和最后一个包的大小。

bool isFirstPackage=true;   //如果是第一个包需要发名称

long bytesHaveSent=0;   //一共发送了多少字节，用于统计
int bytesHaveWritten=0; //加到bytesHaveSent里面，用于统计
int t_start;    //开始发送文件的时间，用于统计

ifstream fin;   //用于读文件
int bytesHaveRead=0;  //已经读到文件的哪里，用于读文件
int leftDataSize;   //这个包还剩多少DATA空间，用于读文件

int nowTime=0;    //现在已经有多少个被成功ack了，用于判断何时停止
int sendTimes;  //这个文件需要发送多少次

void setPort();
void setSeqNum(unsigned int num);
void setAckNum(unsigned int num);
void setSize(int num);
void setAckBit(char a);
void setSynBit(char a);
void setFinBit(char a);
unsigned short calCheckSum(unsigned short* buf) ;// 计算校验和
void setCheckSum();
void setBufferSize(unsigned int num);

// 用于设置套接字
void makeSocket();

void setRTO();
void findFile();


class Getter{
public:
    unsigned int getSeqNum(char* recvBuffer){
        unsigned int a=((recvBuffer[7]&0xff)<<24)+((recvBuffer[6]&0xff)<<16)+((recvBuffer[5]&0xff)<<8)+(recvBuffer[4]&0xff);
        return a&0xffff;
    }
    unsigned int getAckNum(char* recvBuffer){
        unsigned int a=((recvBuffer[11]&0xff)<<24)+((recvBuffer[10]&0xff)<<16)+((recvBuffer[9]&0xff)<<8)+(recvBuffer[8]&0xff);
        return a&0xffff;
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
    unsigned int getBufferSize(char* recvBuffer){
        unsigned int a=((recvBuffer[19]&0xff)<<24)+((recvBuffer[18]&0xff)<<16)+((recvBuffer[17]&0xff)<<8)+(recvBuffer[16]&0xff);
        return a&0xffff;
    }
} getter;


void packSynDatagram(int sequenceNumber);
void packFirst();
void packData();

void printLog(char*);

bool checkSumIsRight();

void getFileName();

void printFileErr();
void printRTOErr();


// 一个格子对应着一个序列号和一个状态
class SendGrid{
public:
    /**
     * state==0：这个窗口没被使用
     * state==1：这个窗口被占用了，已经发送了，但没收到ack，收到ack后转为2，
     *           超时还没有收到ack就重传。如果转2的那个窗口是最左侧的，移动滑动窗口。
     * state==2：发了而且收到ack了，只要左侧的都好了就可以提交并且清空内容。
     */
    int state;  //用于判断格子的状态
    int seq;    //格子的序列号是
    char buffer[BUFFER_SIZE];   //此序列号的sendbuffer
    clock_t start;//这个包被发出去的时间
    SendGrid():state(0),seq(-1){};
    void setBuffer(char* sendBuffer){
        for(int i=0;i<BUFFER_SIZE;i++){
            buffer[i]=sendBuffer[i];
        }
    }
};

// 一个窗口大小是16
class SendWindow{
public:
    SendGrid sendGrid[WINDOW_SIZE];
    // 窗口向右移动1位
    void move(){
        if(sendGrid[0].state==2){//如果最左侧是不是已经ack了
            // cout<<"buffer: ";
            // printLog(sendGrid[0].buffer);
            // printWindow();
            for(int i=1;i<WINDOW_SIZE;i++){
                sendGrid[i-1].state=sendGrid[i].state;
                sendGrid[i-1].seq++;

                for(int j=0;j<BUFFER_SIZE;j++){
                    sendGrid[i-1].buffer[j]=sendGrid[i].buffer[j];
                }
            }
            sendGrid[WINDOW_SIZE-1].state=0;//最右边的格子重新空闲
            sendGrid[WINDOW_SIZE-1].seq++;
            // printWindow();
            if(sendGrid[0].seq==sendTimes){
                ccout<<WINDOW_SIZE<<"\n";
                int t=clock()-t_start;
                ccout<<bytesHaveSent<<",";
                ccout<<t<<",";

                ccout<<(long long)bytesHaveSent * (long long)8 / (long long)t * (long long)CLOCKS_PER_SEC<<"\n";


                exit(0);
            }
            if(sendGrid[0].state==2)//如果最左侧还是已经ack了，继续move
                this->move();
        }
    }
    // 用于debug
    void printWindow(){
        for(int i=0;i<WINDOW_SIZE;i++){
            cout<<"|"<<i<<"\t";
        }
        cout<<endl;
        for(int i=0;i<WINDOW_SIZE;i++){
            cout<<"+-------";
        }
        cout<<endl;
        for(int i=0;i<WINDOW_SIZE;i++){
            cout<<"|"<<sendGrid[i].state<<"\t";
        }
        cout<<endl;
        for(int i=0;i<WINDOW_SIZE;i++){
            cout<<"+-------";
        }
        cout<<endl;
    }
} win;


SOCKET sockSrv;
SOCKADDR_IN  addrServer;
SOCKADDR_IN  addrClient;
int len;

struct timeval timeout; //超时

HANDLE hMutex = NULL;//互斥量，用于多线程



// 发送报文的执行
void sendData(int i);
void resendData(int i);

// 多线程之接收对方的ack
DWORD WINAPI ackReader(LPVOID lpParamter);

// 发送报文的逻辑
void sendFileDatagram();
void sendSynDatagram();




int main(){
    ccout.open("client2.txt",ios::app);

    WSADATA wsaData;
    WSAStartup(MAKEWORD(1, 1), &wsaData);

    makeSocket();
    setRTO();

    sendSynDatagram();
    for(int i=0;i<WINDOW_SIZE;i++){
        win.sendGrid[i].seq=i;
    }
    sendFileDatagram();


    closesocket(sockSrv);
    WSACleanup();
    return 0;
}






void sendData(int i){
    if(sequenceNumber>=sendTimes){
        return;
    }

    win.sendGrid[i].state=1;

    // win.printWindow();
    leftDataSize=DATA_SIZE;
    if(isFirstPackage){
        // 是第一个包
        packFirst();//加上Size
        sequenceNumber++;
        // 数据段的前length个位置放文件名
        for(int j=0;j<fileName.length();j++){
            sendBuffer[HEAD_SIZE+j]=fileName[j];
        }
        leftDataSize -= fileName.length();      //因为要传文件名所以剩下能传数据的空间就小了。
        bytesHaveWritten += fileName.length();  //传文件名也是传
        isFirstPackage=false;
        // 根据序列号packData.
    }
    else{
        packData();//没有Size的普通头
        sequenceNumber++;

    }
    fin.seekg(bytesHaveRead,fin.beg);
    int sendSize = min(leftDataSize, fileLength-bytesHaveRead); //如果是最后一个包的话可能会不满。

    fin.read(&sendBuffer[HEAD_SIZE + (DATA_SIZE - leftDataSize)], sendSize);// sendBuffer从什么地方开始读起，读多少
    bytesHaveRead += sendSize;
    bytesHaveWritten += sendSize;

    setBufferSize(sendSize);
    setCheckSum();
    if(win.sendGrid[i].seq==sendTimes-1){
        setFinBit(1);
        setCheckSum();
    }
    sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrServer, len);

    //保存到窗口自带的Buffer里面。
    for(int j=0;j<BUFFER_SIZE;j++){
        win.sendGrid[i].buffer[j]=sendBuffer[j];
    }

    bytesHaveSent+=getter.getBufferSize(win.sendGrid[i].buffer);
    memset(sendBuffer, 0, sizeof(sendBuffer));
    bytesHaveWritten=0;
}
void resendData(int i){

    for(int j=0;j<BUFFER_SIZE;j++){
        sendBuffer[j]=win.sendGrid[i].buffer[j];
    } 
    sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrServer, len);
    bytesHaveSent+=getter.getBufferSize(win.sendGrid[i].buffer);
}

DWORD WINAPI ackReader(LPVOID lpParamter){
    while(1){
        int it=recvfrom(sockSrv, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&addrServer, &len);
        
        if(!checkSumIsRight()) continue;
        if(!getter.getAckBit(recvBuffer)) continue;

        WaitForSingleObject(hMutex,INFINITE);
        for(int i=0;i<WINDOW_SIZE;i++){
            if(getter.getAckNum(recvBuffer)==win.sendGrid[i].seq){
                win.sendGrid[i].state=2;
                win.move();
                break;
            }
        }
        ReleaseMutex(hMutex);
    }
}

void sendFileDatagram(){
    // getFileName();

    // 根据文件名读入文件，计算文件长度和sendTimes
    findFile();

    // 用于统计
    t_start=clock();

    // 用于多线程
    HANDLE hThread = CreateThread(NULL, 0, ackReader, NULL, 0, NULL);
    hMutex = CreateMutex(NULL, FALSE,"screen");
    
    CloseHandle(hThread);//关闭线程的句柄，但是线程还会继续跑。

    while(1){

        // cout<<"send..."<<win.sendGrid[0].seq<<endl;
        // 拿到锁，可以跑一次循环了

        WaitForSingleObject(hMutex, INFINITE);

        if(win.sendGrid[0].seq==sendTimes){
            ReleaseMutex(hMutex);
            break;
        }
        for(int i=0;i<WINDOW_SIZE;i++){
            switch(win.sendGrid[i].state){
            case 0:
                win.sendGrid[i].start=clock();//获取当前时间
                sendData(i);//把发送内容放在win.sendGrid[i].buffer[]中
                break;
            case 1:
                // 是否超时
                if(clock()-win.sendGrid[i].start < 0.1*CLOCKS_PER_SEC){
                    continue;
                }
                else{
                    // printRTOErr();
                    win.sendGrid[i].start=clock();//重新设置定时器
                    resendData(i);//重新把grid[i].buffer里的内容再发一遍，并且printLog
                }
            case 2:
                break;
            }
        }
        ReleaseMutex(hMutex);


    }
}

void sendSynDatagram(){
    // 这里应该用不着滑动窗口
    while (1){
        packSynDatagram(0);
        sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrServer, len);
        int it=recvfrom(sockSrv, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&addrServer, &len);
        
        if(it<=0){
            if(WSAGetLastError() == 10060){// 超时处理
                // printRTOErr();
                continue;
            }
        }
        else{
            // 不断发送SYN，直到收到SYN+ACK为止。
            if(getter.getAckBit(recvBuffer) && getter.getSynBit(recvBuffer) && checkSumIsRight()){
                break;
            }
        }
    }
}





//##################################### Utils #####################################//

void findFile(){
    fin.open(fileName, ifstream::in | ios::binary);
    fin.seekg(0, fin.end);              //把指针定位到末尾
    fileLength = fin.tellg();           //获取指针
    fin.seekg(0, fin.beg);              //将文件流指针重新定位到流的开始

    if (fileLength <= 0) {
        // printFileErr();
        return;
    }
    
    sendTimes = ceil(((double)fileLength+(double)fileName.length() )/(double)DATA_SIZE);     //需要发送这么多次
}

void makeSocket(){
    //创建用于监听的套接字
    sockSrv = socket(AF_INET, SOCK_DGRAM, 0);
    // SOCKADDR_IN  addrServer;
    addrServer.sin_addr.s_addr = inet_addr("127.0.0.1");
    addrServer.sin_family = AF_INET;
    addrServer.sin_port = htons(1366);

    // SOCKADDR_IN  addrClient;
    len = sizeof(SOCKADDR);
}

void getFileName(){
    // cin>>fileName;
}

void setRTO(){
    timeout.tv_sec = 2000;
    timeout.tv_usec = 0;   

    setsockopt(sockSrv, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
}
//##################################### Utils #####################################//


//##################################### Setters #####################################//

void setPort(){
    // 与0xff是为了把高过8位的都清空。
    sendBuffer[1]=(sourcePort>>8)&0xff;
    sendBuffer[0]=sourcePort&0xff;
    sendBuffer[3]=(destinationPort>>8)&0xff;
    sendBuffer[2]=destinationPort&0xff;
}

void setSeqNum(unsigned int num){
    // 如果序列号超出范围，取模。
    if(num>0xffffffff){
        num%=0xffffffff;
    }
    sendBuffer[7]=(num>>24)&0xff;
    sendBuffer[6]=(num>>16)&0xff;
    sendBuffer[5]=(num>>8)&0xff;
    sendBuffer[4]=num&0xff;
}

void setAckNum(unsigned int num){
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
void setBufferSize(unsigned int num){
    if(num>0xffffffff){
        num%=0xffffffff;
    }
    sendBuffer[19]=(num>>24)&0xff;
    sendBuffer[18]=(num>>16)&0xff;
    sendBuffer[17]=(num>>8)&0xff;
    sendBuffer[16]=num&0xff;
    // getter.getBufferSize(sendBuffer);
}

//##################################### Setters #####################################//


//##################################### CheckSum #####################################//

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

bool checkSumIsRight(){
    unsigned short* buf=(unsigned short*)&recvBuffer[0];
    register unsigned long sum = 0;
    // 把每个字节加一遍
    for(int i=0;i<HEAD_SIZE/2;i++){
        sum += *buf;
        buf++;
        // 如果校验和不止16bit，把高位清0，最低位加一。
        if (sum & 0xffff0000) {
            sum &= 0xffff;
            sum++;
        }
    }

    // 全加起来应该是0xffff

    // if(sum==0xffff){
    // }
    // else{
    // }
    return sum==0xffff;
}

//##################################### CheckSum #####################################//

//##################################### DataPack #####################################//

void packSynDatagram(int sequenceNumber){
    setPort();
    setSeqNum(sequenceNumber);
    setSynBit(1);
    setAckBit(0);
    setFinBit(0);
    setCheckSum();
}


void packFirst(){
    setPort();
    setSeqNum(sequenceNumber);
    setSize(fileName.length());
    setSynBit(0);
    setAckBit(0);
    setFinBit(0);
    setCheckSum();
    // 数据段的前length个位置放文件名
    for(int j=0;j<fileName.length();j++){
        sendBuffer[HEAD_SIZE+j]=fileName[j];
    }
}
// 设置头
void packData(){
    setPort();
    setSeqNum(sequenceNumber);
    setSize(0);
    setSynBit(0);
    setAckBit(0);
    setFinBit(0);
    setCheckSum();
}

//##################################### DataPack #####################################//

//##################################### LogPrint #####################################//

void printLog(char* sendBuffer){
    cout<<"SeqNum: "<<getter.getSeqNum(sendBuffer)<<", ";
    cout<<"AckNum: "<<getter.getAckNum(sendBuffer)<<", ";
    cout<<"Size: "<<getter.getSize(sendBuffer)<<", ";
    cout<<"SYN: "<<getter.getSynBit(sendBuffer)<<", ";
    cout<<"ACK: "<<getter.getAckBit(sendBuffer)<<", ";
    cout<<"FIN: "<<getter.getFinBit(sendBuffer)<<", ";
    cout<<"CheckSum: "<<getter.getCheckSum(sendBuffer)<<endl;
}
//##################################### LogPrint #####################################//

//##################################### ErrorPrint #####################################//

void printFileErr(){
    cout << "+--------------------------------+\n";
    cout << "| Sorry we cannot open the file. |\n";
    cout << "+--------------------------------+\n";                
}

void printRTOErr(){
    cout << "+-----------------------------------------------------------+\n";
    cout << "| Over RTO. The server did not respond us, will send again. |\n";
    cout << "+-----------------------------------------------------------+\n";
}

//##################################### ErrorPrint #####################################//