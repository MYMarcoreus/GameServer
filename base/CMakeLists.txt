cmake_minimum_required(VERSION 3.22)
project(GameServer)
set(CMAKE_CXX_STANDARD 20)

message("in base!")

set(LocalSrc
        public/md5/md5.cpp
        public/tinyxml/tinyxml2.cpp
        log/log.cpp
        config/ConfigManager.cpp
        config/AppXmlConfig.cpp
        config/LogXmlConfig.cpp
        config/RemoteXmlConfig.cpp
        utils/util_functions.cpp
        utils/BoundedQueue.hpp
        utils/ThreadPool.cpp
        utils/status/Status.cpp
        net/IPAddress.cpp
        net/Channel.cpp
        net/EpollPoller.cpp
        net/EventLoop.cpp
        net/Poller.cpp
        net/NewDefaultPoller.cpp
        net/Timestamp.cpp
        net/TimerManager.cpp
        net/Timer.cpp
        net/EventLoopThread.cpp
        net/Acceptor.cpp
        net/Socket.cpp
        net/TcpConnection.cpp
        net/Buffer.cpp
        net/TcpServer.cpp
        net/EventLoopThreadPool.cpp
        net/Connector.cpp
        net/TcpClient.cpp
        net/SocketApiWrapper.cpp
        net/SelectPoller.cpp
        net/FullDuplexPipe.cpp
)

#! 给本地相对路径添加前缀，变为绝对路径
foreach(LocalFile ${LocalSrc})
    list(PREPEND FullLocalSrc "${CMAKE_CURRENT_SOURCE_DIR}/${LocalFile}")
endforeach()

#! 使用PARENT_SCOPE关键字是的子目录可以更改父目录的变量：这里是添加子目录源文件到父目录的变量中
set(COMMON_SOURCES ${COMMON_SOURCES} ${FullLocalSrc} PARENT_SCOPE)