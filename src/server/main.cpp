#include "chatserver.hpp"
#include<iostream>
#include<signal.h>
#include"chatservice.hpp"
using namespace std;
void resetHandler(int){
    //处理服务器ctrl+c结束后，重置user的状态信息：
    ChatService::instance()->reset();
    exit(0);
}


int main(int argc,char **argv){

    if(argc<3){

        cerr<<"comman invalid example: ./ChatServer 192.168.1.108:22"<<endl;
        exit(-1);
    }
    //解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);
    signal(SIGINT,resetHandler);
    EventLoop loop;
    InetAddress addr(ip ,port);
    // InetAddress addr("192.168.1.108" ,22);
    ChatServer server(&loop,addr,"ChatServer");
    server.start();
    loop.loop();
    return 0;
    }
