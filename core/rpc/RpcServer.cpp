#include "RpcServer.h"
#include "AppXmlConfig.h"
#include "TcpConnection.h"
#include "rpc.pb.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/service.h>
#include <google/protobuf/stubs/callback.h>
#include "TcpServer.h"
#include "RpcCodec.h"
#include "ZkServiceClient.h"

#include "log.h"

namespace yy::core::rpc
{
using protocol::core::RpcMessage;

RpcServer::RpcServer(net::EventLoop* accpetorLoop, const net::IPAddressPtr& listenAddr, const std::string & service_root) :
    service_root_(service_root),
    loop_{accpetorLoop},
    server_(std::make_unique<net::TcpServer>(accpetorLoop, listenAddr, true,
        config::g_app_config->GetValue().send_bytes_one(),
        config::g_app_config->GetValue().send_bytes_max(),
        config::g_app_config->GetValue().recv_bytes_one(),
        config::g_app_config->GetValue().recv_bytes_max(),
        config::g_app_config->GetValue().app_xor_code())
    ),
    codec_(std::make_unique<RpcCodec>([this](const net::TcpConnectionPtr & conn, const RpcMessagePtr & msg) { this->OnRpcRequest(conn, msg); })),
    zkServiceManager_{std::make_unique<zk::ZkServiceClient>()}
{
    server_->SetMessageCallback(
    [this](const net::TcpConnectionPtr& conn, net::NetBuffer& buf) {
        codec_->OnTcpData(conn, buf);
    });

    server_->SetConnectionEstablishedCallback([this](const net::TcpConnectionPtr& conn) {
        YLOG_INFO("[RPC Server] Connection established with {}:{}", conn->GetPeerAddr()->GetIPStr(), conn->GetPeerAddr()->GetPort());
        if (not conn->IsConnected()) {
            conn->Shutdown();
        }
    });

    server_->SetConnectionDestroyedCallback([this](const net::TcpConnectionPtr& conn) {
        YLOG_INFO("[RPC Server] Connection destroyed with {}:{}", conn->GetPeerAddr()->GetIPStr(), conn->GetPeerAddr()->GetPort());
    });

    zkServiceManager_->Start(service_root_);
}

RpcServer::~RpcServer()
{
    Stop();
}

void RpcServer::Start(const int ioThreadNum, const net::Milliseconds ioWaitTimeout, const net::F_ThreadInitCallback& cb)
{
    if (services_.empty()) {
        std::cerr << "[RPC Server] No services were provided." << std::endl;
        std::terminate();
    }

    server_->Start(ioThreadNum, ioWaitTimeout, cb);
    const auto listenAddr = server_->GetListenAddr();

    const auto & ip = listenAddr->GetIPStr();
    const auto & port = listenAddr->GetPortStr();
    //! 启动时注册zookeeper服务
    for (auto & [service_name, service] : services_)
    {
        zkServiceManager_->Register(service_name, ip, port);
    }
}

void RpcServer::Stop()
{
    server_->Stop();
}


void RpcServer::OnRpcRequest(const net::TcpConnectionPtr& conn, const RpcMessagePtr & req)
{
    const std::string & service_name = req->service();
    const std::string & method_name = req->method();
    auto rsp_id = req->id();

    RpcMessage::Status errcode = RpcMessage::NO_ERROR;

    //! ① 获取服务
    const auto & service_opt = this->GetService(service_name);
    if (not service_opt.has_value()) {
        errcode = RpcMessage::NO_SERVICE;
        RpcMessage message;
        message.set_type(RpcMessage::RESPONSE);
        message.set_id(rsp_id);
        message.set_error(errcode);
        codec_->SendTCP(conn, message);
        YLOG_WARN("RPC Server：收到Rpc的服务请求，但是Server未注册该服务<{}>", service_name)
        return;
    }
    auto & service = service_opt.value().get();
    const google::protobuf::ServiceDescriptor* desc = service.GetDescriptor();

    //! ② 获取方法
    const google::protobuf::MethodDescriptor * method = desc->FindMethodByName(method_name);
    if (method == nullptr) {
        errcode = RpcMessage::NO_METHOD;
        RpcMessage message;
        message.set_type(RpcMessage::RESPONSE);
        message.set_id(rsp_id);
        message.set_error(errcode);
        codec_->SendTCP(conn, message);
        YLOG_WARN("RPC Server：Server已注册服务<{}>，但是查找不到其下的方法<{}>", service_name, method_name)
        return;
    }

    //! ③ 读取请求消息
    const std::unique_ptr<google::protobuf::Message> request{service.GetRequestPrototype(method).New()}; //! 函数结束后自动析构（如果实现的Rpc方法是异步的且需要用到请求消息，则需要拷贝请求消息）
    if (req->has_request()) {
        if (request->ParseFromString(req->request()) == false) {
            errcode = RpcMessage::INVALID_REQUEST;
            YLOG_WARN("RPC Server：request parse error")
        }
    } else {
        errcode = RpcMessage::INVALID_REQUEST;
    }

    switch (errcode) {
        case protocol::core::RpcMessage_Status_NO_ERROR: {
            //! ④ 生成响应消息
            auto response = service.GetResponsePrototype(method).New(); //! 异步接收响应，不负责生命周期。response仅在done->Run() —— 即SendRpcResponse() —— 调用后自动析构

            //! ⑤ 给下面的method方法的调用，绑定一个Closure的回调函数
            google::protobuf::Closure* done = google::protobuf::NewCallback
                <RpcServer, net::TcpConnectionPtr, std::pair<google::protobuf::Message*, int64_t>> //!FIXED_BUG：这是异步回调函数,TcpConnectionPtr需要增加一个引用计数，
                (this, &RpcServer::SendRpcResponse, conn, {response, rsp_id});

            //! ⑥ 在框架上根据远端rpc请求，调用当前rpc节点上发布的方法
            service.CallMethod(method, nullptr, request.get(), response, done);
            break;
        }
        default: {
            RpcMessage message;
            message.set_type(RpcMessage::RESPONSE);
            message.set_id(rsp_id);
            message.set_error(errcode);
            codec_->SendTCP(conn, message);
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
            codec_->SendTCP(conn, message);

            YLOG_TRACE("RPC Server：发送RPC Response {}, {}", id, response_str.c_str());
        }
        else {
            std::cerr << "serialize response_str error!" << std::endl;
        }
    }
}
// ReSharper restore CppPassValueParameterByConstReference

}
