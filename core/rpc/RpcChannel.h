#pragma once

#include <atomic>
#include <google/protobuf/service.h>
#include "Timestamp.h"

#include "RpcCodec.h"






namespace yy::core
{
class ZKClient;

// RpcClient使用：TcpConnection的包装
class RpcChannel final : public ::google::protobuf::RpcChannel {
public:
    RpcChannel(/* , ZKClient & zk_client*/);
    RpcChannel(const net::TcpConnectionPtr & _conn /* , ZKClient & zk_client*/);

    ///@brief 调用具体实现的服务时会调用的函数
    /// @param method	    不可释放（由 Protobuf 框架管理）	一般是全局静态对象，无需管理
    /// @param controller	由调用方构造，调用方释放			可以异步使用，但不能释放或修改其所有权
    /// @param request	    由调用方构造，调用方释放			通常在 CallMethod 结束前仍有效，异步使用前请拷贝或序列化
    /// @param response	    由调用方构造，调用方持有			负责填充
    /// @param done	        可为空，调用方构造，调用方释放		只负责在响应结束后 done->Run()，不能 delete
    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
                ::google::protobuf::RpcController* controller,
                const ::google::protobuf::Message* request,
                ::google::protobuf::Message* response,
                ::google::protobuf::Closure* done) override;

    void OnRawMessage(const net::TcpConnectionPtr& conn, net::NetBuffer & buf);

    void SetConnection(const net::TcpConnectionPtr & conn);

private:
    ///@brief 接收服务提供方发送来的响应
    void OnRpcResponse(const net::TcpConnectionPtr& conn, const RpcMessagePtr& msg);

    struct PendingCallContext
    {
        ::google::protobuf::Message* response;
        ::google::protobuf::Closure* done;
        google::protobuf::RpcController* controller;
        net::Timestamp sendTime;
    };

    net::TcpConnectionPtr conn_;
    RpcCodec codec_;
    // ZKClient & zk_client_;

    std::atomic_int id_;

    // 用于记录已调用但尚未完成的调用：用于实现异步RPC调用
    std::mutex pending_call_mutex_;
    std::map<int64_t, PendingCallContext> pending_calls_ ;
};

}
