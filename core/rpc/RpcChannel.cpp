#include "RpcChannel.h"
#include "log.h"
#include "rpc.pb.h"
#include "TcpConnection.h"


using yy::protocol::core::RpcMessage;

namespace yy::core
{
RpcChannel::RpcChannel():
    conn_(nullptr),
    codec_([this](const net::TcpConnectionPtr& conn, const RpcMessagePtr& buf) {
        this->OnRpcResponse(conn, buf);
    })
{

}

RpcChannel::RpcChannel(const net::TcpConnectionPtr& _conn):
    conn_(_conn),
    codec_([this](const net::TcpConnectionPtr& conn, const RpcMessagePtr& buf) {
        this->OnRpcResponse(conn, buf);
    })
{
    //
}

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                            google::protobuf::RpcController* controller, const google::protobuf::Message* request,
                            google::protobuf::Message* response, google::protobuf::Closure* done)
{
    //! 设置请求的服务对应的方法
    RpcMessage message;
    message.set_type(RpcMessage::REQUEST);
    int64_t id = id_.fetch_add(1, std::memory_order::relaxed);
    message.set_id(id);
    message.set_service(method->service()->full_name());
    message.set_method(method->name());
    message.set_request(request->SerializeAsString()); // FIXME: error check

    //! 存储发起的请求对应的响应消息类型和响应回调
    {
        std::lock_guard lock(pending_call_mutex_);
        pending_calls_.emplace(id, PendingCallContext{response, done, controller, net::Timestamp::Now()});
    }

    //todo 连接zookeeper服务器，获取服务提供方的ip和端口信息

    //! 根据从zookeeper服务器获取到的ip和端口信息，与服务提供方进行通信
    codec_.SendTCP(conn_, message);
}

void RpcChannel::OnRawMessage(const net::TcpConnectionPtr& conn, net::NetBuffer& buf)
{
    codec_.OnTcpData(conn, buf);
}

void RpcChannel::SetConnection(const net::TcpConnectionPtr& conn)
{
    conn_ = conn;
}

void RpcChannel::OnRpcResponse(const net::TcpConnectionPtr& conn, const RpcMessagePtr& msg)
{
    assert(conn != nullptr);
    assert(msg != nullptr);

    const RpcMessage & message = *msg;
    if (message.type() != RpcMessage::RESPONSE) {
        YLOG_ERROR("Error RpcMessage in RpcChannel::OnRpcResponse");
    }

    const int64_t id = message.id();
    assert(message.has_response() || message.has_error());

    // 获取响应对应的回调
    PendingCallContext call_context = {nullptr, nullptr , nullptr, net::Timestamp{}};
    {
        std::lock_guard lg(pending_call_mutex_);
        auto it = pending_calls_.find(id);
        if (it != pending_calls_.end()) {
            call_context = it->second;
            pending_calls_.erase(it);
        }
    }

    if (call_context.response)
    {
        //! 在处理完后自动释放response：这里注释掉了代码，将谁创建了response，谁就来释放（并不建议让RpcChannel释放，因为response可能是一个栈对象）
        // std::unique_ptr<google::protobuf::Message> d(call_context.response);

        //! 解析消息到response中，protobuf自动将其传入到done回调
        if (message.has_response()) {
            call_context.response->ParseFromString(message.response());
        }

        //! 执行response对应的处理函数
        if (call_context.done) {
            call_context.done->Run();
        }
    }
}
}
