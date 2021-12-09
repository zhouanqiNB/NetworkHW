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

#define BUFFER_SIZE 0xf000
#define HEAD_SIZE 0x10
#define DATA_SIZE (BUFFER_SIZE-HEAD_SIZE)

#pragma comment(lib, "ws2_32.lib")
using namespace std;

ofstream ccout;

char sendBuffer[BUFFER_SIZE];
char recvBuffer[BUFFER_SIZE];

long bytesHaveSent=0;

int sourcePort=1234;
int destinationPort=1235;
int sequenceNumber=0;

bool isFirstPackage=true;

string fileName;
int fileLength;

void setPort();
void setSeqNum(int& num);
void setAckNum(int num);
void setSize(int num);
void setAckBit(char a);
void setSynBit(char a);
void setFinBit(char a);

// 计算校验和
unsigned short calCheckSum(unsigned short* buf) ;

void setCheckSum();

// getters 之所以传参char* 是因为我debug的时候也要输出sendBuffer.
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
void packAckDatagram();
void packFirst();
void packEmptyDatagram();
void packData();

void printLogSendBuffer();
void printLogRecvBuffer();

bool checkSumIsRight();
void getFileName();

void printBindingErr();
void printLibErr();
void printCreateSocketErr();
void printFileErr();
void printRTOErr();

int bytesHaveRead;
int leftDataSize;
// 一个格子对应着一个序列号和一个状态
class SendGrid{
public:
    /**
     * state==0：这个窗口没被使用
     * state==1：这个窗口被占用了，已经发送了，但没收到ack，收到ack后转为2，
     *           超时还没有收到ack就重传。如果转2的那个窗口是最左侧的，移动滑动窗口。
     * state==2：发了而且收到ack了，只要左侧的都好了就可以提交并且清空内容。
     */
    int state;
    int seq;
    // Timer timer;
    char buffer[BUFFER_SIZE];
    clock_t start;//这个包被发出去的时间
    SendGrid():state(0),seq(-1){};
    void setState(int a){state=a;}
    void setSeq(int a){seq=a;}
    void setBuffer(char* sendBuffer){
        for(int i=0;i<BUFFER_SIZE;i++){
            buffer[i]=sendBuffer[i];
        }
    }
};
int nowTime;
int sendTimes;

// 一个窗口大小是16
class SendWindow{
public:
    SendGrid sendGrid[16];
    // 窗口向右移动1位
    void move(){
        if(sendGrid[0].state==2){//如果最左侧是不是已经ack了
            ccout<<"是时候移动窗口了！"<<endl;
            nowTime=sendGrid[0].seq+1;
            cout<<nowTime<<endl;
            for(int i=1;i<16;i++){
                sendGrid[i-1].state=sendGrid[i].state;
                sendGrid[i-1].seq=sendGrid[i].seq;
                for(int j=0;j<BUFFER_SIZE;j++){
                    sendGrid[i-1].buffer[j]=sendGrid[i].buffer[j];
                }
            }
            sendGrid[15].setState(0);//最右边的格子重新空闲
            sendGrid[15].setSeq(sendGrid[14].seq+1);
            ccout<<"我移动了窗口！"<<endl;
            // printWindow();
            cout<<nowTime<<" "<<sendTimes<<endl;
            if(nowTime==sendTimes){
                cout<<"发完了我溜了"<<endl;
                exit(0);
            }
            printWindow();
            if(sendGrid[0].state==2)//如果最左侧还是已经ack了，继续move
                this->move();
        }
    }
    void printWindow(){
        for(int i=0;i<16;i++){
            ccout<<"number "<<i<<" state: "<<sendGrid[i].state<<", seq: "<<sendGrid[i].seq<<endl;
        }
        ccout<<endl;
    }
    bool haveGridLeft(){
        if(sendGrid[15].state!=0){
            return false;
        }
        else{
            return true;
        }
    }
    int getEmptyGrid(){
        for(int i=0;i<16;i++){
            if(sendGrid[i].state==0){
                return i;
            }
        }
    }
} win;

