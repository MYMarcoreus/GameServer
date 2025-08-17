## 网络框架类图

```mermaid
classDiagram
    direction TB
    
    class TcpServer {
        - map~string, **TcpConnection**::ptr~ m_ConnectionMap
        - **EventLoop** * m_AcceptorLoop
        - unique_ptr~**Acceptor**~ m_Acceptor
        - unique_ptr~**EventLoopThreadPool**~ m_recvEventThreadPool
        
        ......
    }
    
    class TcpConnection {
        - **EventLoop** *                  m_mainLoop;  
        - unique_ptr~*Socket*~             m_socket;
        - unique_ptr~*IOChannel*~            m_channel;   
        ......
    }
    

    
	class EventLoop {
	    using ChanneList = std::vector~**IOChannel** *~
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
	
	class IOChannel {
        **EventLoop** *              m_OwnerLoop; 
        IOChannel::State               m_State;     
        SocketApiWrapper::socket_t   m_FD;       
        std::string         m_connName;

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
        *IOChannel*                 m_AcceptChannel;
    }
    
    class Socket {
	
	}
	
	class Poller{
		<<Interface>>
		
        + EventLoop *                 m_OwnerLoop;
        + std::map~int, IOChannel *~    m_ChannelMap; //  get_fd->IOChannel*
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
        - std::unique_ptr<IOChannel> wakeupChannel_
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
    Acceptor *-- IOChannel
    Acceptor *-- Socket	
    TcpConnection *-- Socket
    TcpConnection o-- EventLoop
    TcpConnection *-- IOChannel
    EventLoop "1" o-- "n" IOChannel
    EventLoop "1" *-- "1" Poller
    Poller <|-- EpollPoller
    Poller <|-- SelectPoller
    EventLoop "1" *-- "1" WakeupManager
    WakeupManager <|-- WakeUpFD_windows
    WakeupManager <|-- WakeUpFD_linux
    EventLoop "1" *-- "1" TimerManager
    TimerManager <|-- PriorityQueueTimerManager
    TimerManager <|-- RBTreeTimerManager
    TcpConnection <-- TcpServer: gets ioLoop from m_recvEventThreadPool
    TcpServer "1" *-- "n" TcpConnection
    TcpServer "1" *-- "1" EventLoopThreadPool
    

    EventLoopThreadPool "1" *-- "n" EventLoopThread : manages 
    EventLoopThread "1" *-- "1" EventLoop: creates(ioLoop)(in stack)
    
    note for TcpConnection "①如果来自应用层的数据较少，可以选择直接通过套接字发送数据而不注册Channel的写事件。
    ②如果来自应用层要发送的数据过多，则不能直接发送，而是需要先将数据写入写缓冲区中，
    通过Channel来注册Poller其对应套接字的写事件，
    当套接字可写时（其实一直都可写）时调用TcpConnection设置的回调函数来发送数据，
    并在数据发送完毕后取消监听写事件（与被动的读事件不同，如果不取消监听写事件，便会一直触发写事件）"
    
    note for IOChannel "Channel实际上是套接字或文件描述符与监听者Poller的通道（中介），
    只要需要设置读或写事件的回调（主要），就会需要Channel"
    
    note for Poller "一个套接字实际上对应一个Channel，而Poller也保存了这种关系。
    TcpConnection管理Channel的生命周期，
    而Poller管理Channel对应的套接字在IO复用底层的数据结构。
    然而Poller仅能被EventLoop所有并修改（严格限制所有权，由EventLoop提供Poller对外的接口），
    因此Channel则通过它所属的EventLoop来修改Poller所管理的底层数据结构"
```

