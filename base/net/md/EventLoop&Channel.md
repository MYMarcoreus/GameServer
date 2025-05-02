```mermaid
classDiagram
    direction TB
    
    class TcpServer {
        - map~string, **TcpConnection**::ptr~ m_ConnectionMap
        - **EventLoop** * m_AcceptorLoop
        - unique_ptr~**Acceptor**~ m_Acceptor
        - unique_ptr~**EventLoopThreadPool**~ m_IOThreadPool
        
        ......
    }
    
    class TcpConnection {
        - **EventLoop** *                  m_ioLoop;  
        - unique_ptr~*Socket*~             m_Socket;
        - unique_ptr~*Channel*~            m_Channel;   
        ......
    }
    

    
	class EventLoop {
	    using ChanneList = std::vector~**Channel** *~
        using F_PendingCallback = std::function~void（）~
        using PendingCallbackList = std::vector~F_PendingCallback~
        using F_CloseSocketsCallback = std::function~void（）~
        
        -----
	
        std::thread::id                m_ThreadID;
        bool                           m_IsLooping;
        bool                           m_IsQuit;
        unique_ptr~**Poller**~         m_Poller;         
        unique_ptr~**TimerManager**~   m_TimerManager;
        unique_ptr~**WakeupManager**~  m_WakeupManager;   
        **ChanneList**                 m_ActiveChannels;
        Milliseconds                   m_DefaultPollwaitTimeout;
        PendingCallbackList            m_PenddingFunctors;      
        std::mutex                     m_PenddingFunctorsMutex; 
        std::atomic_bool               m_IsCallingPenddingFunctors;

        F_CloseSocketsCallback m_CloseSocketsCallback;
	}
	
	class Channel {
        **EventLoop** *              m_OwnerLoop; 
        Channel::State               m_State;     
        SocketApiWrapper::socket_t   m_FD;       
        std::string         m_Name;

        --------

        PollerEvent m_InterestedEvent; 
        PollerEvent m_HappenedEvent;  

        --------

        F_EventCallback m_ReadCallback;
        F_EventCallback m_WriteCallback;
        F_EventCallback m_CloseCallback;
        F_EventCallback m_ErrorCallback;

        --------

        bool                m_IsAddedToLoop;
        bool                m_IsTied;
        std::weak_ptr<void> m_Tie;
	}
	

	
    class Acceptor {
        **EventLoop** *           m_AcceptorLoop;
        *Socket*                  m_AcceptSocket;
        *Channel*                 m_AcceptChannel;
    }
    
    class Socket {
	
	}
	
	class Poller{
		<<Interface>>
		
        + EventLoop *                 m_OwnerLoop;
        + std::map~int, Channel *~    m_ChannelMap; //  get_fd->Channel*
	}
	
	class EpollPoller {
		<<Linux>>
		
        int                             m_EpollFD;
        std::vector<struct epoll_event> m_EpollEventList; 
        bool                            m_useET;
        static const int                kEpollMaxSize = 16;
	}
	
    class SelectPoller {
		<<Windows>>
        
        - fd_set select_readfds_;
        - fd_set select_writefds_;
        - fd_set select_expectfds_;
        
        --------

        - fd_set happended_writefds_;
        - fd_set happended_readfds_;
        - fd_set happended_expectfds_;

        std::set<SocketApiWrapper::socket_t, std::greater<SocketApiWrapper::socket_t> > fdSet_;
	}
	
    class WakeupManager{
        + Notify() void
        - OnNotify() void

        - WakeupFD wakeupEventFD_
        - std::unique_ptr<Channel> wakeupChannel_
	}
	
	class WakeUpFD_windows {
		<<Windows>>
		使用UDP连接
	}
	
    class WakeUpFD_linux {
		<<Linux>>
		使用Linux专有的eventfd
	}
	
	class TimerManager{
	
	}
	
	class PriorityQueueTimerManager {
		<<Windows>>
        在PollWait前判断是否有
        超时的任务，若有则处理。
	}
	
    class RBTreeTimerManager {
		<<Linux>>
		使用linux自带的定时器timerfd_create，
		超时时间到时系统自动设置PollWait事件
	}
	

    Acceptor o-- EventLoop
    Acceptor *-- Channel
    Acceptor *-- Socket	
    TcpConnection *-- Socket
    TcpConnection o-- EventLoop
    TcpConnection *-- Channel
    EventLoop "1" o-- "n" Channel
    EventLoop "1" *-- "1" Poller
    Poller <|-- EpollPoller
    Poller <|-- SelectPoller
    EventLoop "1" *-- "1" WakeupManager
    WakeupManager <|-- WakeUpFD_windows
    WakeupManager <|-- WakeUpFD_linux
    EventLoop "1" *-- "1" TimerManager
    TimerManager <|-- PriorityQueueTimerManager
    TimerManager <|-- RBTreeTimerManager
    TcpConnection <-- TcpServer: gets ioLoop from m_IOThreadPool
    TcpServer "1" *-- "n" TcpConnection
    TcpServer "1" *-- "1" EventLoopThreadPool
    

    EventLoopThreadPool "1" *-- "n" EventLoopThread : manages 
    EventLoopThread "1" *-- "1" EventLoop: creates(ioLoop)(in stack)
    
    note for TcpConnection "①如果来自应用层的数据较少，可以选择直接通过套接字发送数据而不注册Channel的写事件。
    ②如果来自应用层要发送的数据过多，则不能直接发送，而是需要先将数据写入写缓冲区中，
    通过Channel来注册Poller其对应套接字的写事件，
    当套接字可写时（其实一直都可写）时调用TcpConnection设置的回调函数来发送数据，
    并在数据发送完毕后取消监听写事件（与被动的读事件不同，如果不取消监听写事件，便会一直触发写事件）"
    
    note for Channel "Channel实际上是套接字或文件描述符与监听者Poller的通道（中介），
    只要需要设置读或写事件的回调（主要），就会需要Channel"
    
    note for Poller "一个套接字实际上对应一个Channel，而Poller也保存了这种关系。
    TcpConnection管理Channel的生命周期，
    而Poller管理Channel对应的套接字在IO复用底层的数据结构。
    然而Poller仅能被EventLoop所有并修改（严格限制所有权，由EventLoop提供Poller对外的接口），
    因此Channel则通过它所属的EventLoop来修改Poller所管理的底层数据结构"
```







