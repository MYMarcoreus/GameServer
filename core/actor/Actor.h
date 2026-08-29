#pragma once

#include <atomic>
#include <concepts>
#include <memory>
#include <stdexcept>

#include "EventLoop.h"
#include "Future.h"
#include "Timer.h"
#include "log.h"
#include "net_definations.h"

namespace yy::core::actor {

class ActorSystem;

/// @brief 所有 Actor 的公共接口（非模板），供 ActorSystem 统一管理（类型擦除）
class IActor {
public:
    using ActorID = uint64_t;

    virtual ~IActor() = default;

    /// @brief 向自身线程投递任务；若已在 Actor 线程则直接执行
    virtual void Post(net::F_TaskCallback task) = 0;
    /// @brief 强制入队执行（即使已在 Actor 线程）
    virtual void Enqueue(net::F_TaskCallback task) = 0;

    [[nodiscard]] virtual net::EventLoop* GetLoop() const = 0;
    [[nodiscard]] virtual ActorID GetID() const = 0;

    /// @brief 由框架在 Actor 线程内调用，分发到子类的 OnStart
    virtual void StartInThread() = 0;
    /// @brief 由框架在 Actor 线程内调用，分发到子类的 OnStop
    virtual void StopInThread() = 0;

    /// @brief 任务执行抛出异常时的回调（在 Actor 线程内调用）
    virtual void OnException(std::exception_ptr e) = 0;
};

/// @brief Actor 基类（CRTP）：每个 Actor 绑定一个 EventLoop 线程，
///        其私有状态只能在该线程内访问（无锁）。
///
/// 生命周期约定：
///  - 强引用只存在于 ActorSystem 的注册表与“已投递消息”内部；
///  - 跨线程通信一律使用 ActorRef（弱引用）；
///  - 每条消息执行期间由框架持有强引用，保证执行期间 Actor 存活；
///  - Stop 会把最后一个强引用投递到 Actor 线程内释放，保证析构线程亲和。
template <typename Derived>
class Actor : public IActor, public std::enable_shared_from_this<Derived> {
public:
    using ActorID = IActor::ActorID;

    explicit Actor(net::EventLoop* loop) : loop_(loop) {}
    ~Actor() override = default;

    void Post(net::F_TaskCallback task) override {
        const auto self = this->shared_from_this(); //! 消息执行期间持有强引用，异常时不拖垮线程
        loop_->RunCallbackInLoop([self, task = std::move(task)]() mutable {
            try { task(); }
            catch (...) { self->OnException(std::current_exception()); }
        });
    }

    void Enqueue(net::F_TaskCallback task) override {
        const auto self = this->shared_from_this();
        loop_->EnqueueCallbackInLoop([self, task = std::move(task)]() mutable {
            try { task(); }
            catch (...) { self->OnException(std::current_exception()); }
        });
    }

    [[nodiscard]] net::EventLoop* GetLoop() const override { return loop_; }
    [[nodiscard]] ActorID GetID() const override { return id_; }

    [[nodiscard]] bool IsInActorThread() const { return loop_->IsInLoopingThread(); }

    //! 定时器接口（线程安全，跨线程调用会被投递到 Actor 线程执行）
    net::TimerID RunAfter(net::Microseconds delay, net::F_TaskCallback cb) { return loop_->RunAfter(delay, std::move(cb)); }
    net::TimerID RunEvery(net::Microseconds interval, net::F_TaskCallback cb) { return loop_->RunEvery(interval, std::move(cb)); }
    void CancelTimer(net::TimerID timerid) { loop_->CancelTimer(timerid); }

    /// @brief 周期性执行回调，Actor 析构后自动取消定时器（需在 Actor 线程内调用）
    net::TimerID RunEveryAlive(net::Microseconds interval, net::F_TaskCallback cb) {
        auto timer = loop_->CreateTimerEvery(interval);
        const auto timerid = timer->GetID();
        const auto weak = this->weak_from_this();
        timer->SetCallback([weak, timerid, loop = loop_, cb = std::move(cb)]() mutable {
            const auto self = weak.lock();
            if (self) {
                cb();
            } else {
                loop->CancelTimer(timerid); //! Actor 已析构，自取消
            }
        });
        loop_->AddTimer(timer);
        return timerid;
    }

    /// @brief 请求停止自身：注销注册表并在 Actor 线程内执行 OnStop（可在任意线程调用，幂等）
    void StopSelf() {
        if (stop_self_requested_.exchange(true)) return; //! 只触发一次
        if (stop_self_cb_) stop_self_cb_();
    }

