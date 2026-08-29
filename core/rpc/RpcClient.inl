#pragma once

template<typename F>
requires std::invocable<F> // 约束调用F为空参数
class LambdaClosureT final : public google::protobuf::Closure {
public:
    explicit LambdaClosureT(F&& func) : func_(std::move(func)) {}

    void Run() override {
        func_();
        delete this;
    }

private:
    F func_;
};

template<std::invocable<> F> // 约束调用F为空参数
google::protobuf::Closure* NewLambdaClosureT(F&& f) {
    return new LambdaClosureT<F>(std::forward<F>(f));
}





template <IsValidStub ServiceStub>
std::string RpcClient<ServiceStub>::GetServiceName()
{
    return RpcStubConnectionPool<ServiceStub>::GetServiceName();
}

template <IsValidStub ServiceStub>
void RpcClient<ServiceStub>::SetConnectionEstablishedCallback(
    typename StubConnType::F_RpcStubConnectionEstablishedCallback cb)
{
    cb_ = std::move(cb);
}

template <IsValidStub ServiceStub>
void RpcClient<ServiceStub>::Start(const size_t pool_size)
{
    if (pool_ == nullptr) {
        pool_ = std::make_unique<RpcStubConnectionPool<ServiceStub>>(pool_size);
        pool_->SetServiceChangeCallback(
            [this](const std::string & service_base, std::unordered_map<std::string, net::IPAddressPtr> providers) {
                YLOG_INFO("ServiceChange to {}", service_base)
            });
        pool_->Start(std::move(cb_));
    }
}

template <IsValidStub ServiceStub>
auto RpcClient<ServiceStub>::GetServerNames() -> std::unordered_map<std::string, net::IPAddressPtr>
{
    Start();
    return pool_->GetServerNames();
}

template <IsValidStub ServiceStub>
template <IsProtobufMessage Request, IsProtobufMessage Response>
bool RpcClient<ServiceStub>::CallRemoteAsync_Random(const std::shared_ptr<Request>& request, FinishedCallback<Response> cb)
{
    Start();

    auto conn = pool_->Acquire_Random(5s);
    if (conn == nullptr) return false;
    if (request == nullptr) return false;

    auto response = new Response;
    // 设置请求参数
    auto controller = new RpcControllerImpl;
    controller->set_wait_for_ready(true);
    controller->set_timeout(5s);

    // 设置响应回调，并使用unique_ptr接管裸指针（响应消息和RpcController的生命周期在此自动管理）
    auto lambda_closure = rpc::NewLambdaClosureT(
        [this, response, controller, cb = std::move(cb)]() mutable  {
            if (cb) cb(std::unique_ptr<Response>(response), std::unique_ptr<RpcControllerImpl>(controller));
        });

    // 通过特化模板函数DoCall调用客户端的`RpcConnection::CallMethod`来同步发送请求
    DoCall<Request, Response>(conn->Stub(), controller, request.get(), response, lambda_closure);

    return true;
}

template <IsValidStub ServiceStub>
template <IsProtobufMessage Request, IsProtobufMessage Response>
bool RpcClient<ServiceStub>::CallRemoteAsync_Random(const Request& request, FinishedCallback<Response> cb)
{
    Start();

    auto conn = pool_->Acquire_Random(5s);
    if (conn == nullptr) return false;

    auto response = new Response;
    auto controller = new RpcControllerImpl;
    controller->set_wait_for_ready(true);
    controller->set_timeout(5s);

    auto lambda_closure = rpc::NewLambdaClosureT(
        [this, response, controller, cb = std::move(cb)]() mutable {
            if (cb) cb(std::unique_ptr<Response>(response), std::unique_ptr<RpcControllerImpl>(controller));
        });

    DoCall<Request, Response>(conn->Stub(), controller, &request, response, lambda_closure);

    return true;
}

template <IsValidStub ServiceStub>
template <IsProtobufMessage Request, IsProtobufMessage Response>
bool RpcClient<ServiceStub>::CallRemoteAsync_From(std::string server_name, const std::shared_ptr<Request>& request,
    FinishedCallback<Response> cb)
{
    Start();

    auto conn = pool_->Acquire_From(server_name, 5s);
    if (conn == nullptr) return false;
    if (request == nullptr) return false;

    auto response = new Response;
    // 设置请求参数
    auto controller = new RpcControllerImpl;
    controller->set_wait_for_ready(true);
    controller->set_timeout(5s);

    // 设置响应回调，并使用unique_ptr接管裸指针（响应消息和RpcController的生命周期在此自动管理）
    auto lambda_closure = rpc::NewLambdaClosureT(
        [this, response, controller, cb = std::move(cb)]() mutable  {
            if (cb)
                cb(std::unique_ptr<Response>(response), std::unique_ptr<RpcControllerImpl>(controller));
        });

    // 通过特化模板函数DoCall调用客户端的`RpcConnection::CallMethod`来同步发送请求
    DoCall<Request, Response>(conn->Stub(), controller, request.get(), response, lambda_closure);

    return true;
}

