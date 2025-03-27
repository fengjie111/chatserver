#ifndef CONNECTIONPOOL_H
#define CONNECTIONPOOL_H

#include <string>
#include <queue>
#include <mutex>
#include "db.h"
#include <atomic>
#include <thread>
#include <memory>
#include <functional>
#include <condition_variable>
using namespace std;

class ConnectionPool{
public:
    static ConnectionPool* instance();
    //给外部提供接口，从连接池中获取一个可用的空闲连接
    shared_ptr<MySQL> getConnection();

private:
    ConnectionPool();
    //从配置文件中加载配置项
    bool loadConfigFile();
    //运行在独立的线程中，专门负责生产新连接
    void produceConnectionTask();
    //运行在独立线程中扫描超过maxIdleTime的空闲连接，回收连接
    void scannerConnectionTask();

    string _ip;
    string _user;
    string _password;
    string _dbname;
    int _port;
    
    int _initSize;
    int _maxSize;
    int _maxIdleTime;
    int _connectionTimeout;

    queue<MySQL*> _connectionQue;
    mutex _queueMutex;
    atomic_int _connectionCnt;
    condition_variable cv;  //用于生产线程和消费线程通信
};

#endif // !CONNECTIONPOLL_H
