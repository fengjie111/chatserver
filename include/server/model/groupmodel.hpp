#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"

class GroupModel{
public:
    //创建群组
    bool createGroup(Group &group);

    //加入群组
    void addGroup(int userid,int groupid,string role);

    //查询用户所在群组信息
    vector<Group> queryGroups(int userid);

    //查询群组用户id列表,用于群聊天消息
    vector<int> queryGroupUsers(int userid,int groupid);

private:

};

#endif // !GROUPMODEL_H
