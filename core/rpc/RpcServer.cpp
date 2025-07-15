#include "RpcServer.h"
#include "AppXmlConfig.h"
#include "TcpConnection.h"
#include "rpc.pb.h"
#include "ZkServiceManager.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/stubs/callback.h>

#include "log.h"

namespace yy::core::rpc
{
using protocol::core::RpcMessage;

RpcServer::RpcServer(yy::net::EventLoop* accpetorLoop, const yy::net::IPAddressPtr& listenAddr, const std::string & service_root) :
    service_root_(service_root),
    loop_{accpetorLoop},
    server_(accpetorLoop, listenAddr, true,
        config::g_app_config->GetValue().send_bytes_one(),
        config::g_app_config->GetValue().send_bytes_max(),
        config::g_app_config->GetValue().recv_bytes_one(),
        config::g_app_config->GetValue().recv_bytes_max(),
        config::g_app_config->GetValue().app_xor_code()),
    codec_([this](const net::TcpConnectionPtr & conn, const RpcMessagePtr & msg) { this->OnRpcRequest(conn, msg); }),
    listenAddr_(listenAddr)
{
    server_.SetMessageCallback(
    [this](const net::TcpConnectionPtr& conn, net::NetBuffer& buf) {
        codec_.OnTcpData(conn, buf);
    });

    server_.SetConnectionEstablishedCallback([this](const net::TcpConnectionPtr& conn) {
        YLOG_INFO("RPC Server: Connection established with {}:{}", conn->GetPeerAddr()->GetIPStr(), conn->GetPeerAddr()->GetPort());
        if (not conn->IsConnected()) {
            conn->Shutdown();
        }
    });

    server_.SetConnectionDestroyedCallback([this](const net::TcpConnectionPtr& conn) {
        YLOG_INFO("RPC Server Connection destroyed with {}:{}", conn->GetPeerAddr()->GetIPStr(), conn->GetPeerAddr()->GetPort());
    });

    zkServiceManager_.Start(service_root_);
}

RpcServer::~RpcServer()
{
    Stop();
}

void RpcServer::Start(int ioThreadNum, net::Milliseconds ioWaitTimeout, const net::F_ThreadInitCallback& cb)
{
    if (services_.empty()) {
        std::cerr << "RPC Server: No services were provided." << std::endl;
        std::terminate();
    }
    const auto & ip = listenAddr_->GetIPStr();
    const auto & port = listenAddr_->GetPortStr();

    //! 启动时注册zookeeper服务
    for (auto & [service_name, service] : services_)
    {
        zkServiceManager_.Register(service_name, ip, port);
    }


    server_.Start(2, 300ms);
}

void RpcServer::Stop()
{
    server_.Stop();
}


void RpcServer::OnRpcRequest(const net::TcpConnectionPtr& conn, const RpcMessagePtr & msg)
{
    const std::string & service_name = msg->service();
    const std::string & method_name = msg->method();
    auto id = msg->id();

    RpcMessage::Status errcode = RpcMessage::NO_ERROR;

    const auto & service_opt = this->GetService(service_name);
    if (not service_opt.has_value()) {
        errcode = RpcMessage::NO_SERVICE;
        YLOG_WARN("RPC Server：收到Rpc的服务请求，但是Server未注册该服务<{}>", service_name)
    }

    auto & service = service_opt.value().get();
    const google::protobuf::ServiceDescriptor* desc = service.GetDescriptor();
    const google::protobuf::MethodDescriptor * method = desc->FindMethodByName(method_name);
    if (method == nullptr) {
        errcode = RpcMessage::NO_METHOD;
        YLOG_WARN("RPC Server：Server已注册服务<{}>，但是查找不到其下的方法<{}>", service_name, method_name)
    }

    //! 读取请求消息
    const std::unique_ptr<google::protobuf::Message> request{service.GetRequestPrototype(method).New()}; //! 函数结束后自动析构
    if (msg->has_request()) {
        if (request->ParseFromString(msg->request()) == false) {
            errcode = RpcMessage::INVALID_REQUEST;
            YLOG_WARN("RPC Server：request parse error")
        }
    } else {
        errcode = RpcMessage::INVALID_REQUEST;
    }

    switch (errcode) {
        case protocol::core::RpcMessage_Status_NO_ERROR: {
            //! 生成响应消息
            auto response = service.GetResponsePrototype(method).New(); //! 异步接收响应是，不负责生命周期，由下面的回调函数析构response

            // 给下面的method方法的调用，绑定一个Closure的回调函数
            google::protobuf::Closure* done = google::protobuf::NewCallback
                <RpcServer, net::TcpConnectionPtr, std::pair<google::protobuf::Message*, int64_t>> //!FIXED_BUG：这是异步回调函数,TcpConnectionPtr需要增加一个引用计数，
                (this, &RpcServer::SendRpcResponse, conn, {response, id});

            // 在框架上根据远端rpc请求，调用当前rpc节点上发布的方法
            service.CallMethod(method, nullptr, request.get(), response, done);
            break;
        }
        default: {
            RpcMessage message;
            message.set_type(RpcMessage::RESPONSE);
            message.set_id(id);
            message.set_error(errcode);
            codec_.SendTCP(conn, message);
            break;
        }
    }


}

// done->Run()中会调用该函数，之后会将自身析构掉
// ReSharper disable CppPassValueParameterByConstReference
void RpcServer::SendRpcResponse(net::TcpConnectionPtr conn /*必须使用值传递*/, std::pair<google::protobuf::Message*, int64_t> pair_response_id)
{
    const std::unique_ptr<google::protobuf::Message> response{pair_response_id.first};
    const auto id = pair_response_id.second;

    if (response) {
        // 序列化成功后，通过网络把rpc方法执行的结果发送会rpc的调用方
        std::string response_str;
        if (response->SerializeToString(&response_str))  {
            RpcMessage message;
            message.set_type(RpcMessage::RESPONSE);
            message.set_id(id);
            message.set_error(RpcMessage::NO_ERROR);

            *message.mutable_response() = std::move(response_str); // 比message.set_response(response_str);高效
            codec_.SendTCP(conn, message);

            YLOG_TRACE("RPC Server：发送RPC Response {}, {}", id, response_str.c_str());
        }
        else {
            std::cerr << "serialize response_str error!" << std::endl;
        }
    }

    // 模拟http的短链接服务，由RpcServer主动断开连接
    // conn->Shutdown();
    // YLOG_TRACE("RPC Server: 断开与<{}:{}>的连接", conn->GetPeerAddr()->GetIPStr(), conn->GetPeerAddr()->GetPortStr())
}
// ReSharper restore CppPassValueParameterByConstReference

}
