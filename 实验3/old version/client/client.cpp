/**
 * 1911590 周安琪
 * 实验3-1
 * 客户端/发送端代码
 */
#include <iostream>
#include <fstream>
#include <string.h>
#include <ctime>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#include<cmath>

#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll

using namespace std;


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

const int BUFFER_SIZE = 0xFFFF;
const int HEAD_SIZE=16;
#define DATA_SIZE (BUFFER_SIZE-HEAD_SIZE)


char sendBuffer[BUFFER_SIZE];
char recvBuffer[BUFFER_SIZE];

int bytesHaveSent=0;


//-------------------------
string fileName;
SOCKET cli_socket;
int fileLength;
sockaddr_in serveraddr, clientaddr;
//-------------------------



int sourcePort=1234;
int destinationPort=1235;
int sequenceNumber=0;
int ackNumber;
int checkSum;


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

int main(){
    
// 设置客户端和服务端IP
    string clientAddr="127.0.0.1";
    string serverAddr="127.0.0.1";
    cout<<"Your IP is "<<clientAddr<<" and your file will be sent to "<<serverAddr<<".\n";
// 设置客户端和服务端IP


// 设置定时器的时间
    // rdt3.0，ms
    struct timeval timeout;
    timeout.tv_sec = 5000;
    timeout.tv_usec = 0;
// 设置定时器的时间


// 加载库文件
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) printLibErr();
    else cout << "We successfully loaded the libs." << endl;
