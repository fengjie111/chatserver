#include "user.hpp"
#include "group.hpp"
#include "json.hpp"
#include "public.hpp"
using json=nlohmann::json;

#include <iostream>
#include <vector>
#include <ctime>
#include <thread>
#include <chrono>
#include <string>
using namespace std;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <atomic>


//记录当前登录的用户信息
User g_currentUser;
//记录当前登录的好友列表消息
vector<User> g_currentUserFriendList;
//记录当前登录的群组列表信息
vector<Group> g_currentUserGroupList;
//控制主菜单页面程序
bool isMainMenuRunning=false;
//用于读写线程之间的通信
sem_t rwsem;
//记录登录状态
atomic_bool g_isLoginSuccess{false};

//显示当前登录的用户信息
void showCurrentUserData(int fd=0,string str="");
//接收线程
void readTaskHandler(int clientfd);
//获取系统时间
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}
//主聊天页面程序
void mainMenu(int clientfd);


//聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, const char** argv) {
    if(argc<3){
        cerr<<"command invalid! example:./ChatClient 192.168.163.128 8000"<<endl;
        exit(-1);
    }
    const char *ip=argv[1];
    uint16_t port=atoi(argv[2]);

    int clientfd=socket(AF_INET,SOCK_STREAM,0);
    if(clientfd==-1){
        cerr<<"socket create error"<<endl;
        exit(-1);
    }

    sockaddr_in server;
    memset(&server,0,sizeof(sockaddr_in));

    server.sin_family=AF_INET;
    server.sin_port=htons(port);
    server.sin_addr.s_addr=inet_addr(ip);

    if(connect(clientfd,(sockaddr *)&server,sizeof(sockaddr_in))){
        cerr<<"connect server error"<<endl;
        close(clientfd);
        exit(-1);
    }

    sem_init(&rwsem,0,0);
    std::thread readTask(readTaskHandler,clientfd);
    readTask.detach();
    
    //main线程用于接收用户输入，负责发送数据
    for(;;){
        cout<<"==================="<<endl;
        cout<<"1. login"<<endl;
        cout<<"2. register"<<endl;
        cout<<"3. quit"<<endl;
        cout<<"==================="<<endl;
        cout<<"choise:";
        int chiose=0;
        cin>>chiose;
        cin.get();

        switch(chiose){
            case 1:{
                int id=0;
                char pwd[50]={0};
                cout<<"userid:";
                cin>>id;
                cin.get();
                cout<<"userpassword:";
                cin.getline(pwd,50);

                json js;
                js["msgid"]=LOGIN_MSG;
                js["id"]=id;
                js["password"]=pwd;
                string request=js.dump();

                g_isLoginSuccess=false;
                int len=send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
                if(len==-1){
                    cerr<<"send login msg error:"<<request<<endl;
                }
                sem_wait(&rwsem);
                if(g_isLoginSuccess==true){
                    isMainMenuRunning=true;
                    mainMenu(clientfd);
                }
                break;
            }
            case 2:{
                char name[50]={0};
                char pwd[50]={0};
                cout<<"username:";
                cin.getline(name,50);
                cout<<"password:";
                cin.getline(pwd,50);

                json js;
                js["msgid"]=REG_MSG;
                js["name"]=name;
                js["password"]=pwd;
                string request=js.dump();
                int len=send(clientfd,request.c_str(),strlen(request.c_str())+1,0);
                if(len==-1){
                    cerr<<"send reg msg error:"<<request<<endl;
                }
                sem_wait(&rwsem);
                break;
            }
            case 3:{
                close(clientfd);
                sem_destroy(&rwsem);
                exit(0);
            }
            default:{
                cerr<<"invalid input!"<<endl;
                break;
            }
        }
    }

    return 0;
}

