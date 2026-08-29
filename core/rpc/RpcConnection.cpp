#include "RpcConnection.h"
#include "log.h"
#include "RemoteXmlConfig.h"
#include "rpc.pb.h"
#include "RpcControllerImpl.h"
#include "TcpConnection.h"
#include "TcpClient.h"
#include "RpcCodec.h"
#include "EventLoop.h"


using yy::protocol::core::RpcMessage;

namespace yy::core::rpc
{
RpcConnection::RpcConnection(net::EventLoop * loop, const net::TcpConnectionPtr& _conn):
    loop_(loop),
    conn_(_conn),
    timeout_guard_{std::make_shared<TimeoutGuard>()},
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
    timeout_guard_->owner = this;

    //! 每 500ms 检查一次未完成调用的超时；通过 weak_ptr 守卫避免回调访问已析构对象
    {
        std::weak_ptr<TimeoutGuard> weak_guard = timeout_guard_;
        timeout_timer_id_ = loop_->RunEvery(500ms, [weak_guard] {
            if (auto guard = weak_guard.lock()) {
                std::lock_guard lg{guard->mtx};
                if (guard->owner) {
                    guard->owner->OnTimeoutCheck();
                }
            }
        });
    }

    tcp_client_->SetMessageCallback(
        [this](const net::TcpConnectionPtr& conn, net::NetBuffer & buf) {
            codec_->OnTcpData(conn, buf);
        });

    tcp_client_->SetConnectionEstablishedCallback(
        [this](const net::TcpConnectionPtr & conn) {
            // 连接意外断开/重连时，之前积压在pending_calls_中的未收到回复的请求应以失败结束
            // （调用 done 回调并释放资源），避免调用方永久挂起以及 response/controller/closure 泄漏。
            FailAllPendingCalls("rpc connection lost");
            conn_ = conn;
            if (connectionEstablishedCallback_)
                connectionEstablishedCallback_(conn);
            is_connected_.release();
        });

    tcp_client_->SetCanAutoRetry(true);
}

RpcConnection::~RpcConnection() {
    {
        //! 在锁内将 owner 置空，保证超时定时器回调不会再访问本对象
        std::lock_guard lg{timeout_guard_->mtx};
        timeout_guard_->owner = nullptr;
    }
    if (timeout_timer_id_ != -1) {
        loop_->CancelTimer(timeout_timer_id_);
    }
    //! 销毁时清理所有未完成调用，防止泄漏
    FailAllPendingCalls("rpc connection destroyed");
}

void RpcConnection::FailAllPendingCalls(const std::string& reason) {
    std::vector<PendingCallContext> doomed;
    {
        std::lock_guard lock{pending_call_mutex_};
        doomed.reserve(pending_calls_.size());
        for (auto& [id, ctx] : pending_calls_) {
            doomed.push_back(std::move(ctx));
        }
        pending_calls_.clear();
    }
    for (auto& ctx : doomed) {
        if (ctx.controller) {
            ctx.controller->SetFailed(reason);
        }
        if (ctx.done) {
            ctx.done->Run();
        }
    }
}

void RpcConnection::OnTimeoutCheck() {
    const auto now = net::Timestamp::Now();
    std::vector<PendingCallContext> expired;
    {
        std::lock_guard lock{pending_call_mutex_};
        for (auto it = pending_calls_.begin(); it != pending_calls_.end(); ) {
            PendingCallContext& ctx = it->second;
            if (ctx.timeout > 0ms && (now - ctx.sendTime) >= ctx.timeout) {
                expired.push_back(std::move(it->second));
                it = pending_calls_.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (auto& ctx : expired) {
        if (ctx.controller) {
            ctx.controller->SetFailed("[RpcConnection] rpc call timeout");
        }
        if (ctx.done) {
            ctx.done->Run();
        }
    }
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
        if (done) done->Run(); //! 序列化失败也必须结束调用，否则 response/controller/closure 会永久泄漏
        return;
    }

    //! 存储发起的请求对应的响应消息类型和响应回调
    std::chrono::milliseconds timeout{0};
    if (auto* ctrl = dynamic_cast<RpcControllerImpl*>(controller); ctrl != nullptr) {
        timeout = ctrl->get_timeout();
    }
    {
        std::lock_guard lock(pending_call_mutex_);
        //todo 支持不同类型的响应处理策略
        //   enum class PendingCallPolicy {
        //       Drop,       // 当前实现
        //       RetryOnce,  // 自动重发一次
        //       RetryUntilTimeout // 一直等到超时
        //   };
        pending_calls_.emplace(id, PendingCallContext{response, done, controller, net::Timestamp::Now(), timeout});
    }

    //! 发送RPC请求
    codec_->SendTCP(conn_, message);
}

bool RpcConnection::Connect(const net::IPAddressPtr& server_addr)
{
    //! 重新连接前，将遗留的未完成调用以失败结束，避免挂起与泄漏
    FailAllPendingCalls("rpc connection reconnecting");
    return tcp_client_->ConnectSync(server_addr);
}

void RpcConnection::Disconnect()
{
    conn_ = nullptr;
    tcp_client_->Disconnect();
    //! 断开连接：未完成调用以失败结束
    FailAllPendingCalls("rpc connection disconnected");
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
    PendingCallContext call_context = {nullptr, nullptr , nullptr, net::Timestamp{}, std::chrono::milliseconds{0}};
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
