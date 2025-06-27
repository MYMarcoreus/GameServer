#include "RpcServer.h"
#include "AppXmlConfig.h"
#include "TcpConnection.h"
#include "rpc.pb.h"
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/service.h>
#include <google/protobuf/stubs/callback.h>

#include "log.h"

namespace yy::core
{
using protocol::core::RpcMessage;

RpcServer::RpcServer(yy::net::EventLoop* accpetorLoop, const yy::net::IPAddressPtr& listenAddr) :
    loop_{accpetorLoop},
    server_(accpetorLoop, listenAddr, true,
        config::g_app_config->GetValue().send_bytes_one(),
        config::g_app_config->GetValue().send_bytes_max(),
        config::g_app_config->GetValue().recv_bytes_one(),
        config::g_app_config->GetValue().recv_bytes_max(),
        config::g_app_config->GetValue().app_xor_code()),
    codec_([this](const net::TcpConnectionPtr & conn, const RpcMessagePtr & msg) { this->OnRpcRequest(conn, msg); })
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
}

RpcServer::~RpcServer()
{
    Stop();
    for (auto s: services_) {
        delete s.second;
    }
}

void RpcServer::Start()
{
    //todo 启动时注册zookeeper服务

    server_.Start(2, 500ms);
}

void RpcServer::Stop()
{
    server_.Stop();
}

void RpcServer::RegisterService(google::protobuf::Service * service)
{
    const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
    services_[desc->full_name()] = service;
    YLOG_INFO("RPC Server: Registered service {}", desc->full_name());
}

void RpcServer::OnRpcRequest(const net::TcpConnectionPtr& conn, const RpcMessagePtr & msg)
{
    const std::string & service_name = msg->service();
    const std::string & method_name = msg->method();
    auto id = msg->id();

    auto it = services_.find(service_name);
    if (it == services_.end()) {
        YLOG_ERROR("{}:{} is not exist!", service_name, method_name)
    }

    const auto & service = it->second;
    const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
    const google::protobuf::MethodDescriptor * method = desc->FindMethodByName(method_name);

    //! 读取请求消息
    std::unique_ptr<google::protobuf::Message> request{service->GetRequestPrototype(method).New()}; //! 函数结束后自动析构
    if (!request->ParseFromString(msg->request())) {
        YLOG_ERROR("request parse error")
        return;
    }

    //! 生成响应消息
    std::unique_ptr<google::protobuf::Message> response{service->GetResponsePrototype(method).New()}; //! 函数结束后自动析构

    // 给下面的method方法的调用，绑定一个Closure的回调函数
    google::protobuf::Closure *done = google::protobuf::NewCallback
        <RpcServer, const net::TcpConnectionPtr&, const std::pair<google::protobuf::Message*, int64_t> &>
        (this, &RpcServer::SendRpcResponse, conn, std::pair<google::protobuf::Message*, int64_t>{response.get(), id});

    // 在框架上根据远端rpc请求，调用当前rpc节点上发布的方法
    // new UserService().Login(controller, request, response, done)
    service->CallMethod(method, nullptr, request.get(), response.get(), done);
}

void RpcServer::SendRpcResponse(const net::TcpConnectionPtr& conn, const std::pair<google::protobuf::Message*, int64_t>& response_id)
{
    const auto response = response_id.first;
    const auto id = response_id.second;

    // 序列化成功后，通过网络把rpc方法执行的结果发送会rpc的调用方
    std::string response_str;
    if (response->SerializeToString(&response_str))  {
        RpcMessage message;
        message.set_type(RpcMessage::RESPONSE);
        message.set_id(id);

        *message.mutable_response() = std::move(response_str); // 比message.set_response(response_str);高效
        codec_.SendTCP(conn, message);

        YLOG_TRACE("RPC Server：发送RPC Response {}, {}", id, response_str.c_str());
    }
    else {
        std::cerr << "serialize response_str error!" << std::endl;
    }

    // 模拟http的短链接服务，由RpcServer主动断开连接
    conn->Shutdown();
    YLOG_TRACE("RPC Server: 断开与<{}:{}>的连接", conn->GetPeerAddr()->GetIPStr(), conn->GetPeerAddr()->GetPortStr())
}
}