template <IsValidStub ServiceStub>
template <IsProtobufMessage Request, IsProtobufMessage Response>
bool RpcClient<ServiceStub>::CallRemoteAsync_From(const std::string& server_name, const Request& request,
    FinishedCallback<Response> cb)
{
    Start();

    auto conn = pool_->Acquire_From(server_name, 5s);
    if (conn == nullptr) return false;

    auto response = new Response;
    auto controller = new RpcControllerImpl;
    controller->set_wait_for_ready(true);
    controller->set_timeout(5s);

    auto lambda_closure = rpc::NewLambdaClosureT(
        [this, response, controller, cb = std::move(cb)]() mutable {
            if (cb) cb(std::unique_ptr<Response>(response), std::unique_ptr<RpcControllerImpl>(controller));
        });

    DoCall<Request, Response>(conn->Stub(), controller, &request, response, lambda_closure);

    return true;
}

template <IsValidStub ServiceStub>
template <IsProtobufMessage Request, IsProtobufMessage Response>
core::actor::Future<RpcResult<Response>> RpcClient<ServiceStub>::CallRemote_Random(const std::shared_ptr<Request>& request)
{
    auto promise = std::make_shared<core::actor::Promise<RpcResult<Response>>>();
    auto future = promise->get_future();

    const bool started = CallRemoteAsync_Random<Request, Response>(request,
        [promise](std::unique_ptr<Response>&& response, std::unique_ptr<RpcControllerImpl>&& controller) mutable {
            RpcResult<Response> result;
            if (!response) {
                result.error_text = "rpc response is null";
            } else if (!controller || controller->Failed()) {
                result.error_text = controller ? controller->ErrorText() : "rpc failed";
            } else {
                result.response = std::move(response);
            }
            promise->set_value(std::move(result));
        });

    if (!started) {
        RpcResult<Response> result;
        result.error_text = "rpc call failed to start";
        promise->set_value(std::move(result));
    }
    return future;
}

template <IsValidStub ServiceStub>
template <IsProtobufMessage Request, IsProtobufMessage Response>
core::actor::Future<RpcResult<Response>> RpcClient<ServiceStub>::CallRemote_Random(const Request& request)
{
    auto promise = std::make_shared<core::actor::Promise<RpcResult<Response>>>();
    auto future = promise->get_future();

    const bool started = CallRemoteAsync_Random<Request, Response>(request,
        [promise](std::unique_ptr<Response>&& response, std::unique_ptr<RpcControllerImpl>&& controller) mutable {
            RpcResult<Response> result;
            if (!response) {
                result.error_text = "rpc response is null";
            } else if (!controller || controller->Failed()) {
                result.error_text = controller ? controller->ErrorText() : "rpc failed";
            } else {
                result.response = std::move(response);
            }
            promise->set_value(std::move(result));
        });

    if (!started) {
        RpcResult<Response> result;
        result.error_text = "rpc call failed to start";
        promise->set_value(std::move(result));
    }
    return future;
}

template <IsValidStub ServiceStub>
template <IsProtobufMessage Request, IsProtobufMessage Response>
core::actor::Future<RpcResult<Response>> RpcClient<ServiceStub>::CallRemote_From(std::string server_name, const std::shared_ptr<Request>& request)
{
    auto promise = std::make_shared<core::actor::Promise<RpcResult<Response>>>();
    auto future = promise->get_future();

    const bool started = CallRemoteAsync_From<Request, Response>(std::move(server_name), request,
        [promise](std::unique_ptr<Response>&& response, std::unique_ptr<RpcControllerImpl>&& controller) mutable {
            RpcResult<Response> result;
            if (!response) {
                result.error_text = "rpc response is null";
            } else if (!controller || controller->Failed()) {
                result.error_text = controller ? controller->ErrorText() : "rpc failed";
            } else {
                result.response = std::move(response);
            }
            promise->set_value(std::move(result));
        });

    if (!started) {
        RpcResult<Response> result;
        result.error_text = "rpc call failed to start";
        promise->set_value(std::move(result));
    }
    return future;
}

template <IsValidStub ServiceStub>
template <IsProtobufMessage Request, IsProtobufMessage Response>
core::actor::Future<RpcResult<Response>> RpcClient<ServiceStub>::CallRemote_From(const std::string& server_name, const Request& request)
{
    auto promise = std::make_shared<core::actor::Promise<RpcResult<Response>>>();
    auto future = promise->get_future();

    const bool started = CallRemoteAsync_From<Request, Response>(server_name, request,
        [promise](std::unique_ptr<Response>&& response, std::unique_ptr<RpcControllerImpl>&& controller) mutable {
            RpcResult<Response> result;
            if (!response) {
                result.error_text = "rpc response is null";
            } else if (!controller || controller->Failed()) {
                result.error_text = controller ? controller->ErrorText() : "rpc failed";
            } else {
                result.response = std::move(response);
            }
            promise->set_value(std::move(result));
        });

    if (!started) {
        RpcResult<Response> result;
        result.error_text = "rpc call failed to start";
        promise->set_value(std::move(result));
    }
    return future;
}
