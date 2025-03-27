#include "connectionpool.hpp"


ConnectionPool* ConnectionPool::instance(){
    static ConnectionPool pool;
    return &pool;
}
ConnectionPool::ConnectionPool(){
    if(loadConfigFile()==false){
        return;
    }

    for(int i=0;i<_initSize;i++){
        MySQL* p=new MySQL();
        p->connect(_ip,_user,_password,_dbname,_port);
        _connectionQue.push(p);
        _connectionCnt++;
    }

    //启动一个新的线程，作为生产者
    thread produce(std::bind(&ConnectionPool::produceConnectionTask,this));
    produce.detach();
    //启动一个新的线程，扫描超过maxIdleTime的空闲连接，回收连接
    thread scanner(std::bind(&ConnectionPool::scannerConnectionTask,this));
    scanner.detach();
}

//从配置文件中加载配置项
bool ConnectionPool::loadConfigFile(){
    FILE *pf=fopen("/home/fengjie/learning/chat/chatserver/src/server/db/mysql.cnf","r");
    if(pf==nullptr){
        LOG_INFO << "mysql.cnf is not exist!";
        return false;
    }
    while(!feof(pf)){
        char line[1024]={0};
        fgets(line,1024,pf);
        string str=line;
        int idx=str.find("=",0);
        if(idx==-1){
            continue;
        }
        int endidx=str.find("\n",idx);
        string key=str.substr(0,idx);
        string value=str.substr(idx+1,endidx-idx-1);
        if(key=="ip"){
            _ip=value;
        }
        else if(key=="port"){
            _port=atoi(value.c_str());
        }
        else if(key=="user"){
            _user=value;
        }
        else if(key=="password"){
            _password=value;
        }
        else if(key=="dbname"){
            _dbname=value;
        }
        else if(key=="initSize"){
            _initSize=atoi(value.c_str());
        }
        else if(key=="MAXSize"){
            _maxSize=atoi(value.c_str());
        }
        else if(key=="maxIdleTime"){
            _maxIdleTime=atoi(value.c_str());
        }
        else if(key=="connectionTimeout"){
            _connectionTimeout=atoi(value.c_str());
        }
    }
    return true;
}

//运行在独立的线程中，专门负责生产新连接
void ConnectionPool::produceConnectionTask(){
    for(;;){
        unique_lock<mutex> lock(_queueMutex);
        while(!_connectionQue.empty()){
            cv.wait(lock);  //队列不空，生产线程进入等待状态
        }
        if(_connectionCnt<_maxSize){
            MySQL* p=new MySQL();
            p->connect(_ip,_user,_password,_dbname,_port);
            p->refreshAliveTime();
            _connectionQue.push(p);
            _connectionCnt++;
            //通知消费者线程，可以消费连接
            cv.notify_all();
        }
    }
}

//给外部提供接口，从连接池中获取一个可用的空闲连接
shared_ptr<MySQL> ConnectionPool::getConnection(){
    unique_lock<mutex> lock(_queueMutex);
    while(_connectionQue.empty()){
        if(cv_status::timeout==cv.wait_for(lock,chrono::microseconds(_connectionTimeout))){
            if(_connectionQue.empty()){
                LOG_INFO<<"获取空闲连接超时.......获取连接失败!";
                return nullptr;
            }
        }
    }

    shared_ptr<MySQL> sp(_connectionQue.front(),[&](MySQL* pcon){
        unique_lock<mutex> lock(_queueMutex);
        pcon->refreshAliveTime();
        _connectionQue.push(pcon);
        cv.notify_all();
    });
    _connectionQue.pop();
    cv.notify_all();

    return sp;
}

//运行在独立线程中扫描超过maxIdleTime的空闲连接，回收连接
void ConnectionPool::scannerConnectionTask(){
    for(;;){
        //通过sleep模拟定时效果
        this_thread::sleep_for(chrono::seconds(_maxIdleTime));

        //扫描整个队列
        unique_lock<mutex> lock(_queueMutex);
        while(_connectionCnt>_initSize){
            MySQL* p=_connectionQue.front();
            if(p->getAliveTime()>=(_maxIdleTime*1000)){
                _connectionQue.pop();
                _connectionCnt--;
                delete p;
            }
            else{
                break;
            }
        }
    }
}