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

#define WINDOW_SIZE 0x10
#define BUFFER_SIZE 0xf000
#define HEAD_SIZE 0x14
#define DATA_SIZE (BUFFER_SIZE-HEAD_SIZE)

#pragma comment(lib, "ws2_32.lib")
using namespace std;

ofstream ccout;//用来输出过多的日志


char sendBuffer[BUFFER_SIZE];
char recvBuffer[BUFFER_SIZE];

int sourcePort=1234;
int destinationPort=1235;
unsigned int sequenceNumber=0;

string fileName;

int fileNameLength; //名字的长度

ofstream fout;

int ackNum=0;   //这次收到的是什么包

void setPort();
void setSeqNum(unsigned int num);
void setAckNum(unsigned int num);
void setSize(int num);
void setAckBit(char a);
void setSynBit(char a);
void setFinBit(char a);
unsigned short calCheckSum(unsigned short* buf) ;//计算校验和
void setCheckSum();

// 用于设置套接字
void makeSocket();


// getters 之所以传参char* 是因为也要输出sendBuffer.
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



void packSynAckDatagram();
void packAckDatagram(int ackNum);
void packEmptyDatagram();

void printLogSendBuffer();
void printLogRecvBuffer();

bool checkSumIsRight();

void getFileName();

class SendGrid{
public:
    /**
     * state==0：这个窗口对应的序列号没收到
     * state==1：这个窗口已经匹配上了
     */
    int state;  //如果已经收到了,move.
    int seq;    //这个窗口对应的序列号
    char buffer[BUFFER_SIZE];   //该序列号的包
    SendGrid():state(0),seq(-1){};
    void setBuffer(char* sendBuffer){
        for(int i=0;i<BUFFER_SIZE;i++){
            buffer[i]=sendBuffer[i];
        }
    }
};

// 一个窗口大小是WINDOW_SIZE
class SendWindow{
public:
    SendGrid sendGrid[WINDOW_SIZE];
    // 窗口向右移动1位
    void move(){
        if(sendGrid[0].state==1){//如果最左侧已经ack了，需要写回。
            cout<<"move"<<endl;
            // 写数据
            if(sendGrid[0].seq==0){
                fileNameLength=getter.getSize(sendGrid[0].buffer);
            }
            else{
                fileNameLength=0;
            }
            
            unsigned int bufferSize=getter.getBufferSize(sendGrid[0].buffer);
            cout<<"bufferSize:"<<bufferSize<<endl;
            fout.write(&sendGrid[0].buffer[HEAD_SIZE+fileNameLength],bufferSize);

            cout<<"写了包 seq"<<sendGrid[0].seq<<endl;
            if(getter.getFinBit(sendGrid[0].buffer)){
                cout<<"我读完了，关文件了！"<<endl;
                fout.close();
            }

            for(int i=1;i<WINDOW_SIZE;i++){
                sendGrid[i-1].state=sendGrid[i].state;
                sendGrid[i-1].seq=sendGrid[i].seq;
                for(int j=0;j<BUFFER_SIZE;j++){
                    sendGrid[i-1].buffer[j]=sendGrid[i].buffer[j];
                }
            }
            sendGrid[15].state=0;//最右边的格子seq++
            sendGrid[15].seq=sendGrid[14].seq+1;
            for(int j=0;j<BUFFER_SIZE;j++){
                sendGrid[15].buffer[j]=0;
            }

            printWindow();

            //如果最左侧还是已经ack了，继续move
            this->move();
        }
    }
    // 用于debug
    void printWindow(){
        for(int i=0;i<WINDOW_SIZE;i++){
            ccout<<"number "<<i<<" state: "<<sendGrid[i].state<<", seq: "<<sendGrid[i].seq<<endl;
        }
        ccout<<endl;
    }
} win;


SOCKET sockSrv;
SOCKADDR_IN addrSrv;
SOCKADDR_IN addrClient;   //用来接收客户端的地址信息
int len;


// 用于接收报文
void recvDatagram();



int main(){
    ccout.open("server.txt");
    //加载套接字库
    WSADATA wsaData;
    WSAStartup(MAKEWORD(1, 1), &wsaData);

    makeSocket();

    //初始化窗口
    for(int i=0;i<WINDOW_SIZE;i++){
        win.sendGrid[i].seq=i;
    }

    recvDatagram();

    closesocket(sockSrv);
    WSACleanup();
    return 0;
}



