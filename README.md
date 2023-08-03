# ChatServer
可以工作在nginx tcp负载均衡环境中的集群聊天服务器和客户端源码 基于muduo实现 redis mysql
编译方式
cd build
rm -rf *
cmake ..
make

需要nginx的TCP负载均衡，基于发布-订阅模式的redis消息队列
