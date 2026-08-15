#include "RpcControllerImpl.h"

namespace yy::core::rpc
{

void RpcControllerImpl::set_timeout(const std::chrono::milliseconds timeout) {
    timeout_ = timeout;
}

std::chrono::milliseconds RpcControllerImpl::get_timeout() const {
    return timeout_;
}

void RpcControllerImpl::set_wait_for_ready(const bool wait) {
    wait_for_ready_ = wait;
}


bool RpcControllerImpl::is_wait_for_ready() const {
    return wait_for_ready_;
}

void RpcControllerImpl::Reset() {
    failed_ = false;
    error_text_.clear();
    timeout_ = std::chrono::milliseconds(0);
    wait_for_ready_ = false;
    cancel_callback_ = nullptr;
}
bool RpcControllerImpl::Failed() const {
    return failed_;
}

std::string RpcControllerImpl::ErrorText() const {
    return error_text_;
}

void RpcControllerImpl::SetFailed(const std::string& reason) {
    failed_ = true;
    error_text_ = reason;
}

void RpcControllerImpl::StartCancel() {
    if (cancel_callback_) {
        cancel_callback_->Run();
    }
}

bool RpcControllerImpl::IsCanceled() const {
    return false; // 若支持 cancel，可以额外实现
}

void RpcControllerImpl::NotifyOnCancel(google::protobuf::Closure* callback) {
    cancel_callback_ = callback;
}

RpcControllerImpl::~RpcControllerImpl()
{
}
}
