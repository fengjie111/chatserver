#include "groupmodel.hpp"
#include "connectionpool.hpp"

//创建群组
bool GroupModel::createGroup(Group &group){
    char sql[1024]={0};
    sprintf(sql,"insert into allgroup(groupname,groupdesc) values('%s','%s')",group.getName().c_str(),group.getDesc().c_str());

    shared_ptr<MySQL> sp=ConnectionPool::instance()->getConnection();
    if(sp){
        if(sp->update(sql)){
            group.setId(mysql_insert_id(sp->getConnection()));
            return true;
        }
    }

    return false;
}

//加入群组
void GroupModel::addGroup(int userid,int groupid,string role){
    char sql[1024]={0};
    sprintf(sql,"insert into groupuser(groupid,userid,grouprole) values(%d,%d,'%s')",groupid,userid,role.c_str());

    shared_ptr<MySQL> sp=ConnectionPool::instance()->getConnection();
    if(sp){
        sp->update(sql);
    }
}

//查询用户所在群组信息
vector<Group> GroupModel::queryGroups(int userid){
    char sql[1024]={0};
    sprintf(sql,"select id,groupname,groupdesc from allgroup inner join groupuser on allgroup.id=groupuser.groupid where groupuser.userid=%d",userid);

    vector<Group> groupvec;
    shared_ptr<MySQL> sp=ConnectionPool::instance()->getConnection();
    if(sp){
        MYSQL_RES* res=sp->query(sql);
        if(res!=nullptr){
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr){
                groupvec.emplace_back(atoi(row[0]),row[1],row[2]);
            }
            mysql_free_result(res);
        }
    }

    for(auto& group:groupvec){
        sprintf(sql,"select user.id,user.name,user.state,groupuser.grouprole from user inner join groupuser on user.id=groupuser.userid where groupid=%d",group.getId());

        MYSQL_RES* res=sp->query(sql);
        if(res!=nullptr){
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr){
                GroupUser groupuser(atoi(row[0]),row[1],row[2]);
                groupuser.setRole(row[3]);
                group.getUsers().push_back(groupuser);
            }
            mysql_free_result(res);
        }        
    }

    return groupvec;
}

//查询群组用户id列表,用于群聊天消息
vector<int> GroupModel::queryGroupUsers(int userid,int groupid){
    char sql[1024]={0};
    sprintf(sql,"select userid from groupuser where groupid=%d and userid!=%d",groupid,userid);

    vector<int> useridvec;
    shared_ptr<MySQL> sp=ConnectionPool::instance()->getConnection();
    if(sp){
        MYSQL_RES* res=sp->query(sql);
        if(res!=nullptr){
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr){
                useridvec.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
    }
    return useridvec;
}