SOCKET sockSrv;
SOCKADDR_IN  addrServer;
SOCKADDR_IN  addrClient;
int len;
struct timeval timeout;

HANDLE hMutex = NULL;//互斥量
ifstream fin;

int bytesHaveWritten; //加到bytesHaveSent里面。

void sendSynDatagram();
void sendFileDatagram();
void makeSocket();
void setRTO();
void findFile();
DWORD WINAPI ackReader(LPVOID lpParamter);
void sendData(int i);
void resendData(int i);

int main(){
    ccout.open("client.txt");
    ccout<<"==============================================================================="<<endl;

	WSADATA wsaData;
	WSAStartup(MAKEWORD(1, 1), &wsaData);

    makeSocket();
    setRTO();

    sendSynDatagram();
    sendFileDatagram();

    closesocket(sockSrv);
    WSACleanup();
    return 0;
}
void setRTO(){
    timeout.tv_sec = 2000;
    timeout.tv_usec = 0;   

    setsockopt(sockSrv, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    ccout << "We successfully set the RTO." << endl;    
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

void findFile(){
    fin.open(fileName, ifstream::in | ios::binary);
    fin.seekg(0, fin.end);              //把指针定位到末尾
    fileLength = fin.tellg();           //获取指针
    fin.seekg(0, fin.beg);              //将文件流指针重新定位到流的开始

    if (fileLength <= 0) {
        printFileErr();
        return;
    }
    ccout<<"The size of this file is "<<fileLength<<" bytes.\n";
    
    // ccout<<"fileLength: "<<fileLength<<endl;
    // ccout<<"fileName.length(): "<<fileName.length()<<endl;
    // ccout<<"DATA_SIZE: "<<DATA_SIZE<<endl;
    sendTimes = ceil(((double)fileLength+(double)fileName.length() )/(double)DATA_SIZE);     //需要发送这么多次
    ccout << "We will split this file to " << sendTimes << " packages and send it." << endl;

}
void sendData(int i){
    ccout<<"我要设置win"<<i<<"的seq为"<<sequenceNumber<<endl;

    win.sendGrid[i].seq=sequenceNumber;
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


    sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrServer, len);
    ccout<<"发送了"<<sequenceNumber-1<<endl;

    //保存到窗口自带的Buffer里面。
    for(int j=0;j<BUFFER_SIZE;j++){
        win.sendGrid[i].buffer[j]=sendBuffer[j];
    }

    bytesHaveSent+=bytesHaveWritten;
    ccout<<"\nbytesHaveSent: "<<bytesHaveSent<<endl;
    // printLogSendBuffer();
    ccout<<"sent."<<endl;
    memset(sendBuffer, 0, sizeof(sendBuffer));
}
void resendData(int i){
    for(int j=0;j<BUFFER_SIZE;j++){
        sendBuffer[j]=win.sendGrid[i].buffer[j];
    } 
    sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrServer, len);
    cout<<"重新发了"<<i<<"号"<<endl;
    printLogSendBuffer();
    ccout<<sequenceNumber<<" resent."<<endl;

}
DWORD WINAPI ackReader(LPVOID lpParamter){

    while(1){
        // 收到消息
        int it=recvfrom(sockSrv, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&addrServer, &len);
        
        // 当对方发癫
        if(!checkSumIsRight()){
            ccout<<"CheckSum is wrong!"<<endl;
        }
        if(getter.getAckBit(recvBuffer)==false){
            ccout<<"not an ack datagram!"<<endl;
            continue;
        }

        WaitForSingleObject(hMutex,INFINITE);
        ccout<<"==============="<<endl;
        ccout<<"ackreader拿到了锁"<<endl;
        ccout<<"==============="<<endl;
        printLogRecvBuffer();

        int i=0;
        for(;i<16;i++){
            // 如果匹配上了
            if(getter.getAckNum(recvBuffer)==win.sendGrid[i].seq){
                ccout<<"matched! window: "<<i;
                ccout<<"seq: "<<getter.getAckNum(recvBuffer)<<endl;
                break;
            }
        }
        // 说明一个都没匹配上
        if(i==16){
            ccout<<"一个都没匹配上"<<endl;
            ReleaseMutex(hMutex);
            continue;
        }

        win.sendGrid[i].state=2;//把这个格子的状态位置2
        // win.printWindow();

        // 没接收到一个看看能不能Move。
        win.move();

        if(nowTime==sendTimes){
            ReleaseMutex(hMutex);
            cout<<"现在是第"<<nowTime<<"次。\n";
            cout<<"我退出了"<<endl;
            return 0L;//表示返回的是long型的0

        }
        ccout<<"==============="<<endl;
        ccout<<"ackreader释放了锁"<<endl;
        ccout<<"==============="<<endl;
        ReleaseMutex(hMutex);

    }

}


