#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <map>
using namespace muduo;

//获取单例对象的接口函数
ChatService* ChatService::instance(){
    static ChatService service;
    return &service;
}

//注册消息以及对应的Handler回调操作
ChatService::ChatService(){
    _msgHandlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG,std::bind(&ChatService::createGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG,std::bind(&ChatService::addGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG,std::bind(&ChatService::groupChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGINOUT_MSG,std::bind(&ChatService::loginout,this,_1,_2,_3)});

    if(_redis.connect()){
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubcribeMessage,this,_1,_2));
    }
}

//登录业务
void ChatService::login(const TcpConnectionPtr& conn,json& js,Timestamp time){
    int id=js["id"].get<int>();
    string password=js["password"];

    User user=_userModel.query(id);
    if(user.getId()!=-1 && user.getPassword() == password){
        if(user.getState()=="online"){
            json response;
            response["msgid"]=LOGIN_MSG_ACK;
            response["erron"]=2;
            response["errmsg"]="该账号已经登录";

            conn->send(response.dump());
        }
        else{
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id,conn});
            }

            _redis.subscribe(id);

            user.setState("online");
            _userModel.updateState(user);
            json response;
            response["msgid"]=LOGIN_MSG_ACK;
            response["erron"]=0;
            response["id"]=user.getId();
            response["name"]=user.getName();


            vector<string> vec=_offlineMsgModel.query(id);
            if(!vec.empty()){
                response["offlinemsg"]=vec;
                _offlineMsgModel.remove(id);
            }

            vector<User> uservec=_friendModel.query(id);
            if(!uservec.empty()){
                vector<string> vec2;
                for(auto &user:uservec){
                    json js;
                    js["id"]=user.getId();
                    js["name"]=user.getName();
                    js["state"]=user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"]=vec2;
            }
            vector<Group> groupvec=_groupModel.queryGroups(id);
            if(!groupvec.empty()){
                vector<string> groupv;
                for(Group& group:groupvec){
                    json groupjs;
                    groupjs["id"]=group.getId();
                    groupjs["groupname"]=group.getName();
                    groupjs["groupdesc"]=group.getDesc();
                    vector<string> userv;
                    for(GroupUser& user:group.getUsers()){
                        json userjs;
                        userjs["role"]=user.getRole();
                        userjs["id"]=user.getId();
                        userjs["name"]=user.getName();
                        userjs["state"]=user.getState();
                        userv.push_back(userjs.dump());
                    }
                    groupjs["users"]=userv;
                    groupv.push_back(groupjs.dump());
                }
                response["groups"]=groupv;
            }
            conn->send(response.dump());
        }
    }
    else{
        json response;
        response["msgid"]=LOGIN_MSG_ACK;
        response["erron"]=1;
        response["errmsg"]="用户名或者密码错误";
        conn->send(response.dump());
    }
    
}
//注册业务
void ChatService::reg(const TcpConnectionPtr& conn,json& js,Timestamp time){
    string name=js["name"];
    string password=js["password"];

    User user;
    user.setName(name);
    user.setPassword(password);
    bool state=_userModel.insert(user);
    if(state){
        json response;
        response["msgid"]=REG_MSG_ACK;
        response["erron"]=0;
        response["id"]=user.getId();
        conn->send(response.dump());
    }
    else{
        json response;
        response["msgid"]=REG_MSG_ACK;
        response["erron"]=1;
        conn->send(response.dump());
    }

}

//获取消息对应的处理器
MsgHandler ChatService::getHandle(int msgid){
    auto it=_msgHandlerMap.find(msgid);
    //记录错误日志
    if(it==_msgHandlerMap.end()){
        //返回一个默认的处理器
        return [=](const TcpConnectionPtr& conn,json& js,Timestamp time){
            LOG_ERROR<< "msgid: "<<msgid<<" can not find handler!";
        };
    }
    else{
        return _msgHandlerMap[msgid];
    }

}

//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr& conn){
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for(auto it=_userConnMap.begin();it!=_userConnMap.end();it++){
            if(it->second==conn){
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    _redis.unsubscribe(user.getId());

    if(user.getId()!=-1){
        user.setState("offline");
        _userModel.updateState(user);
    }
}

//一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr& conn,json& js,Timestamp time){
    int toid=js["to"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it=_userConnMap.find(toid);
        if(it!=_userConnMap.end()){
            it->second->send(js.dump());
            return;
        }
    }
    User user=_userModel.query(toid);
    if(user.getState()=="online"){
        _redis.publish(user.getId(),js.dump());
    }
    else{
        _offlineMsgModel.insert(toid,js.dump());
    }
}

//服务器异常，业务重置方法
void ChatService::reset(){
    _userModel.resetState();
}

//添加好友业务
void ChatService::addFriend(const TcpConnectionPtr& conn,json& js,Timestamp time){
    int userid=js["id"].get<int>();
    int friendid=js["friendid"].get<int>();

    _friendModel.insert(userid,friendid);
}

//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr& conn,json& js,Timestamp time){
    int userid=js["id"].get<int>();
    string groupname=js["groupname"];
    string groupdesc=js["groupdesc"];

    Group group(-1,groupname,groupdesc);
    if(_groupModel.createGroup(group)){
        _groupModel.addGroup(userid,group.getId(),"creator");
    }
}

//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr& conn,json& js,Timestamp time){
    int userid=js["id"].get<int>();
    int groupid=js["groupid"].get<int>();
    _groupModel.addGroup(userid,groupid,"normal");
}

//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr& conn,json& js,Timestamp time){
    int userid=js["id"].get<int>();
    int groupid=js["groupid"].get<int>();
    vector<int> uservec=_groupModel.queryGroupUsers(userid,groupid);
    {
        lock_guard<mutex> lock(_connMutex);
        for(int& id:uservec){
            auto it=_userConnMap.find(id);
            if(it!=_userConnMap.end()){
                it->second->send(js.dump());
            }
            else{
                User user=_userModel.query(id);
                if(user.getState()=="online"){
                    _redis.publish(user.getId(),js.dump());
                }
                else{
                    _offlineMsgModel.insert(id,js.dump());
                }

            }
        }
    }
}
//处理注销业务
void ChatService::loginout(const TcpConnectionPtr& conn,json& js,Timestamp time){
    int userid=js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it=_userConnMap.find(userid);
        if(it!=_userConnMap.end()){
            _userConnMap.erase(it);
        }
    }

    _redis.unsubscribe(userid);

    User user(userid,"","offline");
    _userModel.updateState(user);
}

//从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubcribeMessage(int userid,string msg){
    {    
        lock_guard<mutex> lock(_connMutex);
        auto it=_userConnMap.find(userid);
        if(it!=_userConnMap.end()){
            it->second->send(msg);
            return;
        }
    }
    _offlineMsgModel.insert(userid,msg);
}