## 接受用户连接的过程（`Acceptor::HandleAcceptAll`的channel读回调与`TcpServer::HandleNewConnection`回调） 

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
                        
                        TcpServer ->> +TcpConnection: SetUdpRecievedCallback<br>(GameServer设置的回调函数)
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
    participant IOChannel
    participant TcpConnection
    participant Buffer

    participant TcpServer
    participant ProtobufTcpCodec
    participant ProtobufDispatcher_Tcp
    participant GameServer

    participant LogicServerManager
    participant ProtobufDispatcher_User
    participant RoomService
    participant TestService

    
	EventLoop ->>+ Poller: m_Poller->PollWait
	Poller  -->> - EventLoop : filled m_ActiveChannels
	note over EventLoop, IOChannel: 有客户端数据到来，TcpConnection对应的的套接字有读事件
	loop for activeChannel in m_ActiveChannels
        EventLoop ->> +IOChannel: activeChannel->HandleHappenedEvent() 
		opt 读事件发生(执行m_ReadCallback回调)
			IOChannel ->> TcpConnection: HandleRead()
                alt  HandleRead_LT
                    TcpConnection ->> Buffer : RecvAllFromSocket()
                    Buffer -->> TcpConnection: 
                    
                    note over TcpConnection, TcpServer : 执行回调→
                    TcpConnection ->> +TcpServer: m_UdpMessageCallback <br> 执行回调
                        TcpServer ->> +GameServer: m_UdpMessageCallback
                            GameServer ->> +ProtobufTcpCodec: OnData()
                                ProtobufTcpCodec ->> +ProtobufDispatcher_Tcp: OnProtobufMessage()
                                    ProtobufDispatcher_Tcp ->> +GameServer: OnUnknownTcpMessage()
                                        GameServer ->> +LogicServerManager: AppNotifier_Command()
                                        	note over LogicServerManager, TestService: 异步函数
                                            LogicServerManager -) ProtobufDispatcher_User: m_wordThreads.PushTask<br>(OnProtobufMessage)
                                            	alt RoomService
                                                    alt LoginRequest
                                                        LogicServerManager ->> RoomService: OnLogin
                                                        RoomService -->> LogicServerManager: 
                                                    else OtherPlayerDataRequest
                                                        LogicServerManager ->> RoomService: OnOtherPlayerDataRequest
                                                        RoomService -->> LogicServerManager: 
                                                    else SelfMovement
                                                        LogicServerManager ->> RoomService: OnSelfMovement
                                                        RoomService -->> LogicServerManager: 
                                                    else SelfJumpAndGravity
                                                        LogicServerManager ->> RoomService: OnSelfJumpAndGravity
                                                        RoomService -->> LogicServerManager: 
                                                    else PlayerLeave
                                                        LogicServerManager ->> RoomService: OnLeave
                                                        RoomService -->> LogicServerManager: 
                                                    end 
                                                else TestService
                                                	alt TestMsg1
                                                        LogicServerManager ->> TestService: OnTestMsg1
                                                        TestService -->> LogicServerManager: 
                                                	else TestMsg2
                                                        LogicServerManager ->> TestService: OnTestMsg2
                                                        TestService -->> LogicServerManager: 
                                                	end
                                                end
                                            ProtobufDispatcher_User --) LogicServerManager: 
                                        LogicServerManager -->> -GameServer: 
                                    GameServer -->> -ProtobufDispatcher_Tcp: 
                                ProtobufDispatcher_Tcp -->> -ProtobufTcpCodec: 
                            ProtobufTcpCodec -->> -GameServer: 
                        GameServer -->> -TcpServer: 
                    TcpServer -->> -TcpConnection: 
                    
                 else HandleRead_ET
                     loop 直到调用返回eAgain错误代码，表示结束ET
                        TcpConnection ->> Buffer : RecvAllFromSocket()
                        Buffer -->> TcpConnection: 
                    end
                end
			TcpConnection -->> IOChannel:  
		end 
		
		opt 写事件发生(执行m_WriteCallback回调)
			IOChannel ->> TcpConnection: HandleWrite()
		end
        
		opt 错误事件发生(执行m_ErrorCallback回调)
			IOChannel ->> TcpConnection: HandleError()
		end
		
		opt 关闭事件发生(执行m_CloseCallback回调)
			IOChannel ->> TcpConnection: HandleClose()
		end
		
        IOChannel -->> -EventLoop: 
    end
    
