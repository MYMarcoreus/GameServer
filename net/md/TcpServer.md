- contains：类内的普通成员
- creates/manages：类内的指针成员，由管理者new并delete

```mermaid
classDiagram
    direction TB
    
	
    class LogicServerManager {
    	<<Singleton>>
        - **EventLoop** * m_accpetorLoop = new *EventLoop*(500ms)
        - **IServer**   * m_tcpServer = new *GameServer*(m_accpetorLoop, listenAddr)
        
        - **RoomService** * m_player
        - **TestService**   * m_test
        - **ProtobufDispatcher**<**UserConnection**::ptr> m_tcpDispatcher
        - **ThreadPool** m_wordThreads
    }
    
    class RoomService{
    	<<Singleton>>
    	......
    }
    class TestService{
		<<Singleton>>
		......
    }
    
    class ProtobufDispatcher {
        <<template NetworkChannelType>>

        std::map<
        	const google::protobuf::Descriptor *,
        	std::shared_ptr＜ Callback~NetworkChannelType~ ＞
        >;
        
        ProtobufMessageCallback m_UnknownCallback;
    }

    
    class ThreadPool {
        BoundedQueue~Task~            m_Queue
        vector~thread~                m_Threads
        bool                          m_IsRunning
        **EventLoop** *               m_TimerLoop
    }

    
    class IServer {
        <<interface>>
    }
	
	class GameServer {
        - **TcpServer** m_tcpServer
        - **EventLoop** * m_AccpetorLoop
        - std::unordered_map~string , **UserConnection**::ptr~ m_users
        - **ProtobufTcpCodec**               m_tcpCodec
        - **ProtobufDispatcher**<**TcpConnection**::ptr~ m_tcpDispatcher
        
        ......
        
        - F_Notifier        m_NotifierSecurity
        - F_Notifier        m_NotifierDisconnect
        - F_NotifierCommand m_NotifierCommand
        
        ......
	}
	
	class ProtobufTcpCodec {
	    - F_ProtobufMessageDispatchCallback   m_ProtobufMessageDispatchCallback
        - F_ProtobufErrorMessageCallback      m_ProtobufErrorMessageCallback
	}
	    
    class UserConnection {
        - **TcpConnection**::ptr m_tcpChannel
        
        - E_UserBaseState       m_state;
        - uint32_t              m_uid;
        - ProtobufTcpCodec &       m_tcpCodec;
    }

    class TcpServer {
        - map~string, **TcpConnection**::ptr~ m_ConnectionMap
        - **EventLoop** * m_AcceptorLoop
        - unique_ptr~**Acceptor**~ m_Acceptor
        - unique_ptr~**EventLoopThreadPool**~ m_recvEventThreadPool
        
        ......
        
        - F_ConnectionEstablishedCallback      m_ConnectionEstablishedCallback
        - F_ConnectionWriteCompleteCallback    m_ConnectionWriteCompleteCallback
        - F_TcpMessageCallback                    m_UdpMessageCallback
        - F_ConnectionShutdownCallback         m_ConnectionShutdownCallback
        
        ......
        
        + void Stop(): 执行m_accpetorLoop->QuitLoop()退出loop
    }
    
    class Acceptor {
        **EventLoop** *           m_AcceptorLoop;
        *Socket*                  m_AcceptSocket;
        *IOChannel*                 m_AcceptChannel;
        
        ......
        
        - NewConnectionCallback   m_NewConnectionCallback
        
        ......
    }

    class TcpConnection {
        - **EventLoop** *                 m_mainLoop;
        
        ......
        
        - unique_ptr~*Socket*~             m_socket;
        - unique_ptr~*IOChannel*~            m_channel;
        - *Buffer* m_sendBuf;
        - *Buffer* m_recvBuf;
        
        ......
        
        - F_ConnectionEstablishedCallback      m_ConnectionEstablishedCallback  
        - F_TcpMessageCallback                    m_UdpMessageCallback
        - F_ConnectionWriteCompleteCallback    m_ConnectionWriteCompleteCallback
        - F_ConnectionCloseCallback            m_ConnectionCloseCallback
        - F_ConnectionShutdownCallback         m_ConnectionShutdownCallback
        
        ......
    }
    
    class Buffer {
    	......
    }
    
    class EventLoopThreadPool {
        - **EventLoop** *                          m_BaseLoop
        - vector~**EventLoop** *~                  m_ioLoops
        - vector＜unique_ptr~**EventLoopThread**~＞ m_Threads
        - int                                      m_NextLoop
    }
    
	class EventLoopThread {
		- **EventLoop** * m_mainLoop
		- std::thread m_LoopThread
		......
	}
	
	class EventLoop {
		......
	}
	
	subgraph LAYER_GAME
        LogicServerManager "1" *-- "1" EventLoop : creates (acceptor loop)
        LogicServerManager "1" *-- "1" IServer : manages 
        LogicServerManager o-- RoomService: has
        LogicServerManager o-- TestService: has
        LogicServerManager "1" *-- "1" ThreadPool: contains 
        
	end
	
	subgraph LAYER_CORE
        IServer <|-- GameServer
        GameServer o-- EventLoop : uses and quit(acceptor loop)
        GameServer "1" *-- "1" TcpServer : contains 
        
        GameServer "1" *-- "n" UserConnection  : manages
        GameServer "1" *-- "1" ProtobufTcpCodec : manages        
        UserConnection "1" o-- "1" TcpConnection :  每个UserConnection对应一个TcpConnection
        UserConnection  o-- ProtobufTcpCodec : uses by reference
    end
    
    subgraph LAYER_NET
        TcpServer o-- EventLoop : uses(acceptor loop)
        Acceptor o-- EventLoop : uses(acceptor loop)
        TcpServer "1" *-- "1" Acceptor : manages
        TcpServer "1" *-- "1" EventLoopThreadPool : manages
        TcpServer "1" *-- "n" TcpConnection : manages
        TcpConnection <-- EventLoopThreadPool: gets ioLoop from
        TcpConnection o-- EventLoop: uses(ioLoop)
        TcpConnection *-- Buffer

        EventLoopThreadPool o-- EventLoop: uses(acceptor loop)
        EventLoopThreadPool "1" *-- "n" EventLoopThread : manages 
        EventLoopThread "1" *-- "1" EventLoop: creates(ioLoop)(in stack)
    end
    class CallbackT {
    	<<template NetworkChannelType, T>>
    	- ProtobufMessageTCallback m_Callback
    }
    
    class Callback {
    	<<template NetworkChannelType>>
    }

    Callback "1" <|-- "n" CallbackT
    
    ProtobufDispatcher "1" *-- "n" Callback
    
    GameServer "1" *-- "1" ProtobufDispatcher : contains **ProtobufDispatcher**<**TcpConnection**Ptr> 为处理“下层net”无法识别的消息，故为下层的TcpConnection类型
    LogicServerManager "1" *-- "1" ProtobufDispatcher : contains **ProtobufDispatcher**<**UserConnection**Ptr> 为处理“下层core”无法识别的消息，故为下层的UserConnection类型
   
    
  
```



