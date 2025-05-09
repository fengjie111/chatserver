#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h>

using namespace std;

//处理服务器ctrl+c结束后，重置user的状态信息
void resetHandle(int){
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc, const char** argv){
    if(argc<3){
        cerr<<"command invalid! example:./ChatServer 192.168.163.128 6000"<<endl;
        exit(-1);
    }
    const char *ip=argv[1];
    uint16_t port=atoi(argv[2]);
    signal(SIGINT,resetHandle);

    EventLoop loop;
    InetAddress addr(ip,port);
    ChatServer server(&loop,addr,"ChatServer");

    server.start();
    loop.loop();

    return 0;
}
