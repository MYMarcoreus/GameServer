#include "RpcController.h"

namespace yy::core
{

RpcController::RpcController() = default;


void RpcController::set_timeout(const std::chrono::milliseconds timeout) {
    timeout_ = timeout;
}

std::chrono::milliseconds RpcController::get_timeout() const {
    return timeout_;
}

void RpcController::set_wait_for_ready(const bool wait) {
    wait_for_ready_ = wait;
}


bool RpcController::is_wait_for_ready() const {
    return wait_for_ready_;
}

void RpcController::Reset() {
    failed_ = false;
    error_text_.clear();
    timeout_ = std::chrono::milliseconds(0);
    wait_for_ready_ = false;
    cancel_callback_ = nullptr;
}
bool RpcController::Failed() const {
    return failed_;
}

std::string RpcController::ErrorText() const {
    return error_text_;
}

void RpcController::SetFailed(const std::string& reason) {
    failed_ = true;
    error_text_ = reason;
}

void RpcController::StartCancel() {
    if (cancel_callback_) {
        cancel_callback_->Run();
    }
}

bool RpcController::IsCanceled() const {
    return false; // 若支持 cancel，可以额外实现
}

void RpcController::NotifyOnCancel(google::protobuf::Closure* callback) {
    cancel_callback_ = callback;
}

RpcController::~RpcController()
{
}
}