//处理登录的响应
void doLoginResponse(json &responsejs){
    if(responsejs["erron"].get<int>()!=0){
        cerr<<responsejs["errmsg"]<<endl;
        g_isLoginSuccess=false;
    }
    else{
        g_currentUser.setId(responsejs["id"].get<int>());
        g_currentUser.setName(responsejs["name"]);
        g_currentUser.setState("online");
        g_currentUserFriendList.clear();
        if(responsejs.contains("friends")){
            for(string str:responsejs["friends"]){
                json js=json::parse(str);
                g_currentUserFriendList.emplace_back(js["id"].get<int>(),js["name"],js["state"]);
            }
        }
        g_currentUserGroupList.clear();
        if(responsejs.contains("groups")){
            for(string groupstr:responsejs["groups"]){
                json groupjs=json::parse(groupstr);
                Group group(groupjs["id"].get<int>(),groupjs["groupname"],groupjs["groupdesc"]);
                for(string userstr:groupjs["users"]){
                    json userjs=json::parse(userstr);
                    GroupUser groupuser(userjs["id"].get<int>(),userjs["name"],userjs["state"]);
                    groupuser.setRole(userjs["role"]);
                    group.getUsers().push_back(groupuser);
                }
                g_currentUserGroupList.push_back(group);
            }
        }
        showCurrentUserData();

        if(responsejs.contains("offlinemsg")){
            for(string str:responsejs["offlinemsg"]){
                json js=json::parse(str);
                if(js.contains("groupid")){
                    cout<<"群消息"<<"["<<js["groupid"]<<"]"<<js["time"]<<"["<<js["id"]<<"]"<<js["name"]<<" said:"<<js["msg"]<<endl;
                }
                else{
                    cout<<js["time"]<<"["<<js["id"]<<"]"<<js["name"]<<" said:"<<js["msg"]<<endl;
                }
            }
        }
        g_isLoginSuccess=true;
    }
}

//处理注册的响应
void doRegResponse(json& responsejs){
    if(responsejs["erron"].get<int>()!=0){
        cerr<<responsejs["name"]<<" is already exist, register error!"<<endl;
    }
    else{
        cout<<responsejs["name"]<<" register success, userid is:"<<responsejs["id"]<<endl;
    }
}

//接收线程
void readTaskHandler(int clientfd){
    for(;;){
        char buffer[1024]={0};
        int len=recv(clientfd,buffer,1024,0);
        if(len==-1){
            cerr<<"recv error!"<<endl;
            close(clientfd);
            exit(-1);
        }

        json js=json::parse(buffer);
        int msgtype=js["msgid"].get<int>();
        if(ONE_CHAT_MSG==msgtype){
            cout<<js["time"]<<"["<<js["id"]<<"]"<<js["name"]<<" said:"<<js["msg"]<<endl;
            continue;
        }
        if(GROUP_CHAT_MSG==msgtype){
            cout<<"群消息"<<"["<<js["groupid"]<<"]"<<js["time"]<<"["<<js["id"]<<"]"<<js["name"]<<" said:"<<js["msg"]<<endl;
            continue;
        }
        if(LOGIN_MSG_ACK==msgtype){
            doLoginResponse(js);
            sem_post(&rwsem);
            continue;
        }
        if(REG_MSG_ACK==msgtype){
            doRegResponse(js);
            sem_post(&rwsem);
            continue;
        }
    }
}

void help(int fd=0,string str="");
void chat(int,string);
void addfriend(int,string);
void creategroup(int,string);
void addgroup(int,string);
void groupchat(int,string);
void quit(int,string);
//系统支持的客户端命令列表
unordered_map<string,string> commandMap={
    {"help","显示所有支持的命令，格式help"},
    {"chat","一对一聊天，格式chat:friendid:message"},
    {"addfriend","添加好友，格式addfriend:friendid"},
    {"creategroup","创建群组，格式creategroup:groupname:groupdesc"},
    {"addgroup","加入群组，格式addgroup:groupid"},
    {"groupchat","群聊，格式groupchat:groupid:message"},
    {"quit","注销，格式quit"},
    {"show","显示用户信息，格式show"}
};
//注册系统支持的客户端命令处理
unordered_map<string,function<void(int,string)>> commandHandlerMap={
    {"help",help},
    {"chat",chat},
    {"addfriend",addfriend},
    {"creategroup",creategroup},
    {"addgroup",addgroup},
    {"groupchat",groupchat},
    {"quit",quit},
    {"show",showCurrentUserData}
};

//主聊天页面程序
void mainMenu(int clientfd){
    help();

    char buffer[1024]={0};
    while(isMainMenuRunning){
        cin.getline(buffer,1024);
        string commandbuf(buffer);
        string command;
        int idx=commandbuf.find(":");
        if(idx==-1){
            command=commandbuf;
        }
        else{
            command=commandbuf.substr(0,idx);
        }
        auto it=commandHandlerMap.find(command);
        if(it==commandHandlerMap.end()){
            cerr<<"invalid input command!"<<endl;
            continue;
        }
        it->second(clientfd,commandbuf.substr(idx+1,commandbuf.size()-idx));
    }
}

