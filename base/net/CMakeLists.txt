cmake_minimum_required(VERSION 3.22)
project(GameServer)
set(CMAKE_CXX_STANDARD 20)

set(LocalSrc
        PriorityQueueTimerManager.cpp
        NewDefaultTimerManager.cpp
        socket_definations.cpp
        UdpTransport.cpp
        UdpServer.cpp
        UdpSession.cpp
        NetBuffer.cpp
        IOCPPoller.cpp
        IPAddress.cpp
        IOChannel.cpp
        EpollPoller.cpp
        EventLoop.cpp
        Poller.cpp
        NewDefaultPoller.cpp
        Timestamp.cpp
        RBTreeTimerManager.cpp
        Timer.cpp
        EventLoopThread.cpp
        Acceptor.cpp
        Socket.cpp
        TcpConnection.cpp
        TcpServer.cpp
        EventLoopThreadPool.cpp
        Connector.cpp
        TcpClient.cpp
        SocketApiWrapper.cpp
        SelectPoller.cpp
        FullDuplexPipe.cpp
        NetBuffer.cpp
)

#! 给本地相对路径添加前缀，变为绝对路径
foreach(LocalFile ${LocalSrc})
    list(APPEND FullLocalSrc "${CMAKE_CURRENT_SOURCE_DIR}/${LocalFile}")
endforeach()

#! 使用PARENT_SCOPE关键字是的子目录可以更改父目录的变量：这里是添加子目录源文件到父目录的变量中
#set(COMMON_SOURCES ${COMMON_SOURCES} ${FullLocalSrc} PARENT_SCOPE)

set(FullLocalSrc ${FullLocalSrc} PARENT_SCOPE)