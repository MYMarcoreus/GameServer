#pragma once
#include <functional>
#include <memory>
#include <chrono>
#include "noncopyable.h"
#include "copyable.h"
// #include "Timestamp.h"


using namespace std::placeholders;
using namespace std::chrono_literals;





namespace yy::net {

using Milliseconds = std::chrono::milliseconds ; // us
using Microseconds = std::chrono::microseconds ; // ms
using Seconds = std::chrono::seconds;            // s

class TcpConnection;
class UdpTransport;
class UdpSession;
class Timer;
class Socket;
class EventLoop;
class IPAddress;
class NetBuffer;
class Timestamp;
class Connector;

using F_TaskCallback = std::function<void()>;
using TimerID = int64_t;

using TcpConnectionPtr  = std::shared_ptr<TcpConnection>;
using UdpSessionPtr     = std::shared_ptr<UdpSession>;
using TimerPtr          = std::shared_ptr<Timer>;
using IPAddressPtr      = std::shared_ptr<IPAddress>;
using ConnectorPtr      = std::shared_ptr<Connector>;


using UdpTransportPtr   = std::shared_ptr<UdpTransport>;

using F_ConnectionEstablishedCallback   = std::function<void(const TcpConnectionPtr &)>;
using F_ConnectionDestroyedCallback     = std::function<void(const TcpConnectionPtr &)>;
using F_ConnectionWriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;
using F_ConnectionShutdownCallback      = std::function<void(const TcpConnectionPtr &)>;
using F_ConnectionCloseCallback         = std::function<void(const TcpConnectionPtr &)>;
using F_ThreadInitCallback               = std::function<void(EventLoop *)>;
using F_CloseShutdownConnectionsCallback = std::function<void()>;

using F_TcpMessageCallback = std::function<void(const TcpConnectionPtr &, NetBuffer &)>;
using F_UdpMessageCallback = std::function<void(const UdpSessionPtr &, NetBuffer &)>;


}