void help(int,string){
    cout<<"show command list"<<endl;
    for(auto& cmd:commandMap){
        cout<<cmd.first<<":"<<cmd.second<<endl;
    }
    cout<<endl;
}

void chat(int clientfd,string str){
    int idx=str.find(":");
    if(idx==-1){
        cerr<<"chat command invalid!"<<endl;
        return;
    }
    int friendid=atoi(str.substr(0,idx).c_str());
    string message=str.substr(idx+1,str.size()-idx);
    json js;
    js["msgid"]=ONE_CHAT_MSG;
    js["id"]=g_currentUser.getId();
    js["name"]=g_currentUser.getName();
    js["to"]=friendid;
    js["msg"]=message;
    js["time"]=getCurrentTime();
    
    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1){
        cerr<<"send chat msg error ->"<<buffer<<endl;
    }
}

void addfriend(int clientfd,string str){
    int friendid=atoi(str.c_str());
    json js;
    js["msgid"]=ADD_FRIEND_MSG;
    js["id"]=g_currentUser.getId();
    js["friendid"]=friendid;

    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1){
        cerr<<"send addfriend msg error ->"<<buffer<<endl;
    }
}
void creategroup(int clientfd,string str){
    int idx=str.find(":");
    if(idx==-1){
        cerr<<"creategroup command invalid!"<<endl;
    }
    string groupname=str.substr(0,idx);
    string groupdesc=str.substr(idx+1,str.size()-idx);
    json js;
    js["msgid"]=CREATE_GROUP_MSG;
    js["id"]=g_currentUser.getId();
    js["groupname"]=groupname;
    js["groupdesc"]=groupdesc;

    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1){
        cerr<<"send creategroup msg error ->"<<buffer<<endl;
    }

}
void addgroup(int clientfd,string str){
    int groupid=atoi(str.c_str());

    json js;
    js["msgid"]=ADD_GROUP_MSG;
    js["id"]=g_currentUser.getId();
    js["groupid"]=groupid;

    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1){
        cerr<<"send addgroup msg error ->"<<buffer<<endl;
    }
}
void groupchat(int clientfd,string str){
    int idx=str.find(":");
    if(idx==-1){
        cerr<<"groupchat command invalid!"<<endl;
    }
    int groupid=atoi(str.substr(0,idx).c_str());
    string message=str.substr(idx+1,str.size()-idx);

    json js;
    js["msgid"]=GROUP_CHAT_MSG;
    js["groupid"]=groupid;
    js["id"]=g_currentUser.getId();
    js["msg"]=message;
    js["name"]=g_currentUser.getName();
    js["time"]=getCurrentTime();

    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1){
        cerr<<"send groupchat msg error ->"<<buffer<<endl;
    }
}
void quit(int clientfd,string){
    json js;
    js["msgid"]=LOGINOUT_MSG;
    js["id"]=g_currentUser.getId();

    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1){
        cerr<<"send loginout msg error ->"<<buffer<<endl;
    }
    else{
        isMainMenuRunning=false;
    }
    
}

void showCurrentUserData(int,string){
    cout<<"=================login user================="<<endl;
    cout<<"current login user => id:"<<g_currentUser.getId()<<" name:"<<g_currentUser.getName()<<endl;
    cout<<"-----------------friend list-----------------"<<endl;
    if(!g_currentUserFriendList.empty()){
        for(User& user:g_currentUserFriendList){
            cout<<user.getId()<<"  "<<user.getName()<<"  "<<user.getState()<<endl;
        }
    }
    cout<<"-----------------group list-----------------"<<endl;
    if(!g_currentUserGroupList.empty()){
        for(Group& group:g_currentUserGroupList){
            printf("%4d  ",group.getId());
            cout<<group.getName()<<"  "<<group.getDesc()<<endl;
            for(GroupUser& user:group.getUsers()){
                cout<<"      "<<user.getId()<<"  "<<user.getName()<<"  "<<user.getState()<<"  "<<user.getRole()<<endl;
            }
        }
    }
    cout<<"==========================================="<<endl;
}