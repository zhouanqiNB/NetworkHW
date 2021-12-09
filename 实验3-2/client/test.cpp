#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <time.h>
#include <Winsock2.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <ctime>
#include <io.h>
#include <cmath>

using namespace std;

void set(int* array,int number);

int main(){
	int n;
	cin>>n;
	cout<<n<<endl;
	bool* window=new bool[n];
	for(int i=0;i<n;i++){
		window[i]=0;
		cout<<"set"<<i<<endl;
	}
	for(int i=0;i<n;i++){
		cout<<window[i];
	}
	cout<<endl;
	window[2]=1;
	for(int i=0;i<n;i++){
		cout<<window[i];
	}
}