    void StartInThread() final {
        try { OnStart(); }
        catch (...) { LogException("OnStart", std::current_exception()); }
    }

    void StopInThread() final {
        try { OnStop(); }
        catch (...) { LogException("OnStop", std::current_exception()); }
    }

    void OnException(std::exception_ptr e) override { OnExceptionHandler(e); }

protected:
    /// @brief 在 Actor 线程内调用一次，用于初始化（启动定时器等）
    virtual void OnStart() {}
    /// @brief 在 Actor 线程内调用一次，用于清理（取消定时器等）
    virtual void OnStop() {}

    /// @brief 任务异常处理：默认记录日志并注销自身（OnStop 在当前任务结束后执行），子类可覆写
    virtual void OnExceptionHandler(const std::exception_ptr& e) {
        LogException("task", e);
        StopSelf();
    }

private:
    friend class ActorSystem;
    void SetID(ActorID id) { id_ = id; }
    void SetStopSelfCallback(std::function<void()> cb) { stop_self_cb_ = std::move(cb); }

    static void LogException(const char* where, const std::exception_ptr& e) {
        try {
            if (e) std::rethrow_exception(e);
        } catch (const std::exception& ex) {
            YLOG_ERROR("Actor {} 异常：{}", where, ex.what());
        } catch (...) {
            YLOG_ERROR("Actor {} 未知异常", where);
        }
    }

    net::EventLoop* loop_;
    ActorID id_ = 0;
    std::function<void()> stop_self_cb_;
    std::atomic<bool> stop_self_requested_{false};
};

/// @brief Actor 的跨线程安全句柄：只持有弱引用，保证 Actor 最后在其自身线程内析构
template <typename ActorT>
class ActorRef {
public:
    using ActorID = typename ActorT::ActorID;

    ActorRef() = default;
    explicit ActorRef(std::shared_ptr<ActorT> actor) : weak_(std::move(actor)) {
        if (const auto self = weak_.lock()) id_ = self->GetID();
    }

    /// @brief 投递无参任务；Actor 已销毁则静默丢弃
    template <typename Fn>
    void Post(Fn&& fn) const {
        if (auto self = weak_.lock()) self->Post(std::forward<Fn>(fn));
    }

    /// @brief 投递带 Actor 引用的任务（在 Actor 线程内执行）
    template <typename Fn> requires std::invocable<Fn, ActorT&>
    void Send(Fn&& fn) const {
        if (auto self = weak_.lock()) {
            self->Post([self, fn = std::forward<Fn>(fn)]() mutable { fn(*self); });
        }
    }

    /// @brief 异步调用，返回链式 Future（Actor 已销毁时 future 携带异常）
    template <typename Result, typename Fn>
    Future<Result> Ask(Fn&& fn) const {
        auto promise = std::make_shared<Promise<Result>>();
        Future<Result> future = promise->get_future();
        const auto self = weak_.lock();
        if (!self) {
            promise->set_exception(std::make_exception_ptr(std::runtime_error("actor is dead")));
            return future;
        }
        self->Post([self, fn = std::forward<Fn>(fn), promise]() mutable {
            try { promise->set_value(fn()); }
            catch (...) { promise->set_exception(std::current_exception()); }
        });
        return future;
    }

    /// @brief 异步调用（回调带 Actor 引用），返回链式 Future；Actor 已销毁时 future 携带异常
    template <typename Result, typename Fn> requires std::invocable<Fn, ActorT&>
    Future<Result> AskWith(Fn&& fn) const {
        auto promise = std::make_shared<Promise<Result>>();
        Future<Result> future = promise->get_future();
        const auto self = weak_.lock();
        if (!self) {
            promise->set_exception(std::make_exception_ptr(std::runtime_error("actor is dead")));
            return future;
        }
        self->Post([self, fn = std::forward<Fn>(fn), promise]() mutable {
            try { promise->set_value(fn(*self)); }
            catch (...) { promise->set_exception(std::current_exception()); }
        });
        return future;
    }

    [[nodiscard]] bool IsAlive() const { return !weak_.expired(); }
    [[nodiscard]] ActorID GetID() const { return id_; }

    /// @brief 便于 if (ref) 判断句柄是否仍指向存活 Actor
    explicit operator bool() const noexcept { return IsAlive(); }

private:
    std::weak_ptr<ActorT> weak_;
    ActorID id_ = 0;
};

} // namespace yy::core::actor
