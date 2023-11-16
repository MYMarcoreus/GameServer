#ifndef LINUXGAMESERVER_NET_DEFINATIONS_H
#define LINUXGAMESERVER_NET_DEFINATIONS_H

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
class Timer;
class Socket;
class EventLoop;
class IPAddress;
class Buffer;
class Timestamp;
class Connector;

using F_TaskCallback = std::function<void()>;
using TimerID = int64_t;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using TimerPtr         = std::shared_ptr<Timer>;
using IPAddressPtr     = std::shared_ptr<IPAddress>;
using ConnectorPtr     = std::shared_ptr<Connector>;


using F_ConnectionEstablishedCallback   = std::function<void(const TcpConnectionPtr &)>;
using F_ConnectionDestroyedCallback     = std::function<void(const TcpConnectionPtr &)>;
using F_ConnectionWriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;
using F_ConnectionShutdownCallback      = std::function<void(const TcpConnectionPtr &)>;
using F_ConnectionCloseCallback         = std::function<void(const TcpConnectionPtr &)>;
using F_ThreadInitCallback              = std::function<void(EventLoop *)>;
using F_CloseShutdownConnectionsCallback = std::function<void()>;

using F_MessageCallback = std::function<void(const TcpConnectionPtr &, Buffer &)>;


}



#endif //LINUXGAMESERVER_NET_DEFINATIONS_H