`ProtobufDispatcher`图：

```mermaid
classDiagram
    direction TB

    CallbackT~NetworkChannelType, T~ --|> Callback~NetworkChannelType~
    Callback~NetworkChannelType~ "n" --* "1"  ProtobufDispatcher~NetworkChannelType~
    ProtobufDispatcher_Tcp~NetworkChannelType~ "1" --* "1" GameServer
    
    CallbackT_1~TcpConnection, HeartBody~    --|> Callback_Tcp~TcpConnection~
    CallbackT_2~TcpConnection, SecurityBody~ --|> Callback_Tcp~TcpConnection~
    Callback_Tcp~TcpConnection~ "n" --* "1"  ProtobufDispatcher_Tcp~TcpConnection~
    ProtobufDispatcher_Tcp~TcpConnection~ "1" --* "1" GameServer
    
    CallbackT_3~UserConnection, LoginRequest~    --|> Callback_User~UserConnection~
    CallbackT_4~UserConnection, OtherPlayerDataRequest~ --|> Callback_User~UserConnection~
    CallbackT_5~UserConnection, SelfMovement~    --|> Callback_User~UserConnection~
    CallbackT_6~UserConnection, SelfJumpAndGravity~ --|> Callback_User~UserConnection~
    CallbackT_7~UserConnection, PlayerLeave~ --|> Callback_User~UserConnection~
    Callback_User~UserConnection~ "n" --* "1"  ProtobufDispatcher_User~UserConnection~
    ProtobufDispatcher_User~UserConnection~ "1" --* "1" LogicServerManager
```