#### 主线程（即Acceptor线程）初始化（`GameManager::Init()`）：EventLoop的构造较复杂，不在此列出

```mermaid
sequenceDiagram
    autonumber
    

	actor main
	participant GameManager
    participant ConfigManager
	participant GameServer
    participant ProtobufDispatcher
    participant ProtobufCodec
	participant TcpServer
	participant EventLoopThreadPool

    participant Acceptor
    participant Socket
    participant EventLoop
    participant Poller
    participant Channel
    participant TcpConnection
    
    main ->> +GameManager: GameManager::Init()
    rect rgb(242, 242, 255) 
        GameManager ->> +ConfigManager: LoadXmlConfigs()
        ConfigManager -->> -GameManager: 
        GameManager ->> +ProtobufDispatcher: ProtobufDispatcher(UnkonwnCommand)
        ProtobufDispatcher -->> -GameManager: 

        GameManager ->> EventLoop: acceptorLoop = new EventLoop()
        EventLoop -->> GameManager: ......
        rect rgb(242, 242, 255) 
            note over GameManager, GameServer: Init GameServer

            GameManager ->> +GameServer: m_server = new GameServer<br>(m_accpetorLoop, listenAddr);
            rect rgb(242, 242, 255) 
                note over GameServer, TcpServer: Constructor of GameServer 
                GameServer ->> +TcpServer: TcpServer<br>(acceptorLoop, listenAddr)
                rect rgb(242, 242, 255) 
                    note over TcpServer, Acceptor: Constructor of TcpServer 
                        TcpServer ->> +EventLoopThreadPool: new EventLoopThreadPool(acceptorLoop)
                        EventLoopThreadPool -->> -TcpServer: ......
                        TcpServer ->> +Acceptor: new Acceptor(acceptorLoop)
                        rect rgb(242, 242, 255) 
                            note over Acceptor, Socket: Constructor of Acceptor 
                            Acceptor ->>+ Socket: SetOpt_ReuseAddr(true)
                            Socket -->> Acceptor: 
                            Acceptor ->> Socket: SetOpt_ReusePort(reusePort)
                            Socket -->> Acceptor: 
                            Acceptor ->> Socket: SetOpt_Linger(true)
                            Socket -->> Acceptor: 
                            Acceptor ->> Socket: Bind(listenAddr)
                            Socket -->>- Acceptor: 
                        end
                        Acceptor -->> -TcpServer:  
                end
                GameServer ->> +TcpServer: SetMessageCallback(ProtobufCodec::OnData)
                TcpServer -->> -GameServer: 
                GameServer ->> +TcpServer: SetConnectionEstablishedCallback
                TcpServer -->> -GameServer: 
                GameServer ->> +TcpServer: SetConnectionShutdownCallback(AfterShutdownConnection)
                TcpServer -->> -GameServer: 
                TcpServer -->> -GameServer: 

                GameServer ->> +ProtobufDispatcher: ProtobufDispatcher(OnUnknownMessage)
                ProtobufDispatcher -->> -GameServer: 
                GameServer ->> +ProtobufDispatcher: RegisterMessageCallback(OnHeart)
                ProtobufDispatcher -->> -GameServer:           
                GameServer ->> +ProtobufDispatcher: RegisterMessageCallback(OnSecurity)
                ProtobufDispatcher -->> -GameServer:             

                GameServer ->> +ProtobufCodec: ProtobufCodec(ProtobufDispatcher::OnProtobufMessage)
                ProtobufCodec -->> -GameServer: 
            end

            GameServer -->> -GameManager: 

            GameManager ->> GameServer: SetNotifier_Security<br>(cb=AppNotifier_Secutiry)
            %% GameServer ->> GameServer: m_notifier_security = cb
            GameServer -->> GameManager: 
            GameManager ->> GameServer: SetNotifier_DisConnect<br>(cb=AppNotifier_Disconnect)
            %% GameServer ->> GameServer: m_notifier_disconnect = cb
            GameServer -->> GameManager: 
            GameManager ->> GameServer: SetNotifier_Command<br>(cb=AppNotifier_Command)
            %% GameServer ->> GameServer:  m_notifier_command = cb
            GameServer -->> GameManager: 
        end

        note right of GameManager: "c": 初始化具体业务的对象
    end


    GameManager ->> -main: 
    
    
    %%TcpConnection ->> Channel: m_Channel->SetReadCallback(cb: this->HandleRead)
    %%Channel ->> Channel: m_ReadCallback = cb
    



```