void recvDatagram(){

    //建立连接，接收数据，判断数据校验和，回应报文
    while (1){
        recvfrom(sockSrv, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&addrClient, &len);
        ccout<<"received."<<endl;
        printLogRecvBuffer();

    // 如果是SYN报文#######################################################################
        if(getter.getSynBit(recvBuffer)){
            ccout<<"Got an SYN datagram!"<<endl;
            
            //如果校验和没问题就返回一个SYN ACK报文，否则返回空报文
            if(checkSumIsRight()){
                for(int i=0;i<WINDOW_SIZE;i++){
                    win.sendGrid[i].seq=i;
                    win.sendGrid[i].state=0;
                }
                // SYN报文协商起始的序列号。
                ackNum=getter.getSeqNum(recvBuffer);
                setAckNum(getter.getSeqNum(recvBuffer));
                packSynAckDatagram();
                sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrClient, len);
                printLogSendBuffer();
                ccout<<"Sent SYN ACK."<<endl;
            }
            else{
                packEmptyDatagram();
                sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrClient, len);
                printLogSendBuffer();
                ccout<<"sent."<<endl;
            }
        }
    // 如果是SYN报文#######################################################################


    // 如果是普通数据报文###################################################################
        else{
            ccout<<"\n这是一个文件报文!"<<endl;

            if(checkSumIsRight()){
                //如果校验和没问题就返回一个ACK报文+序列号
                //（这里是默认SYN只发一遍，之后的全部都是File）


                int i=0;
                for(;i<WINDOW_SIZE;i++){
                    // 如果匹配上了
                    if(getter.getSeqNum(recvBuffer)==win.sendGrid[i].seq){
                        ccout<<"匹配上了我们窗口里的"<<i<<",它要的序列号是"<<win.sendGrid[i].seq<<endl;

                        // 如果是第一个，开一下文件
                        if(getter.getSize(recvBuffer)!=0){
                            ccout<<"这是第一个包，我要开始写了。"<<endl;
                            fileName="";
                            fileNameLength=getter.getSize(recvBuffer);
                            cout<<"文件名长度"<<fileNameLength<<endl;
                            ccout<<"我获取了文件的名称"<<endl;

                            for(int i=0;i<getter.getSize(recvBuffer);i++){
                                fileName+=recvBuffer[HEAD_SIZE+i];
                            }
                            ccout<<"fileName: "<<fileName<<endl;
                            fout.open(fileName,ios_base::out | ios_base::app | ios_base::binary);

                        }

                        // 1表示已经ack了
                        win.sendGrid[i].state=1;
                        for(int j=0;j<BUFFER_SIZE;j++){
                            win.sendGrid[i].buffer[j]=recvBuffer[j];
                        }
                        // win.printWindow();
                        packAckDatagram(getter.getSeqNum(recvBuffer));
                        if(getter.getFinBit(recvBuffer)){//如果收到了fin，挥手。
                            setFinBit(1);
                            setCheckSum();
                            ccout<<"file receiving ends."<<endl;
                        }
                        sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrClient, len);
                        printLogSendBuffer();
                        ccout<<"sent."<<endl;

                        break;
                    }
                }
                //没匹配上
                if(i==WINDOW_SIZE){
                    // 如果已经ack过了不在窗口里了，再发一遍
                    if(getter.getSeqNum(recvBuffer)<win.sendGrid[0].seq){
                        ccout<<"left of the window.\n";
                        packAckDatagram(getter.getSeqNum(recvBuffer));
                        if(getter.getFinBit(recvBuffer)){//如果收到了fin，挥手。
                            setFinBit(1);
                            setCheckSum();
                            fout.close();
                            ccout<<"file receiving ends."<<endl;
                        }
                        sendto(sockSrv, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&addrClient, len);
                        printLogSendBuffer();
                        ccout<<"sent."<<endl;
                        continue;
                    }
                    // 这个包对我而言为时过早了啊
                    if(getter.getSeqNum(recvBuffer)>win.sendGrid[15].seq){
                        ccout<<"too early!"<<endl;
                        continue;
                    }

                    ccout<<"not in window!"<<endl;
                }
                win.move();
            }

            else{
                ccout<<"CheckSum not right!\n";
            }
        }
    // 如果是普通数据报文###################################################################
    
    }    
}


//##################################### Utils #####################################//

void makeSocket(){
    //创建用于监听的套接字
    sockSrv = socket(AF_INET, SOCK_DGRAM, 0);//失败会返回 INVALID_SOCKET
    // SOCKADDR_IN addrSrv;
    addrSrv.sin_addr.s_addr = inet_addr("127.0.0.1");//输入你想通信的她（此处是本机内部）
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(1366);

    //绑定套接字, 绑定到端口
    bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR));//会返回一个SOCKET_ERROR

    // SOCKADDR_IN addrClient;   //用来接收客户端的地址信息
    len = sizeof(SOCKADDR);
    ccout<<"listening..."<<endl;
}
void getFileName(){
    ccout<<"Tell me which file you want to send.\n";
    cin>>fileName;
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



void packSynAckDatagram(){
    setPort();
    setSeqNum(sequenceNumber);
    setSynBit(1);
    setAckBit(1);
    setFinBit(0);
    setCheckSum();
}

//事实上已经改成了ackNum，而不是expectedNum，但没改名字
void packAckDatagram(int ackNum){
    setPort();
    setSeqNum(sequenceNumber);
    setAckNum(ackNum);
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
