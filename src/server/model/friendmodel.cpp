#include "friendmodel.hpp"
#include "connectionpool.hpp"
#include <functional>

//添加好友关系
void FriendModel::insert(int userid,int friendid){
    char sql[1024]={0};
    sprintf(sql,"insert into friend values(%d,%d)",userid,friendid);
    
    shared_ptr<MySQL> sp=ConnectionPool::instance()->getConnection();
    if(sp){
        sp->update(sql);
    }
}

//返回用户好友列表
vector<User> FriendModel::query(int userid){
    char sql[1024]={0};
    sprintf(sql,"select id,name,state from user inner join friend on user.id=friend.friendid where friend.userid=%d",userid);

    shared_ptr<MySQL> sp=ConnectionPool::instance()->getConnection();
    vector<User> vec;
    if(sp){
        MYSQL_RES *res=sp->query(sql);
        if(res!=nullptr){
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr){
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
            mysql_free_result(res);
        }
    }
    return vec;
}