### 主线程（即Acceptor线程）启动（`GameManager::StartListenAndIOLoop()`）：

```mermaid
sequenceDiagram
    autonumber
	
	actor main
	participant GameManager
	participant GameServer
	participant TcpServer
	participant EventLoopThreadPool

    participant Acceptor
    participant EventLoop
    participant Poller
    participant Channel
    participant Socket
    participant TcpConnection
    
    main ->> +GameManager: StartListenAndIOLoop()
        GameManager ->> +GameServer: Start()
            GameServer ->> +TcpServer: Start(nIOthread=<br>appconfig::io_thread_num())
                
                TcpServer ->> +EventLoopThreadPool: Start(nIOthread)
                EventLoopThreadPool -->> -TcpServer:  
                
                TcpServer ->> +Acceptor: SetNewConnectionCallback(HandleNewConnection)
                Acceptor -->> -TcpServer: 
                
                TcpServer ->> +Acceptor: StartListen()
                    Acceptor ->> +Channel: SetReadCallback(HandleAcceptAll)
                    Channel -->> -Acceptor: 
                    Acceptor ->> +Channel: EnableReading()
                    Channel -->> -Acceptor: 
                    Acceptor ->> +Socket: Listen()
                    Socket -->> -Acceptor: 
                    
                Acceptor -->> -TcpServer: 

            TcpServer -->> -GameServer: 
        GameServer -->> -GameManager: 
    GameManager -->> -main: 
```



### 接受用户连接的过程（`Acceptor::HandleAcceptAll`的channel读回调与`TcpServer::HandleNewConnection`回调）