// 加载库文件

    serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(1234);

    cli_socket = socket(AF_INET, SOCK_DGRAM, 0);
    // if (cli_socket == INVALID_SOCKET) printCreateSocketErr();
    // else cout << "We successfully created a new socket." << endl;

    setsockopt(cli_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    cout << "We successfully set the RTO." << endl;


// 发送文件的循环
    while(1){

        getFileName();

    // 读入文件，计算文件长度
        ifstream fin(fileName, ifstream::in | ios::binary);
        fin.seekg(0, fin.end);              //把指针定位到末尾
        fileLength = fin.tellg();       //获取指针
        fin.seekg(0, fin.beg);              //将文件流指针重新定位到流的开始


        if (fileLength <= 0) {
            printFileErr();
            continue;
        }
        cout<<"The size of this file is "<<fileLength<<" bytes.\n";
    // 读入文件，计算文件长度


        printf("connecting.....\n");
        shakeHand();


    //接收客户端对SYN的回复
        //这回肯定不会有AckNumber
        if (int it = recvfrom(cli_socket, recvBuffer, sizeof(recvBuffer), 0, (sockaddr*)&serveraddr, &sockaddrInLength) <= 0) {
            if (it == 0) {
                cout << "+-----------------------+\n";
                cout << "| Received an empty ACK.|\n";
                cout << "+-----------------------+\n";
            }
            if (WSAGetLastError() == 10060) {
                cout << "+------------------------------------------+\n";
                cout << "| Over RTO. The server did not respond us. |\n";
                cout << "+------------------------------------------+\n";
                // continue;
            }
        }
        else {
            // 如果ACK是1，那么成功接收。
            if (getAckBit()&&getSynBit()) {
                cout << "We successfully built the connection! We are going to send your file." << endl;
            }
        }
        setSynBit(0);

    接收客户端对SYN的回复
        

    // 发送文件
        int sendTimes = ceil( fileLength / DATA_SIZE );     //需要发送这么多次
        cout << "We will split this file to " << sendTimes << " packages and send it." << endl;

        int t_start=clock();
        bool isFirstPackage=true;

        int bytesHaveRead=0;
        int leftDataSize=0;
        int bytesHaveWritten=0; //加到bytesHaveSent里面。

        for(int i=0;i<sendTimes;i++){

            bytesHaveWritten=0;
            leftDataSize=DATA_SIZE; //剩下的数据段的大小，应该是65535-16=65519

        // 处理文件名
            if(isFirstPackage){
                setSize(fileName.length());

                for(int j=0;j<fileName.length();j++){
                    sendBuffer[HEAD_SIZE+j]=fileName[j];
                }
                leftDataSize -= fileName.length();      //因为要传文件名所以剩下能传数据的空间就小了。
                bytesHaveWritten += fileName.length();  //传文件名也是传
                isFirstPackage=false;
            }
            else{
                setSize(0);
            }
        // 处理文件名

        // 读文件
            fin.seekg(bytesHaveRead,fin.beg); //把指针移动到要发送的起始位置。

            int sendSize = min(leftDataSize, fileLength-bytesHaveRead); //如果是最后一个包的话可能会不满。
            fin.read(&sendBuffer[HEAD_SIZE + (DATA_SIZE - leftDataSize)], sendSize);// sendBuffer从什么地方开始读起，读多少
            bytesHaveRead += sendSize;
            bytesHaveWritten += sendSize;
        // 读文件

        // 设置头 
            //设置序列号，不需要自动++，依赖对方返回的AckNumber来确定下一个序列号。
            setSeqNum(sequenceNumber);
            // 设置校验和
            setCheckSum();

            if(i==sendTimes-1)
                setFinBit(1);
            else
                setFinBit(0);
        // 设置头 

        // 发文件
            sendto(cli_socket, sendBuffer, sizeof(sendBuffer), 0, (sockaddr*)&serveraddr, sockaddrInLength);
            bytesHaveSent += bytesHaveWritten;
            memset(sendBuffer, 0, sizeof(sendBuffer));
        // 发文件


        // 接收ACK
            // 如果出问题了。
            if (int it = recvfrom(cli_socket, recvBuffer, sizeof(recvBuffer), 0, (sockaddr*)&serveraddr, &sockaddrInLength) <= 0) {
                if (it == 0) {
                    cout << "+----------------------------+\n";
                    cout << "| Received an empty datagram.|\n";
                    cout << "+----------------------------+\n";
                }
                int errorcode = WSAGetLastError();
                // no response over RTO
                if (errorcode == 10060) {
                    cout << "+------------------------------------------+\n";
                    cout << "| Over RTO. The server did not respond us. |\n";
                    cout << "+------------------------------------------+\n";
                    i--;    //退回再发一次
                    sequenceNumber=i;
                    bytesHaveRead -= sendSize;
                    if (i == -1) isFirstPackage=true;
                    continue;
                }
                else {
                    cout << "+--------------------------------------+\n";
                    cout << "| Unknown Error, client will shut down |\n";
                    cout << "+--------------------------------------+\n";
                    return 0;
                }
            }

            // 查看到Ack而且查看校验和没问题
            if(getAckBit()&&!isCorrupt()){
                cout<<"Got a valid ACK!\n";
                i=sequenceNumber=getAckNum();
                cout<<"the next sequenceNumber is "<<sequenceNumber<<".\n";
                bytesHaveSent=sequenceNumber*DATA_SIZE;

                if(i==sendTimes-1){
                    setFinBit(0);
                }
                continue;
            }

            // 校验和有问题反正重来一遍。
            if(isCorrupt()){
                cout << "+---------------------+\n";
                cout << "| Got an unvalid ACK! |\n";
                cout << "+---------------------+\n";
                i--;
                sequenceNumber=i;

                bytesHaveRead -= sendSize;
                if (i == -1) isFirstPackage=true;
                continue;            
            }
        // 接收ACK

        }
        int t_end=clock();
        cout<<"Sent "<<bytesHaveSent<<" bytes, ";
        cout<<"Cost time: "<< t_end - t_start << "(ms), ";
        // numOfBit/time
        cout <<"Throughput rate: " << bytesHaveSent * 8 / (t_end - t_start) * CLOCKS_PER_SEC << " bps" << endl;
        fin.close();
    // 发送文件


        // client收到ACK回复之后，把i改成ackNumber。

        
    }
// 发送文件的循环


    WSACleanup();
    cout<<"We successfully shut down the socket."<<endl;

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

void shakeHand(){
    // 发送SYN报文，开始循环，直到接收到SYN+ACK为止。
    while(1){
        packSynDatagram(sequenceNumber);
        cout<<"We successfully packed the SYN datagram."<<endl;

        int sockaddrInLength=sizeof(sockaddr_in);

        sendto(cli_socket, sendBuffer, sizeof(sendBuffer), 0, (SOCKADDR*)&serveraddr, sockaddrInLength);
        cout<<"sent."<<endl;


        if(recvfrom(cli_socket,recvBuffer,sizeof(recvBuffer),0,(SOCKADDR*)&serveraddr,&sockaddrInLength)==SOCKET_ERROR)
            if (WSAGetLastError() == 10060)     printRTOErr();
            
        
        // 打破循环的唯一条件是
        if(getAckBit()&&getSynBit()){
            cout<<"hand shake succeed."<<endl;
            break;
        }
    }
}

void printRTOErr(){
    cout << "+------------------------------------------+\n";
    cout << "| Over RTO. The server did not respond us. |\n";
    cout << "+------------------------------------------+\n";
}