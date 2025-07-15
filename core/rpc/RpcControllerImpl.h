#pragma once

#include <google/protobuf/service.h>
#include <string>
#include <chrono>
#include <atomic>

namespace yy::core::rpc
{

class RpcControllerImpl final : public google::protobuf::RpcController {
public:
    RpcControllerImpl() = default;

    // 客户端设置项
    auto set_timeout(std::chrono::milliseconds timeout) -> void;
    auto get_timeout() const -> std::chrono::milliseconds;

    auto set_wait_for_ready(bool wait) -> void;
    auto is_wait_for_ready() const -> bool;

    // 状态控制
    auto Reset() -> void override;
    auto Failed() const -> bool override;
    auto ErrorText() const -> std::string override;
    auto StartCancel() -> void override;
    auto SetFailed(const std::string& reason) -> void override;
    auto IsCanceled() const -> bool override;
    auto NotifyOnCancel(google::protobuf::Closure* callback) -> void override;

    virtual ~RpcControllerImpl() override;

private:
    std::chrono::milliseconds timeout_{std::chrono::milliseconds(0)};
    bool wait_for_ready_ = false;
    std::atomic<bool> failed_{false};
    std::string error_text_{};
    google::protobuf::Closure* cancel_callback_ = nullptr;
};

}
