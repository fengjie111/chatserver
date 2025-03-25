# chatserver
可以工作在nginx tcp负载均衡器中的集群聊天服务器和客户端源码，基于muduo实现\n
\n
编译方式\n
cd bulid\n
rm -rf *\n
cmake ..\n
make\n
\n
需要nginx tcp负载均衡，启动redis，启动mysql
