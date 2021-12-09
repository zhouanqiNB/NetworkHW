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
    SendGrid():state(0),seq(-1){};
    void setState(int a){state=a;}
    void setSeq(int a){seq=a;}
    void setBuffer(char* sendBuffer){
        for(int i=0;i<BUFFER_SIZE;i++){
            buffer[i]=sendBuffer[i];
        }
    }
};

// 一个窗口大小是16
class SendWindow{
public:
    SendGrid sendGrid[16];
    // 窗口向右移动1位
    void move(){
        if(sendGrid[0].state==2){//如果最左侧是不是已经ack了
            for(int i=1;i<16;i++){
                sendGrid[i-1].state=sendGrid[i].state;
                sendGrid[i-1].seq=sendGrid[i].seq;
            }
            sendGrid[15].setState(0);//最右边的格子重新空闲
            if(sendGrid[0].state==2)//如果最左侧还是已经ack了，继续move
                this->move();
        }
    }
    void printWindow(){
        for(int i=0;i<16;i++){
            cout<<"number "<<i<<" state: "<<sendGrid[i].state<<", seq: "<<sendGrid[i].seq<<endl;
        }
        cout<<endl;
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
};

SOCKET sockSrv;
SOCKADDR_IN  addrServer;
SOCKADDR_IN  addrClient;
int len;
struct timeval timeout;

void sendSynDatagram();
void sendFileDatagram();
void makeSocket();
void setRTO();

int main(){

	WSADATA wsaData;
	WSAStartup(MAKEWORD(1, 1), &wsaData);

    makeSocket();
    setRTO();

    sendSynDatagram();
    // sendFileDatagram();

    closesocket(sockSrv);
    WSACleanup();
    return 0;
}
void setRTO(){
    timeout.tv_sec = 2000;
    timeout.tv_usec = 0;   

    setsockopt(sockSrv, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    cout << "We successfully set the RTO." << endl;    
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

void sendFileDatagram(){
    getFileName();

    // 读入文件，计算文件长度
    ifstream fin(fileName, ifstream::in | ios::binary);
    fin.seekg(0, fin.end);              //把指针定位到末尾
    fileLength = fin.tellg();           //获取指针
    fin.seekg(0, fin.beg);              //将文件流指针重新定位到流的开始

    if (fileLength <= 0) {
        printFileErr();
        return;
    }
    cout<<"The size of this file is "<<fileLength<<" bytes.\n";
    
    // cout<<"fileLength: "<<fileLength<<endl;
    // cout<<"fileName.length(): "<<fileName.length()<<endl;
    // cout<<"DATA_SIZE: "<<DATA_SIZE<<endl;
    int sendTimes = ceil(((double)fileLength+(double)fileName.length() )/(double)DATA_SIZE);     //需要发送这么多次
    cout << "We will split this file to " << sendTimes << " packages and send it." << endl;

    int t_start=clock();
    sequenceNumber=0;

    int bytesHaveRead=0;    //数据指针
    int bytesHaveWritten=0; //加到bytesHaveSent里面。

    int nowTime=0;
    while(1){
        int leftDataSize=DATA_SIZE;
        bytesHaveWritten=0;

        //把头弄好
        if(isFirstPackage){
            packFirst();//加上Size

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
        }
        if(nowTime==sendTimes-1){
            setFinBit(1);
            setCheckSum();
        }
        //把头弄好


        //开始读数据
        fin.seekg(bytesHaveRead,fin.beg);
        int sendSize = min(leftDataSize, fileLength-bytesHaveRead); //如果是最后一个包的话可能会不满。

        // cout<<"sendSize: "<<sendSize<<endl;
        // cout<<"leftDataSize: "<<leftDataSize<<endl;
        // cout<<"fileLength-bytesHaveRead: "<<fileLength-bytesHaveRead<<endl;
        fin.read(&sendBuffer[HEAD_SIZE + (DATA_SIZE - leftDataSize)], sendSize);// sendBuffer从什么地方开始读起，读多少
        bytesHaveRead += sendSize;
        bytesHaveWritten += sendSize;


        sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrServer, len);
        bytesHaveSent+=bytesHaveWritten;
        cout<<"bytesHaveSent: "<<bytesHaveSent<<endl;
        printLogSendBuffer();
        cout<<"sent."<<endl;
        memset(sendBuffer, 0, sizeof(sendBuffer));


        int it=recvfrom(sockSrv, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&addrServer, &len);
        
        if(it<=0){
            if(WSAGetLastError()==10060){ //超时重传
                printRTOErr();
                continue;//再次循环，反正ACKNum没有更新，还用那个序列号
            }
        }
        else{
            printLogRecvBuffer();
            cout<<"received."<<endl;

            // 如果校验和出问题了，把当前的包再发一遍
            if(!checkSumIsRight()){
                bytesHaveRead-=sendSize;
                bytesHaveWritten -= sendSize;
                continue;
            }

            // 没有损坏
            if(getter.getAckBit(recvBuffer)){
                if(sequenceNumber+1==getter.getAckNum(recvBuffer)||sequenceNumber+1+getter.getAckNum(recvBuffer)==128){
                    nowTime++;
                    cout<<"nowTime: "<<nowTime<<endl;
                }
                sequenceNumber=getter.getAckNum(recvBuffer);

                if(getter.getFinBit(recvBuffer)){
                    break;
                }
                continue;
            } 
        }
    }
    int t_end=clock();
    cout<<"Sent "<<bytesHaveSent<<" bytes, ";
    cout<<"Cost time: "<< t_end - t_start << "(ms), ";
    // numOfBit/time
    cout <<"Throughput rate: " << bytesHaveSent * 8 / (t_end - t_start) * CLOCKS_PER_SEC << " bps" << endl;
    fin.close();    
}

void sendSynDatagram(){
    // 这里应该用不着滑动窗口
    while (1){
        packSynDatagram(0);
        sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrServer, len);
        printLogSendBuffer();
        cout << "sent." << endl;

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
                cout<<"Got a SYN ACK!"<<endl;
                break;
            }
        }
    }
}

void getFileName(){
    cout<<"Tell me which file you want to send.\n";
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
//##################################### LogPrint #####################################//

//##################################### ErrorPrint #####################################//

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
    cout << "+-----------------------------------------------------------+\n";
    cout << "| Over RTO. The server did not respond us, will send again. |\n";
    cout << "+-----------------------------------------------------------+\n";
}

//##################################### ErrorPrint #####################################//