void sendFileDatagram(){
    getFileName();

    // 根据文件名读入文件，计算文件长度和sendTimes
    findFile();

    int t_start=clock();
    sequenceNumber=0;

    bytesHaveRead=0;    //数据指针
    bytesHaveWritten=0; //加到bytesHaveSent里面。

    nowTime=0;


    HANDLE hThread = CreateThread(NULL, 0, ackReader, NULL, 0, NULL);
    hMutex = CreateMutex(NULL, FALSE,"screen");
    
    CloseHandle(hThread);//关闭线程的句柄，但是线程还会继续跑。

    while(1){

        // 拿到锁，可以跑一次循环了
        if(nowTime==sendTimes-1){
            cout<<"我退出了！"<<endl;
            return;
        }

        WaitForSingleObject(hMutex, INFINITE);
        ccout<<"==============="<<endl;
        ccout<<"发文件拿到了锁"<<endl;
        ccout<<"==============="<<endl;

        if(win.sendGrid[0].seq==sendTimes-1){
            ccout<<"==============="<<endl;
            ccout<<"这是最后一个了"<<endl;
            ccout<<"==============="<<endl;
            ReleaseMutex(hMutex);
            break;
        }
        for(int i=0;i<16;i++){

            if(win.sendGrid[i].state==2){//如果已经被ack了不做处理
                ccout<<"窗口"<<i<<"已经被ack了"<<endl;
                continue;
            }
            // 如果timer没被用上，那么开始发文件
            if(win.sendGrid[i].state==0){
                ccout<<"窗口"<<i<<"还没用上"<<endl;

                win.sendGrid[i].start=clock();//获取当前时间
                ccout<<"我要在窗口"<<i<<"发数据"<<endl;
                sendData(i);//把发送内容放在win.sendGrid[i].buffer[]中
                // 同时在里面printLog
            }
            // timer已经开了
            else{
                ccout<<"窗口"<<i<<"已经用上了"<<endl;

                int time=clock()-win.sendGrid[i].start;
                // 还没有超时呢不处理
                // clock是按照CPU算的，我这里给的超时是一秒
                if(time<1*CLOCKS_PER_SEC){
                    ccout<<"没有超时\n";
                    continue;
                }
                // 超时了，重传
                else{
                    printRTOErr();
                    win.sendGrid[i].start=clock();//重新设置定时器
                    resendData(i);//重新把grid[i].buffer里的内容再发一遍，并且printLog
                }
            }

        }
        ccout<<"==============="<<endl;
        ccout<<"发文件释放了锁"<<endl;
        ccout<<"==============="<<endl;
        ReleaseMutex(hMutex);


    }
}

void sendSynDatagram(){
    // 这里应该用不着滑动窗口
    while (1){
        packSynDatagram(0);
        sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrServer, len);
        printLogSendBuffer();
        ccout << "sent." << endl;

        int it=recvfrom(sockSrv, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&addrServer, &len);
        
        if(it<=0){
            if(WSAGetLastError() == 10060){// 超时处理
                printRTOErr();
                continue;
            }
        }
        else{
            printLogRecvBuffer();
            // 不断发送SYN，直到收到SYN+ACK为止。
            if(getter.getAckBit(recvBuffer) && getter.getSynBit(recvBuffer) && checkSumIsRight()){
                ccout<<"Got a SYN ACK!"<<endl;
                break;
            }
        }
    }
}

