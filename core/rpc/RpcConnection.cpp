#include "RpcConnection.h"
#include "log.h"
#include "RemoteXmlConfig.h"
#include "rpc.pb.h"
#include "RpcController.h"
#include "TcpConnection.h"


using yy::protocol::core::RpcMessage;

namespace yy::core
{
RpcConnection::RpcConnection(yy::net::EventLoop * loop, const net::TcpConnectionPtr& _conn):
    loop_(loop),
    conn_(_conn),
    codec_([this](const net::TcpConnectionPtr& conn, const RpcMessagePtr& buf) {
        this->OnRpcResponse(conn, buf);
    }),
    tcp_client_{std::make_unique<yy::net::TcpClient>(loop,
        yy::config::g_remote_config->GetValue().sendBytesOne,
        yy::config::g_remote_config->GetValue().sendBytesMax,
        yy::config::g_remote_config->GetValue().recvBytesOne,
        yy::config::g_remote_config->GetValue().recvBytesMax,
        yy::config::g_remote_config->GetValue().appXorCode)}
{
    tcp_client_->SetMessageCallback(
        [this](const yy::net::TcpConnectionPtr& conn, yy::net::NetBuffer & buf) {
            codec_.OnTcpData(conn, buf);
        });

    tcp_client_->SetConnectionEstablishedCallback(
        [this](const net::TcpConnectionPtr & conn) {
            // 连接意外断开时，之前积压在pending_calls_中的未收到回复的请求应该进行处理：
            //     方式一：使这些请求清空（这里所采用的）
            //     方式一：重新发送这些请求（服务端需要判断是否收到重复的请求）
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
    if (conn_ == nullptr or not conn_->IsConnected()) {
        auto ctrl = dynamic_cast<RpcController*>(controller);
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

    //! 设置请求的服务对应的方法
    RpcMessage message;
    message.set_type(RpcMessage::REQUEST);
    int64_t id = id_.fetch_add(1, std::memory_order::relaxed);
    message.set_id(id);
    message.set_service(method->service()->name());
    message.set_method(method->name());
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
        pending_calls_.emplace(id, PendingCallContext{response, done, controller, net::Timestamp::Now()});
    }

    //! 根据从zookeeper服务器获取到的ip和端口信息，与服务提供方进行通信
    codec_.SendTCP(conn_, message);
}

void RpcConnection::Connect(const net::IPAddressPtr& server_addr)
{
    {
        std::lock_guard lock(pending_call_mutex_);
        pending_calls_.clear();
    }
    tcp_client_->Connect(server_addr);
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
