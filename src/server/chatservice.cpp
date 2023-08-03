#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include "string"
#include <vector>
#include <map>
using namespace muduo;
using namespace std;
// 获取单例对象的接口函数：
ChatService *ChatService::instance()
{

    static ChatService service;
    return &service;
}
// 服务器异常，业务重置方法
void ChatService::reset()
{

    // 把online状态的用户，设置为offline；

    _userModel.resetState();
}
// 注册消息以及对应的回调操作：
ChatService::ChatService()
{

    _MsghandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _MsghandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _MsghandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _MsghandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _MsghandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    _MsghandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _MsghandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _MsghandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
    // 连接redis服务器：

    if (_redis.connect())
    {
        // 设置上报消息的回调：
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));

        
        LOG_ERROR << "shangbaohuidiaoxiaoxi" ;
    }
}

// 获取消息对应的处理器：
MsgHandler ChatService::getHandler(int msgid)
{
    
    // 记录错误日志
    auto it = _MsghandlerMap.find(msgid);
    if (it == _MsghandlerMap.end())
    {
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp)
        {
            LOG_ERROR << "msgid:" << msgid << "can not find handler";
        };
    }
    else
    {
        return _MsghandlerMap[msgid];
    }
}
// 处理登录业务
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    string pwd = js["password"];
    User user = _userModel.query(id);
    if (user.getId() == id && user.getPwd() == pwd)
    {

        if (user.getState() == "online")
        {
            // 该用户已经登陆，不允许重复登录：
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "this account is using, input another!";

            conn->send(response.dump());
        }
        else
        {

            // 登录成功，记录用户的连接信息：
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }
            // id登录成功后，向redis订阅channel(id)
            _redis.subscribe(id);

            // 登录成功，更新用户状态信息：state offline=>onine
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 查询该用户是否有离线消息：

            vector<string> vec = _offlineMsgModel.qurey(id);

            if (!vec.empty())
            {
                response["offlinemsg"] = vec;
                // 读取该用户的离线消息后，删除该用户所有的离线消息：
                _offlineMsgModel.remove(id);
            }
            // 查询该用户的好友的信息并返回：
            vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty())
            {

                vector<string> vec2;
                for (User &user : userVec)
                {

                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();

                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }
            // 查询用户的群组信息：
            vector<Group> groupusersVec = _groupModel.queryGroups(id);
            if (!groupusersVec.empty())
            {

                vector<string> groupV;

                for (Group &group : groupusersVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    // userV中存放的时一条条的json字符串，这一条条字符串中包含了每一个GroupUser中的信息；
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();

                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }
                response["groups"] = groupV;
            }
            // response这个json字符串中就包含了，当一个用户登录成功时，要发给这个用户的所有json字符串消息；
            conn->send(response.dump());
        }
    }
    else // 登录失败：
    {
        if (user.getId() == -1)
        {
            // 该用户不存在：
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 1;
            response["errmsg"] = "The user does not exist, please register！";
            conn->send(response.dump());
        }
        else
        {

            // 如果id既不等于-1，然后id和password也不符合，那说明时密码输错了：
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "The login password is incorrect, please re-enter the password";
            conn->send(response.dump());
        }
    }
}

// 处理注册业务：
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];
    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if (state)
    {
        // 注册成功：
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败：
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}
// 处理注销业务：
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{

    int userid = js["id"].get<int>();
    {
        // 线程互斥：
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {

            _userConnMap.erase(it);
        }
    }
    // 取消用户，相当于就是下线，在redis中取消订阅通道：
    _redis.unsubscribe(userid);
    // 更新用户的状态信息：
    User user;
    user.setId(userid);
    user.setState("offline");
    _userModel.updateState(user);
}
// 处理客户端异常退出：
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    { // 在这个大括号的作用域下保证线程安全：
        lock_guard<mutex> lock(_connMutex);

        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); it++)
        {
            if (it->second == conn)
            {

                // 从map表中删除用户的链接信息：
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    // 用户注销，相当于就是下线，在redis中取消订阅：
    _redis.unsubscribe(user.getId());
    // 更新用户的状态信息：
    if (user.getId() != -1)
    {

        user.setState("offline");
        _userModel.updateState(user);
    }
}
// 一对一聊天业务：
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{

    int toid = js["toid"].get<int>();

    {

        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            // toid在线，转发消息  服务器主动推送消息给toid
            // json response;
            // response["msg"] = js["msg"];
            it->second->send(js.dump());
            return;
        }
    }
    // 查询toid是否在线：
    User user = _userModel.query(toid);
    if (user.getState() == "online")
    {

        _redis.publish(toid, js.dump());
        return;
    }

    // toid不在线，存储离线消息：
    _offlineMsgModel.insert(toid, js.dump());
}
// 添加好友业务：
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    _friendModel.insert(userid, friendid);
}
// 创建群组业务：

void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{

    int userid = js["id"].get<int>();

    string name = js["groupname"];
    string desc = js["groupdesc"];
    // 存储创建的群组信息：
    Group group(-1, name, desc);
    if (_groupModel.createGroup(group))
    {

        // 存储群组创建人信息：

        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}
// 加入群组业务：

void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}
// 群组聊天业务：

void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{

    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    vector<int> useridVec = _groupModel.queryGroupsUsers(userid, groupid);

    lock_guard<mutex> lock(_connMutex);

    for (int id : useridVec)
    {

        auto it = _userConnMap.find(id);

        if (it != _userConnMap.end())
        {

            // 转发群消息：
            it->second->send(js.dump());
        }
        else
        {
            // 查询toid是否在线：
            User user = _userModel.query(id);
            if (user.getState() == "online")
            {
                _redis.publish(id, js.dump());
            }
            else
            {   //存储离线群消息：
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}
// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{LOG_ERROR << "fasongxiaoxi";
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    // 存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}
