#include "offlinemessagemodel.hpp"
#include "connectionpool.hpp"

//存储用户的离线消息
void OfflineMsgModel::insert(int userid,string msg){
    char sql[1024]={0};
    sprintf(sql,"insert into offlinemessage values(%d,'%s');",userid,msg.c_str());
    shared_ptr<MySQL> sp=ConnectionPool::instance()->getConnection();
    if(sp){    
        sp->update(sql);
    }
}

//删除用户的离线消息
void OfflineMsgModel::remove(int userid){
    char sql[1024]={0};
    sprintf(sql,"delete from offlinemessage where userid=%d;",userid);
    shared_ptr<MySQL> sp=ConnectionPool::instance()->getConnection();
    if(sp){
        sp->update(sql);
    }
}

//查询用户的离线消息
vector<string> OfflineMsgModel::query(int userid){
    vector<string> vec;
    //组装sql语句
    char sql[1024]={0};
    sprintf(sql,"select message from offlinemessage where userid=%d;",userid);

    shared_ptr<MySQL> sp=ConnectionPool::instance()->getConnection();
    if(sp){
        MYSQL_RES *res=sp->query(sql);
        if(res!=nullptr){
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr){
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
        }
    }
    return vec;
}