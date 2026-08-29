#pragma once

#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

namespace yy::core::actor {

/// @brief 链式 Future（future/then 模式，不依赖协程）：
///        用 then/onError 串联异步步骤，避免回调嵌套。
///
/// 与 ActorRef::Ask 配合时：
///  - 结果在 Actor 线程内 set_value，续体（then/onError 回调）通常也在该线程内联执行；
///  - 极端情况下若 future 已就绪才注册 then，续体会在注册线程执行。
///
/// 约定：
///  - T 需可拷贝（get() 按值返回）；Future<void> 暂不支持；
///  - 一个 Future 只能被消费一次（get / then / onError 择一）。
template <typename T>
class Future;

namespace detail {

template <typename T>
struct FutureState {
    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;
    std::unique_ptr<T> value;
    std::exception_ptr error;
    std::function<void()> continuation;

    void set_value(T v) {
        std::function<void()> cont;
        {
            std::lock_guard<std::mutex> lk{mtx};
            value = std::make_unique<T>(std::move(v));
            ready = true;
            cont.swap(continuation);
        }
        if (cont) cont();
        cv.notify_all();
    }

    void set_exception(std::exception_ptr e) {
        std::function<void()> cont;
        {
            std::lock_guard<std::mutex> lk{mtx};
            error = std::move(e);
            ready = true;
            cont.swap(continuation);
        }
        if (cont) cont();
        cv.notify_all();
    }
};

template <>
struct FutureState<void> {
    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;
    std::exception_ptr error;
    std::function<void()> continuation;

    void set_value() {
        std::function<void()> cont;
        {
            std::lock_guard<std::mutex> lk{mtx};
            ready = true;
            cont.swap(continuation);
        }
        if (cont) cont();
        cv.notify_all();
    }

    void set_exception(std::exception_ptr e) {
        std::function<void()> cont;
        {
            std::lock_guard<std::mutex> lk{mtx};
            error = std::move(e);
            ready = true;
            cont.swap(continuation);
        }
        if (cont) cont();
        cv.notify_all();
    }
};

} // namespace detail

template <typename T>
class Future {
public:
    using value_type = T;

    Future() = default;
    explicit Future(std::shared_ptr<detail::FutureState<T>> state) : state_(std::move(state)) {}

    /// @brief 阻塞等待并取结果；若为错误则重新抛出
    T get() {
        std::unique_lock<std::mutex> lk{state_->mtx};
        state_->cv.wait(lk, [this] { return state_->ready; });
        if (state_->error) std::rethrow_exception(state_->error);
        return T(*state_->value);
    }

    /// @brief 是否已就绪（不阻塞）
    bool ready() const {
        std::lock_guard<std::mutex> lk{state_->mtx};
        return state_->ready;
    }

    /// @brief 成功后执行 f（T -> U），错误沿链自动传递
    template <typename F>
    auto then(F f) -> Future<decltype(f(std::declval<T>()))>;

    /// @brief 成功后执行 f（T -> Future<U>），自动扁平化为 Future<U>
    template <typename F>
    auto thenAsync(F f) -> decltype(f(std::declval<T>()));

    /// @brief 失败时执行 fallback（exception_ptr -> T）提供回退值
    template <typename F>
    Future<T> onError(F fallback);

private:
    template <typename U> friend class Future;

    std::shared_ptr<detail::FutureState<T>> state_;
};

template <>
class Future<void> {
public:
    using value_type = void;

    Future() = default;
    explicit Future(std::shared_ptr<detail::FutureState<void>> state) : state_(std::move(state)) {}

    void get() {
        std::unique_lock<std::mutex> lk{state_->mtx};
        state_->cv.wait(lk, [this] { return state_->ready; });
        if (state_->error) std::rethrow_exception(state_->error);
    }

    bool ready() const {
        std::lock_guard<std::mutex> lk{state_->mtx};
        return state_->ready;
    }

    /// @brief 成功后执行 f（() -> U），错误沿链自动传递；f 可返回 void
    template <typename F>
    auto then(F f) -> Future<decltype(f())>;

    /// @brief 成功后执行 f（() -> Future<U>），自动扁平化为 Future<U>
    template <typename F>
    auto thenAsync(F f) -> decltype(f());

    /// @brief 失败时执行 fallback（exception_ptr -> void）
    Future<void> onError(std::function<void(std::exception_ptr)> fallback);

private:
    template <typename U> friend class Future;

