项目名称：基于moduo网络库实现的ChatServer服务器
项目地址：https://github.com/dukai1255/ChatServer
平台工具：VsCode远程Linux(Ubuntu 18.04)开发 CMake构建集成编译环境 Linux shell输出项目编译脚本
项目内容：
1.使用muduo网络库作为项目的网络核心模块，解耦网络模块、业务模块和数据模块代码；
2.使用json序列化和反序列化消息作为私有通信协议；
3.配置Nginx基于权重的Tcp负载均衡算法，实现聊天服务器的集群功能，提高后端服务的并发能力；
4.基于发布-订阅的redis消息队列，实现跨服务器的消息通信；
5.使用mysql关系型数据库作为项目数据的落地存储；
6.使用连接池提高数据库的数据存取性能.


