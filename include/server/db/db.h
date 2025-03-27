#ifndef DB_H
#define DB_H

#include <mysql/mysql.h>
#include <string>
#include <muduo/base/Logging.h>
#include <ctime>
using namespace std;

// 数据库操作类
class MySQL
{
public:
    // 初始化数据库连接
    MySQL();
    // 释放数据库连接资源
    ~MySQL();
    // 连接数据库
    bool connect(string ip,string user,string password,string dbname,int port);
    // 更新操作
    bool update(string sql);
    // 查询操作
    MYSQL_RES *query(string sql);
    //获取连接
    MYSQL* getConnection(); 
    //刷新连接的起始空闲时间点
    void refreshAliveTime(){_alivetime=clock();};
    //返回空闲时间
    clock_t getAliveTime()const{return clock()-_alivetime;};
private:
    MYSQL *_conn;
    clock_t _alivetime; //记录进入空闲状态后的时间
};

#endif