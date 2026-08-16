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
#include "util_functions.h"

#include <iostream>

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
    //! 前置检查：必须至少注册一个服务
    if (services_.empty()) {
        std::cerr << "[RPC Server] No services were provided." << std::endl;
        std::terminate();
    }

    // 声明
    std::string register_ip;
    std::string port;

    do {
        //! ① 启动监听与 IO 线程
        server_->Start(ioThreadNum, ioWaitTimeout, cb);

        //! ② 计算注册到 ZooKeeper 的地址：
        //! 监听通配地址 0.0.0.0（INADDR_ANY）时不能注册它——远端连接 0.0.0.0 会报 ECONNREFUSED，
        //! 应优先使用配置的 advertiseIp，否则回落到真实可达的本机 IP
        const auto listenAddr = server_->GetListenAddr();
        port = listenAddr->GetPortStr();
        const std::string bind_ip = listenAddr->GetIPStr();
        const auto & advertise = config::g_app_config->GetValue().advertise_ip();
        register_ip = !advertise.empty() ? advertise : ((bind_ip == "0.0.0.0") ? yy::util::GetLocalIP() : bind_ip);
    } while (false);

    //! 统一出口：将所有服务注册到 ZooKeeper 并记录实例信息（供 Stop 时注销）
    for (auto & [service_name, service] : services_)
    {
        zkServiceManager_->Register(service_name, register_ip, port);
        registered_services_.push_back({service_name, register_ip, port});
    }
}

void RpcServer::Stop()
{
    do {
        //! ① 显式注销 ZooKeeper 中的服务实例节点，避免进程退出后节点残留
        if (!registered_services_.empty()) {
            //! 使用 std::cerr 同步输出关键生命周期事件，避免异步日志在进程退出时丢失
            const auto info = std::format("[RPC Server] Unregistering {} service instance(s) from ZooKeeper",
                                          registered_services_.size());
            std::cerr << info << std::endl;
            YLOG_INFO("{}", info);
        }
        for (const auto& registered : registered_services_) {
            zkServiceManager_->Unregister(registered.name, registered.ip, registered.port);
        }
        registered_services_.clear();
    } while (false);

    //! 统一出口：停止服务器
    server_->Stop();
}


void RpcServer::OnRpcRequest(const net::TcpConnectionPtr& conn, const RpcMessagePtr & req)
{
    // 声明
    const std::string & service_name = req->service();
    const std::string & method_name = req->method();
    const auto rsp_id = req->id();

    RpcMessage::Status errcode = RpcMessage::NO_ERROR;
    google::protobuf::Service* service = nullptr;
    const google::protobuf::MethodDescriptor* method = nullptr;
    std::unique_ptr<google::protobuf::Message> request;

    do {
        //! ① 获取服务
        const auto service_opt = this->GetService(service_name);
        if (!service_opt.has_value()) {
            errcode = RpcMessage::NO_SERVICE;
            break;
        }
        service = &service_opt.value().get();

        //! ② 获取方法
        method = service->GetDescriptor()->FindMethodByName(method_name);
        if (method == nullptr) {
            errcode = RpcMessage::NO_METHOD;
            break;
        }

        //! ③ 读取请求消息（函数结束后自动析构；若 Rpc 方法异步且需要请求消息，需自行拷贝）
        request.reset(service->GetRequestPrototype(method).New());
        if (!req->has_request() || !request->ParseFromString(req->request())) {
            errcode = RpcMessage::INVALID_REQUEST;
            break;
        }

        //! ④ 生成响应消息（异步接收响应，生命周期由 done->Run() —— SendRpcResponse() —— 接管）
        google::protobuf::Message* response = service->GetResponsePrototype(method).New();

        //! ⑤ 为方法调用绑定 Closure 回调（TcpConnectionPtr 值传递以增加引用计数，保证异步回调期间连接存活）
        google::protobuf::Closure* done = google::protobuf::NewCallback
            <RpcServer, net::TcpConnectionPtr, std::pair<google::protobuf::Message*, int64_t>>
            (this, &RpcServer::SendRpcResponse, conn, {response, rsp_id});

        //! ⑥ 在框架上根据远端 rpc 请求「同步」调用当前 rpc 节点上发布的方法
        service->CallMethod(method, nullptr, request.get(), response, done);
    } while (false);

    //! 统一出口：按结果记录日志
    switch (errcode) {
        case RpcMessage::NO_ERROR:
            //! 成功路径：响应已由 CallMethod 内部的 done 回调异步发送
            break;
        case RpcMessage::NO_SERVICE:
            YLOG_WARN("RPC Server：收到Rpc的服务请求，但是Server未注册该服务<{}>", service_name);
            break;
        case RpcMessage::NO_METHOD:
            YLOG_WARN("RPC Server：Server已注册服务<{}>，但是查找不到其下的方法<{}>", service_name, method_name);
            break;
        case RpcMessage::INVALID_REQUEST:
            YLOG_WARN("RPC Server：request parse error");
            break;
        default:
            YLOG_WARN("RPC Server：Rpc 请求处理失败，error code: {}", static_cast<int>(errcode));
            break;
    }

    //! 统一出口：出错时发送错误响应
    if (errcode != RpcMessage::NO_ERROR) {
        RpcMessage message;
        message.set_type(RpcMessage::RESPONSE);
        message.set_id(rsp_id);
        message.set_error(errcode);
        codec_->SendTCP(conn, message);
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

            YLOG_TRACE("RPC Server：发送RPC Response {}, {}", id, response_str);
        }
        else {
            std::cerr << "serialize response_str error!" << std::endl;
        }
    }
}
// ReSharper restore CppPassValueParameterByConstReference

}