```mermaid
sequenceDiagram
    autonumber

    participant Poller
    participant EventLoop
    participant Channel_acceptor
    participant Acceptor
    participant Socket
    participant GameServer
    participant TcpServer
    participant TcpConnection
    participant EventLoopThreadPool 

    
	EventLoop ->>+ Poller: m_Poller->PollWait
	Poller  -->> - EventLoop : filled m_ActiveChannels
	note over EventLoop, Channel_acceptor: 有连接到来，Acceptor监听的套接字有读事件
	loop for activeChannel in m_ActiveChannels
        EventLoop ->> +Channel_acceptor: activeChannel->HandleHappenedEvent() 
		opt 读事件发生(执行m_ReadCallback回调)
			note over Channel_acceptor, Acceptor: 执行回调→
			Channel_acceptor ->> +Acceptor: HandleAcceptAll()<br>执行回调
                Acceptor ->> Socket: AcceptAll(<br>isNewSockNonBlock=true)
                Socket -->> Acceptor: allConnfd
                loop for conn_fd, conn_addr in allConnfd
                	note over Acceptor, TcpServer: 执行回调→
                    Acceptor ->> +TcpServer: HandleNewConnection(conn_fd, coon_addr)<br>执行回调
                    
                    	TcpServer ->> +EventLoopThreadPool: GetNextLoop()
                    	EventLoopThreadPool -->> -TcpServer: ioLoop
                        
                        TcpServer ->> +TcpConnection: TcpConnection(conn_fd, coon_addr, ioLoop)
                        TcpConnection -->> -TcpServer: conn
                        
                        TcpServer ->> +TcpConnection: SetConnectionEstablishedCallback<br>(GameServer设置的回调函数)
                        TcpConnection -->> -TcpServer: 
                        
                        TcpServer ->> +TcpConnection: SetMessageCallback<br>(GameServer设置的回调函数)
                        TcpConnection -->> -TcpServer: 
                        
                        TcpServer ->> +TcpConnection: SetConnectionWriteCompleteCallback<br>(GameServer设置的回调函数)
                        TcpConnection -->> -TcpServer: 
                        
                        TcpServer ->> +TcpConnection: SetConnectionCloseCallback<br>(GameServer设置的回调函数)
                        TcpConnection -->> -TcpServer: 
                        
                        TcpServer ->> +TcpConnection: SetConnectionShutdownCallback<br>(GameServer设置的回调函数)
                        TcpConnection -->> -TcpServer: 
                        
                        rect rgb(242, 242, 255) 
                        note over GameServer, Channel_conn: 异步函数，在CallPenddingCallbacks()中运行
                            TcpServer -) +TcpConnection: run conn->ConnectionEstablished() in ioLoop<br>异步函数
                                TcpConnection ->> +Channel_conn:  EnableReading()
                                Channel_conn -->> -TcpConnection:  

                                TcpConnection ->> +Channel_conn:  Tie(shared_from_this())
                                Channel_conn -->> -TcpConnection:  

                                note over GameServer, TcpConnection: ←执行回调
                                TcpConnection ->> +TcpServer: m_ConnectionEstablishedCallback <br> 执行回调
                                    TcpServer ->> +GameServer: m_ConnectionEstablishedCallback
                                        GameServer -->> GameServer: OnConnectionEstablished()
                                    GameServer -->> -TcpServer: 
                                TcpServer -->> -TcpConnection: 

                            TcpConnection --) -TcpServer: 
                        end
                    TcpServer -->> -Acceptor: 
                end
			Acceptor -->> -Channel_acceptor:  
		end 
		
		opt 写事件发生(执行m_WriteCallback回调)
			Channel_acceptor -x Acceptor: Acceptor未设置
		end
        
		opt 错误事件发生(执行m_ErrorCallback回调)
			Channel_acceptor -x Acceptor: Acceptor未设置
		end
		
		opt 关闭事件发生(执行m_CloseCallback回调)
			Channel_acceptor -x Acceptor: Acceptor未设置
		end
		
        Channel_acceptor -->> -EventLoop: 
    end
    
```













## 读事件：接受客户端数据的调用流程图

