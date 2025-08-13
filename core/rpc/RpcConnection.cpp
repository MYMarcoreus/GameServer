#include "RpcConnection.h"
#include "log.h"
#include "RemoteXmlConfig.h"
#include "rpc.pb.h"
#include "RpcControllerImpl.h"
#include "TcpConnection.h"
#include "TcpClient.h"
#include "RpcCodec.h"


using yy::protocol::core::RpcMessage;

namespace yy::core::rpc
{
RpcConnection::RpcConnection(net::EventLoop * loop, const net::TcpConnectionPtr& _conn):
    loop_(loop),
    conn_(_conn),
    codec_(std::make_unique<RpcCodec>([this](const net::TcpConnectionPtr& conn, const RpcMessagePtr& buf) {
            this->OnRpcResponse(conn, buf);
        })),
    tcp_client_{std::make_unique<net::TcpClient>(loop,
        config::g_remote_config->GetValue().sendBytesOne,
        config::g_remote_config->GetValue().sendBytesMax,
        config::g_remote_config->GetValue().recvBytesOne,
        config::g_remote_config->GetValue().recvBytesMax,
        config::g_remote_config->GetValue().appXorCode)}
{
    tcp_client_->SetMessageCallback(
        [this](const net::TcpConnectionPtr& conn, net::NetBuffer & buf) {
            codec_->OnTcpData(conn, buf);
        });

    tcp_client_->SetConnectionEstablishedCallback(
        [this](const net::TcpConnectionPtr & conn) {
            // 连接意外断开时，之前积压在pending_calls_中的未收到回复的请求应该进行处理：
            //     方式一：使这些请求清空（这里所采用的）
            //     方式二：（todo）重新发送这些请求（服务端需要判断是否收到重复的请求，客户端需要在收到响应前一直保存请求）
            {
                std::lock_guard lock(pending_call_mutex_);
                pending_calls_.clear();
            }
            conn_ = conn;
            if (connectionEstablishedCallback_)
                connectionEstablishedCallback_(conn);
            is_connected_.release();
        });

    tcp_client_->SetCanAutoRetry(true);
}

void RpcConnection::CallMethod(const google::protobuf::MethodDescriptor* method,
                               google::protobuf::RpcController* controller, const google::protobuf::Message* request,
                               google::protobuf::Message* response, google::protobuf::Closure* done)
{
    //! 处理请求参数
    if (conn_ == nullptr or not conn_->IsConnected()) {
        auto ctrl = dynamic_cast<RpcControllerImpl*>(controller);
        // 未设置连接超时等待参数，则返回
        if (not ctrl->is_wait_for_ready()) {
            ctrl->SetFailed("Connection is not ready");
            if (done) done->Run();
            return;
        }

        // 超时未连接成功
        if (not is_connected_.try_acquire_for(ctrl->get_timeout())) {
            ctrl->SetFailed("Connection not ready within timeout");
            if (done) done->Run();
            return;
        }
    }

    //! 填充RPC请求头：设置请求的服务对应的方法，并设置RPC请求id
    RpcMessage message;
    message.set_type(RpcMessage::REQUEST);
    int64_t id = id_.fetch_add(1, std::memory_order::relaxed);
    message.set_id(id);
    message.set_service(method->service()->name());
    message.set_method(method->name());
    //! 填充请求
    std::string request_str;
    if (request->SerializeToString(&request_str)) {
        message.set_request(request_str);
    } else {
        controller->SetFailed("[RpcConnection] serialize request error!");
        return;
    }

    //! 存储发起的请求对应的响应消息类型和响应回调
    {
        std::lock_guard lock(pending_call_mutex_);
        //todo 支持不同类型的响应处理策略
        //   enum class PendingCallPolicy {
        //       Drop,       // 当前实现
        //       RetryOnce,  // 自动重发一次
        //       RetryUntilTimeout // 一直等到超时
        //   };
        pending_calls_.emplace(id, PendingCallContext{response, done, controller, net::Timestamp::Now()});
    }

    //! 发送RPC请求
    codec_->SendTCP(conn_, message);
}

bool RpcConnection::Connect(const net::IPAddressPtr& server_addr)
{
    {
        std::lock_guard lock(pending_call_mutex_);
        pending_calls_.clear();
    }
    return tcp_client_->ConnectSync(server_addr);
}

void RpcConnection::Disconnect()
{
    conn_ = nullptr;
    tcp_client_->Disconnect();
    {
        std::lock_guard lock(pending_call_mutex_);
        pending_calls_.clear();
    }
}

void RpcConnection::OnRpcResponse(const net::TcpConnectionPtr& conn, const RpcMessagePtr& msg)
{
    assert(conn != nullptr);
    assert(msg != nullptr);

    const RpcMessage & message = *msg;
    if (message.type() != RpcMessage::RESPONSE) {
        YLOG_ERROR("Error RpcMessage in RpcConnection::OnRpcResponse");
    }

    const int64_t id = message.id();
    assert(message.has_response() || message.has_error());

    //! 获取响应对应的回调
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
        //! 在处理完后自动释放response：这里注释掉了代码，将谁创建了response，谁就来释放（并不建议让RpcConnection释放，因为response可能是一个栈对象）
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
