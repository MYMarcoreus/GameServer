#pragma once

#include <atomic>
#include <condition_variable>
#include <google/protobuf/service.h>
#include "core_definations.h"
#include "net_definations.h"
#include "Timestamp.h"

namespace yy::net
{
class TcpClient;
}

namespace yy::core::rpc
{

// RpcClient使用：TcpConnection的包装
class RpcConnection final : public google::protobuf::RpcChannel {
public:
    explicit RpcConnection(net::EventLoop * loop, const net::TcpConnectionPtr & _conn = nullptr);
    ~RpcConnection() override = default;

    ///@brief 调用具体实现的服务时会调用的函数，`该函数不进行参数的生命周期管理`
    /// @param method	    不可释放（由 Protobuf 框架管理）
    /// @param controller	由调用方构造，调用方释放
    /// @param request	    由调用方构造，调用方释放			通常在 CallMethod 结束前仍有效，异步使用前请拷贝或序列化
    /// @param response	    由调用方构造，调用方持有			负责填充
    /// @param done	        可为空，调用方构造，调用方释放		在响应被填充后应该调用done->Run()，Run()函数调用结束后delete掉done对象自身
    void CallMethod(const google::protobuf::MethodDescriptor* method,
                google::protobuf::RpcController* controller,
                const google::protobuf::Message* request,
                google::protobuf::Message* response,
                google::protobuf::Closure* done) override;

    ///@brief 阻塞连接
    bool Connect(const net::IPAddressPtr& server_addr = nullptr);

    void Disconnect();


    void SetConnectionEstablishedCallback(const net::F_ConnectionEstablishedCallback &connectionEstablishedCallback) {
        connectionEstablishedCallback_ = connectionEstablishedCallback;
    }

private:
    ///@brief 接收服务提供方发送来的响应，并调用（请求时设置的）响应回调
    void OnRpcResponse(const net::TcpConnectionPtr& conn, const RpcMessagePtr& msg);

    struct PendingCallContext
    {
        google::protobuf::Message*          response;
        google::protobuf::Closure*          done;
        google::protobuf::RpcController*    controller;
        net::Timestamp                      sendTime;
    };

    net::EventLoop *            loop_;
    net::TcpConnectionPtr       conn_;
    std::unique_ptr<RpcCodec>   codec_;

    std::binary_semaphore is_connected_{0};

    std::atomic_int id_;

    // 用于记录已调用但尚未完成的调用：用于实现异步RPC调用
    std::mutex                              pending_call_mutex_;
    std::map<int64_t, PendingCallContext>   pending_calls_ ;

    net::F_ConnectionEstablishedCallback connectionEstablishedCallback_;
    std::unique_ptr<net::TcpClient> tcp_client_;
};

}