void getFileName(){
    std::cout<<"Tell me which file you want to send.\n";
    cin>>fileName;
}

//##################################### Setters #####################################//

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
        ccout<<"+-------------------------------------------------+\n";
        ccout<<"| This fileName is too long! We cannot handle it! |\n";
        ccout<<"+-------------------------------------------------+\n";
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
        // ccout<<*buf<<" ";
        buf++;
        // 如果校验和不止16bit，把高位清0，最低位加一。
        if (sum & 0xffff0000) {
            // ccout<<"把高位清零\n";
            sum &= 0xffff;
            sum++;
        }
    }

    // 全加起来应该是0xffff

    // if(sum==0xffff){
    //     ccout<<"CheckSum: "<<sum<<", ";
    //     ccout<<"CheckSum right!"<<endl;
    // }
    // else{
    //     ccout<<"CheckSum: "<<sum<<", ";
    //     ccout<<"CheckSum wrong!"<<endl;
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

void packSynAckDatagram(){
    setPort();
    setSeqNum(sequenceNumber);
    setSynBit(1);
    setAckBit(1);
    setFinBit(0);
    setCheckSum();
}

void packAckDatagram(){
    setPort();
    setSeqNum(sequenceNumber);
    setSynBit(0);
    setAckBit(1);
    setFinBit(0);
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

void printLogSendBuffer(){
    ccout<<"sendBuffer: ";
    ccout<<"SeqNum: "<<getter.getSeqNum(sendBuffer)<<", ";
    ccout<<"AckNum: "<<getter.getAckNum(sendBuffer)<<", ";
    ccout<<"Size: "<<getter.getSize(sendBuffer)<<", ";
    ccout<<"SYN: "<<getter.getSynBit(sendBuffer)<<", ";
    ccout<<"ACK: "<<getter.getAckBit(sendBuffer)<<", ";
    ccout<<"FIN: "<<getter.getFinBit(sendBuffer)<<", ";
    ccout<<"CheckSum: "<<getter.getCheckSum(sendBuffer)<<endl;
}

void printLogRecvBuffer(){
    ccout<<"recvBuffer: ";
    ccout<<"SeqNum: "<<getter.getSeqNum(recvBuffer)<<", ";
    ccout<<"AckNum: "<<getter.getAckNum(recvBuffer)<<", ";
    ccout<<"Size: "<<getter.getSize(recvBuffer)<<", ";
    ccout<<"SYN: "<<getter.getSynBit(recvBuffer)<<", ";
    ccout<<"ACK: "<<getter.getAckBit(recvBuffer)<<", ";
    ccout<<"FIN: "<<getter.getFinBit(recvBuffer)<<", ";
    ccout<<"CheckSum: "<<getter.getCheckSum(recvBuffer)<<endl;
}
//##################################### LogPrint #####################################//

//##################################### ErrorPrint #####################################//

void printBindingErr(){
    ccout << "+-------------------------------+\n" ;
    ccout << "| We met problems when binding. |\n" ;
    ccout << "+-------------------------------+\n" ;
}

void printLibErr(){
    ccout << "+------------------------------------+\n" ;
    ccout << "| We met problems when loading libs. |\n" ;
    ccout << "+------------------------------------+\n" ;            
}

void printCreateSocketErr(){
    ccout << "+--------------------------------+\n" ;
    ccout << "| We cannot create a new socket. |\n" ;
    ccout << "+--------------------------------+\n" ;            
}

void printFileErr(){
    ccout << "+--------------------------------+\n";
    ccout << "| Sorry we cannot open the file. |\n";
    ccout << "+--------------------------------+\n";                
}

void printRTOErr(){
    ccout << "+-----------------------------------------------------------+\n";
    ccout << "| Over RTO. The server did not respond us, will send again. |\n";
    ccout << "+-----------------------------------------------------------+\n";
}

//##################################### ErrorPrint #####################################//