    std::shared_ptr<detail::FutureState<void>> state_;
};

template <typename T>
template <typename F>
auto Future<T>::then(F f) -> Future<decltype(f(std::declval<T>()))> {
    using U = decltype(f(std::declval<T>()));
    auto next = std::make_shared<detail::FutureState<U>>();

    std::function<void()> cont = [src = state_, f = std::move(f), next]() mutable {
        std::exception_ptr err;
        std::unique_ptr<T> v;
        {
            std::lock_guard<std::mutex> lk{src->mtx};
            err = src->error;
            if (!err) v = std::make_unique<T>(std::move(*src->value));
        }
        if (err) {
            next->set_exception(err);
            return;
        }
        try {
            if constexpr (std::is_void_v<U>) {
                f(std::move(*v));
                next->set_value();
            } else {
                next->set_value(f(std::move(*v)));
            }
        } catch (...) {
            next->set_exception(std::current_exception());
        }
    };

    bool ready_now = false;
    {
        std::lock_guard<std::mutex> lk{state_->mtx};
        if (state_->ready) ready_now = true;
        else state_->continuation = cont;
    }
    if (ready_now) cont();

    return Future<U>{next};
}

template <typename T>
template <typename F>
auto Future<T>::thenAsync(F f) -> decltype(f(std::declval<T>())) {
    using InnerFuture = decltype(f(std::declval<T>()));
    using U = typename InnerFuture::value_type;
    auto next = std::make_shared<detail::FutureState<U>>();

    std::function<void()> cont = [src = state_, f = std::move(f), next]() mutable {
        std::exception_ptr err;
        std::unique_ptr<T> v;
        {
            std::lock_guard<std::mutex> lk{src->mtx};
            err = src->error;
            if (!err) v = std::make_unique<T>(std::move(*src->value));
        }
        if (err) {
            next->set_exception(err);
            return;
        }

        //! 调用 f 得到内部 Future<U>（f 需返回有效的 Future）
        std::shared_ptr<detail::FutureState<U>> inner_state;
        try {
            InnerFuture inner = f(std::move(*v));
            inner_state = inner.state_; //! Future 各特化互为友元，可访问私有成员
        } catch (...) {
            next->set_exception(std::current_exception());
            return;
        }

        //! 把 inner 的结果接到 next
        std::function<void()> inner_cont = [inner_state, next]() mutable {
            std::exception_ptr e;
            {
                std::lock_guard<std::mutex> lk{inner_state->mtx};
                e = inner_state->error;
            }
            if (e) {
                next->set_exception(e);
                return;
            }
            try {
                if constexpr (std::is_void_v<U>) {
                    next->set_value();
                } else {
                    std::unique_ptr<U> u;
                    {
                        std::lock_guard<std::mutex> lk{inner_state->mtx};
                        u = std::make_unique<U>(std::move(*inner_state->value));
                    }
                    next->set_value(std::move(*u));
                }
            } catch (...) {
                next->set_exception(std::current_exception());
            }
        };

        bool inner_ready = false;
        {
            std::lock_guard<std::mutex> lk{inner_state->mtx};
            if (inner_state->ready) inner_ready = true;
            else inner_state->continuation = inner_cont;
        }
        if (inner_ready) inner_cont();
    };

    bool ready_now = false;
    {
        std::lock_guard<std::mutex> lk{state_->mtx};
        if (state_->ready) ready_now = true;
        else state_->continuation = cont;
    }
    if (ready_now) cont();

    return InnerFuture{next};
}

template <typename T>
template <typename F>
Future<T> Future<T>::onError(F fallback) {
    auto next = std::make_shared<detail::FutureState<T>>();

    std::function<void()> cont = [src = state_, fallback = std::move(fallback), next]() mutable {
        std::exception_ptr err;
        std::unique_ptr<T> v;
        {
            std::lock_guard<std::mutex> lk{src->mtx};
            err = src->error;
            if (!err) v = std::make_unique<T>(std::move(*src->value));
        }
        try {
            if (err) next->set_value(fallback(err)); //! 回退值
            else next->set_value(std::move(*v));
        } catch (...) {
            next->set_exception(std::current_exception());
        }
    };

    bool ready_now = false;
    {
        std::lock_guard<std::mutex> lk{state_->mtx};
        if (state_->ready) ready_now = true;
        else state_->continuation = cont;
    }
    if (ready_now) cont();

    return Future<T>{next};
}

// ---------------- Future<void>::then ----------------
template <typename F>
auto Future<void>::then(F f) -> Future<decltype(f())> {
    using U = decltype(f());
    auto next = std::make_shared<detail::FutureState<U>>();

    std::function<void()> cont = [src = state_, f = std::move(f), next]() mutable {
        if (src->error) {
            next->set_exception(src->error);
            return;
        }
        try {
            if constexpr (std::is_void_v<U>) {
                f();
                next->set_value();
            } else {
                next->set_value(f());
            }
        } catch (...) {
            next->set_exception(std::current_exception());
        }
    };

    bool ready_now = false;
    {
        std::lock_guard<std::mutex> lk{state_->mtx};
        if (state_->ready) ready_now = true;
        else state_->continuation = cont;
    }
    if (ready_now) cont();

    return Future<U>{next};
}

// ---------------- Future<void>::thenAsync ----------------
template <typename F>
auto Future<void>::thenAsync(F f) -> decltype(f()) {
    using InnerFuture = decltype(f());
    using U = typename InnerFuture::value_type;
    auto next = std::make_shared<detail::FutureState<U>>();

    std::function<void()> cont = [src = state_, f = std::move(f), next]() mutable {
        if (src->error) {
            next->set_exception(src->error);
            return;
        }

        std::shared_ptr<detail::FutureState<U>> inner_state;
        try {
            InnerFuture inner = f();
            inner_state = inner.state_;
        } catch (...) {
            next->set_exception(std::current_exception());
            return;
        }

        std::function<void()> inner_cont = [inner_state, next]() mutable {
            std::exception_ptr e;
            {
                std::lock_guard<std::mutex> lk{inner_state->mtx};
                e = inner_state->error;
            }
            if (e) {
                next->set_exception(e);
                return;
            }
            try {
                if constexpr (std::is_void_v<U>) {
                    next->set_value();
                } else {
                    std::unique_ptr<U> u;
                    {
                        std::lock_guard<std::mutex> lk{inner_state->mtx};
                        u = std::make_unique<U>(std::move(*inner_state->value));
                    }
                    next->set_value(std::move(*u));
                }
            } catch (...) {
                next->set_exception(std::current_exception());
            }
        };

        bool inner_ready = false;
        {
            std::lock_guard<std::mutex> lk{inner_state->mtx};
            if (inner_state->ready) inner_ready = true;
            else inner_state->continuation = inner_cont;
        }
        if (inner_ready) inner_cont();
    };

    bool ready_now = false;
    {
        std::lock_guard<std::mutex> lk{state_->mtx};
        if (state_->ready) ready_now = true;
        else state_->continuation = cont;
    }
    if (ready_now) cont();

    return InnerFuture{next};
}

// ---------------- Future<void>::onError ----------------
inline Future<void> Future<void>::onError(std::function<void(std::exception_ptr)> fallback) {
    auto next = std::make_shared<detail::FutureState<void>>();

    std::function<void()> cont = [src = state_, fallback = std::move(fallback), next]() mutable {
        try {
            if (src->error) fallback(src->error);
            next->set_value();
        } catch (...) {
            next->set_exception(std::current_exception());
        }
    };

    bool ready_now = false;
    {
        std::lock_guard<std::mutex> lk{state_->mtx};
        if (state_->ready) ready_now = true;
        else state_->continuation = cont;
    }
    if (ready_now) cont();

    return Future<void>{next};
}

/// @brief 生产端：与 Future 配对使用
template <typename T>
class Promise {
public:
    Promise() : state_(std::make_shared<detail::FutureState<T>>()) {}

    Future<T> get_future() { return Future<T>{state_}; }

    void set_value(T v) { state_->set_value(std::move(v)); }
    void set_exception(std::exception_ptr e) { state_->set_exception(std::move(e)); }

private:
    std::shared_ptr<detail::FutureState<T>> state_;
};

template <>
class Promise<void> {
public:
    Promise() : state_(std::make_shared<detail::FutureState<void>>()) {}

    Future<void> get_future() { return Future<void>{state_}; }

    void set_value() { state_->set_value(); }
    void set_exception(std::exception_ptr e) { state_->set_exception(std::move(e)); }

private:
    std::shared_ptr<detail::FutureState<void>> state_;
};

} // namespace yy::core::actor