```mermaid
sequenceDiagram
    autonumber

    participant Poller
    participant EventLoop
    participant Channel
    participant TcpConnection
    participant Socket

    participant TcpServer
    participant ProtobufCodec
    participant ProtobufDispatcher_Tcp
    participant GameServer

    participant GameManager
    participant ProtobufDispatcher_User
    participant GamePlayerManager
    participant GameTestManager

    
	EventLoop ->>+ Poller: m_Poller->PollWait
	Poller  -->> - EventLoop : filled m_ActiveChannels
	note over EventLoop, Channel: 有客户端数据到来，TcpConnection对应的的套接字有读事件
	loop for activeChannel in m_ActiveChannels
        EventLoop ->> +Channel: activeChannel->HandleHappenedEvent() 
		opt 读事件发生(执行m_ReadCallback回调)
			Channel ->> TcpConnection: HandleRead()
                alt  HandleRead_LT
                    TcpConnection ->> Socket : Readv()
                    Socket -->> TcpConnection: 
                    
                    note over TcpConnection, TcpServer : 执行回调→
                    TcpConnection ->> +TcpServer: m_MessageCallback <br> 执行回调
                        TcpServer ->> +GameServer: m_MessageCallback
                            GameServer ->> +ProtobufCodec: OnData()
                                ProtobufCodec ->> +ProtobufDispatcher_Tcp: OnProtobufMessage()
                                    ProtobufDispatcher_Tcp ->> +GameServer: OnUnknownMessage()
                                        GameServer ->> +GameManager: AppNotifier_Command()
                                        	note over GameManager, GameTestManager: 异步函数
                                            GameManager -) ProtobufDispatcher_User: m_wordThreads.PushTask<br>(OnProtobufMessage)
                                            	alt GamePlayerManager
                                                    alt LoginRequest
                                                        GameManager ->> GamePlayerManager: OnLogin
                                                        GamePlayerManager -->> GameManager: 
                                                    else OtherPlayerDataRequest
                                                        GameManager ->> GamePlayerManager: OnOtherPlayerDataRequest
                                                        GamePlayerManager -->> GameManager: 
                                                    else SelfMovement
                                                        GameManager ->> GamePlayerManager: OnSelfMovement
                                                        GamePlayerManager -->> GameManager: 
                                                    else SelfJumpAndGravity
                                                        GameManager ->> GamePlayerManager: OnSelfJumpAndGravity
                                                        GamePlayerManager -->> GameManager: 
                                                    else PlayerLeave
                                                        GameManager ->> GamePlayerManager: OnLeave
                                                        GamePlayerManager -->> GameManager: 
                                                    end 
                                                else GameTestManager
                                                	alt TestMsg1
                                                        GameManager ->> GameTestManager: OnTestMsg1
                                                        GameTestManager -->> GameManager: 
                                                	else TestMsg2
                                                        GameManager ->> GameTestManager: OnTestMsg2
                                                        GameTestManager -->> GameManager: 
                                                	end
                                                end
                                            ProtobufDispatcher_User --) GameManager: 
                                        GameManager -->> -GameServer: 
                                    GameServer -->> -ProtobufDispatcher_Tcp: 
                                ProtobufDispatcher_Tcp -->> -ProtobufCodec: 
                            ProtobufCodec -->> -GameServer: 
                        GameServer -->> -TcpServer: 
                    TcpServer -->> -TcpConnection: 
                    
                 else HandleRead_ET
                    TcpConnection ->> Socket : Recv()
                end
			TcpConnection ->> Channel:  
		end 
		
		opt 写事件发生(执行m_WriteCallback回调)
			Channel ->> TcpConnection: HandleWrite()
		end
        
		opt 错误事件发生(执行m_ErrorCallback回调)
			Channel ->> TcpConnection: HandleError()
		end
		
		opt 关闭事件发生(执行m_CloseCallback回调)
			Channel ->> TcpConnection: HandleClose()
		end
		
        Channel -->> -EventLoop: 
    end
    
```

## 写事件：

