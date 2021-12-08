#include"timeStamp.h"
#include<iostream>
#include<time.h>
using namespace std;



//在消息的前16位加上时间戳，解析在另一个函数表示。
void getTime(char* res){
    struct tm t;   //tm结构指针
    time_t now;  //声明time_t类型变量
    time(&now);      //获取系统日期和时间
    localtime_s(&t, &now);   //获取当地日期和时间

    //格式化输出本地时间
    int i=0;
    res[i++]=(t.tm_year + 1900)/1000+'0';
    res[i++]=((t.tm_year + 1900)%1000)/100+'0';
    res[i++]=((t.tm_year + 1900)%100)/10+'0';
    res[i++]=(t.tm_year + 1900)%10+'0';

    res[i++]=(t.tm_mon + 1)/10+'0';
    res[i++]=(t.tm_mon + 1)%10+'0';

    res[i++]=(t.tm_mday)/10+'0';
    res[i++]=(t.tm_mday)%10+'0';

    res[i++]=(t.tm_hour)/10+'0';
    res[i++]=(t.tm_hour)%10+'0';

    res[i++]=(t.tm_min)/10+'0';
    res[i++]=(t.tm_min)%10+'0';

    res[i++]=(t.tm_sec)/10+'0';
    res[i++]=(t.tm_sec)%10+'0';


    res[i]=' ';


    return;
}




void timePrint(char* time, int color){
    
    //color=0, green
    //color=1, blue


    cout<<" ";

    if(color==0)
        cout<<"\033[32m";
    if(color==1)
        cout<<"\033[34m";


    int i=0;

    for(;i<4;i++)   cout<<time[i];
    cout<<"/";

    for(;i<6;i++)   cout<<time[i];
    cout<<"/";

    for(;i<8;i++)   cout<<time[i];
    cout<<" ";

    for(;i<10;i++)  cout<<time[i];
    cout<<":";

    for(;i<12;i++)  cout<<time[i];
    cout<<":";

    for(;i<14;i++)  cout<<time[i];
    cout<<"\n";

    cout<<"\033[0m";

}