```

## 写事件：

```mermaid
sequenceDiagram
    autonumber

    participant Poller
    participant EventLoop
    participant IOChannel
    participant Socket
    participant TcpConnection
    participant AnyBody
    participant Buffer
    participant TcpServer

    participant GameServer
 
    participant ProtobufTcpCodec
    participant UserConnection
    participant LogicServerManager
    participant RoomService



    LogicServerManager ->> +RoomService: OnSelfMovement
        RoomService ->> +UserConnection: SendUDP()
            UserConnection ->> +ProtobufTcpCodec: SendUDP()
                ProtobufTcpCodec -) +TcpConnection: SendUDP() in ioLoop
                note over ProtobufTcpCodec, TcpConnection: CallPenddingCallbacks的异步函数
                    alt 输出缓冲为空
                            TcpConnection ->> Socket: SendUDP() 
                            Socket -->> TcpConnection: 
                    	alt  SendUDP()发送了所有数据
                    		TcpConnection ->> AnyBody: 执行上层的写回调（项目中未设置）
                    		AnyBody -->> TcpConnection: 
                        else SendUDP()不能发送所有数据
                            TcpConnection ->>  IOChannel: EnableWriting()，注册写事件，<br>等待写事件发生在EventLoop中执行HandleWrite()
                            IOChannel -->> TcpConnection: 
                        end
                        
                        TcpConnection --) ProtobufTcpCodec: 
                    else 输出缓冲不为空
                    	TcpConnection --) -ProtobufTcpCodec: 
                    end

            ProtobufTcpCodec -->> -UserConnection: 
        UserConnection -->> -RoomService: 
    RoomService -->> -LogicServerManager: 



    EventLoop ->>+ Poller: m_Poller->PollWait
	Poller  -->> - EventLoop : filled m_ActiveChannels
	note over EventLoop, IOChannel: 有客户端数据到来，TcpConnection对应的的套接字有读事件
	loop for activeChannel in m_ActiveChannels
        EventLoop ->> +IOChannel: activeChannel->HandleHappenedEvent() 
		opt 读事件发生(执行m_ReadCallback回调)
			IOChannel -x TcpConnection: HandleRead()
		end 
		
		opt 写事件发生(执行m_WriteCallback回调)
			IOChannel ->> TcpConnection: HandleWrite()
			TcpConnection -->> IOChannel: 
		end
        
		opt 错误事件发生(执行m_ErrorCallback回调)
			IOChannel -x TcpConnection: HandleError()
		end
		
		opt 关闭事件发生(执行m_CloseCallback回调)
			IOChannel -x TcpConnection: HandleClose()
		end
		
        IOChannel -->> -EventLoop: 
    end
    
```

## 代办函数的执行逻辑:

```mermaid
sequenceDiagram
    autonumber

    participant EventLoop
    participant Poller
    participant IOChannel
    participant TcpConnection
    actor AnyBody
    

    
	EventLoop ->>+ Poller: m_Poller->PollWait
	Poller  -->> - EventLoop : filled m_ActiveChannels
	loop for activeChannel in m_ActiveChannels
        EventLoop ->> +IOChannel: activeChannel->HandleHappenedEvent() 
		opt 读事件发生(执行m_ReadCallback回调)
			IOChannel -x TcpConnection: HandleRead()
		end 
		
		opt 写事件发生(执行m_WriteCallback回调)
			IOChannel ->> TcpConnection: HandleWrite()
			TcpConnection -->> IOChannel: 
		end
        
		opt 错误事件发生(执行m_ErrorCallback回调)
			IOChannel -x TcpConnection: HandleError()
		end
		
		opt 关闭事件发生(执行m_CloseCallback回调)
			IOChannel -x TcpConnection: HandleClose()
		end
		
        IOChannel -->> -EventLoop: 
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

## 杂项

### one-loop per-thread模型无需`EPOLLONESHOT`

> - `EPOLLONESHOT`的含义：`EPOLLONESHOT` 是 `epoll` 的一个事件选项，表示**某个文件描述符上的事件只会触发一次**。事件被触发后，`epoll` 会自动将其从监听队列中禁用，**必须手动通过 `epoll_ctl(..., EPOLL_CTL_MOD, ...)` 重新激活**，才能再次监听该 fd 的事件。
>
> - `EPOLLONESHOT`的作用：**对于`one-loop multi-thread`模型，防止多个线程同时处理同一个socket所带来的数据竞争**。

本框架采用的是 **“one-loop per-thread”** 模型，每个TCP连接的套接字只在所属的 `EventLoop`（即一个IO线程）中处理事件，即一个连接只会被一个IO线程处理，处理完之后传递给上层的工作线程 ———— 然后继续处理该连接的IO事件。**天然避免了多个线程同时操作同一个连接的问题**，因此无需设置`EPOLLONESHOT`选项。