```mermaid
sequenceDiagram
    autonumber

    participant Poller
    participant EventLoop
    participant Channel
    participant Socket
    participant TcpConnection


    participant TcpServer

    participant GameServer
 
    participant ProtobufCodec
    participant UserConnection
    participant GameManager
    participant GamePlayerManager



    GameManager ->> +GamePlayerManager: OnSelfMovement
        GamePlayerManager ->> +UserConnection: Send()
            UserConnection ->> +ProtobufCodec: Send()
                ProtobufCodec -) +TcpConnection: Send() in ioLoop
                note over ProtobufCodec, TcpConnection: CallPenddingCallbacks的异步函数
                    alt 输出缓冲为空
                        TcpConnection ->> Socket: Send() 直接发送
                        Socket -->> TcpConnection: 
                        TcpConnection --) ProtobufCodec: 
                    else 输出缓冲不为空
                    	TcpConnection --) -ProtobufCodec: 
                    end

            ProtobufCodec -->> -UserConnection: 
        UserConnection -->> -GamePlayerManager: 
    GamePlayerManager -->> -GameManager: 



    EventLoop ->>+ Poller: m_Poller->PollWait
	Poller  -->> - EventLoop : filled m_ActiveChannels
	note over EventLoop, Channel: 有客户端数据到来，TcpConnection对应的的套接字有读事件
	loop for activeChannel in m_ActiveChannels
        EventLoop ->> +Channel: activeChannel->HandleHappenedEvent() 
		opt 读事件发生(执行m_ReadCallback回调)
			Channel -x TcpConnection: HandleRead()
		end 
		
		opt 写事件发生(执行m_WriteCallback回调)
			Channel ->> TcpConnection: HandleWrite()
			TcpConnection -->> Channel: 
		end
        
		opt 错误事件发生(执行m_ErrorCallback回调)
			Channel -x TcpConnection: HandleError()
		end
		
		opt 关闭事件发生(执行m_CloseCallback回调)
			Channel -x TcpConnection: HandleClose()
		end
		
        Channel -->> -EventLoop: 
    end
    
```









## 代办函数的执行逻辑:

```mermaid
sequenceDiagram
    autonumber

    participant EventLoop
    participant Poller
    participant Channel
    participant TcpConnection
    actor AnyBody
    

    
	EventLoop ->>+ Poller: m_Poller->PollWait
	Poller  -->> - EventLoop : filled m_ActiveChannels
	loop for activeChannel in m_ActiveChannels
        EventLoop ->> +Channel: activeChannel->HandleHappenedEvent() 
		opt 读事件发生(执行m_ReadCallback回调)
			Channel -x TcpConnection: HandleRead()
		end 
		
		opt 写事件发生(执行m_WriteCallback回调)
			Channel ->> TcpConnection: HandleWrite()
			TcpConnection -->> Channel: 
		end
        
		opt 错误事件发生(执行m_ErrorCallback回调)
			Channel -x TcpConnection: HandleError()
		end
		
		opt 关闭事件发生(执行m_CloseCallback回调)
			Channel -x TcpConnection: HandleClose()
		end
		
        Channel -->> -EventLoop: 
    end
    
    rect rgb(242, 242, 255) 
        note over EventLoop, AnyBody: EnqueueCallbackInLoop(functor): start
        AnyBody ->> +EventLoop: EnqueueCallbackInLoop(functor)
        critical [push functor]
            EventLoop ->> EventLoop: m_PenddingFunctors.emplace_back(functor)
        end
        EventLoop ->> +WakeupManager: SetNeedWake()
        WakeupManager -->> -EventLoop: 
        EventLoop -->> -AnyBody:  
        note over EventLoop, AnyBody: EnqueueCallbackInLoop(functor): end
    end
    
    rect rgb(242, 242, 255) 
        note over EventLoop, AnyBody: CallPenddingCallbacks(): start
        EventLoop ->> AnyBody: CallPenddingCallbacks()
        critical [fill callingFunctors]
            EventLoop ->> EventLoop: m_PenddingFunctors.swap(callingFunctors);
        end
        
        loop for functor in callingFunctors
                EventLoop ->> AnyBody:  functor()，运行那些不在ioLoop线程执行EnqueueCallbackInLoop()加入的代办函数
                AnyBody -->> EventLoop: 
        end
        AnyBody -->> EventLoop: 
        note over EventLoop, AnyBody: CallPenddingCallbacks(): end
    end


```

