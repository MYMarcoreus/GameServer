#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Actor.h"
#include "EventLoopThreadPool.h"

namespace yy::core::actor {

/// @brief Actor 调度器：管理工作线程池，负责创建/停止/查找 Actor 并分配线程。
///
/// 线程模型（对外透明）：
///  - Spawn/Stop/StopAll/Get/GetByName/Exists/Size 均可在任意线程调用，内部已加锁；
///  - 强引用只保存在本注册表中，外部一律通过 ActorRef（弱引用）访问；
///  - 每个 Actor 的私有状态仍只在其绑定的 EventLoop 线程内访问（无锁）。
class ActorSystem {
public:
    ActorSystem(net::EventLoop* base_loop, std::size_t thread_num)
        : pool_(std::make_unique<net::EventLoopThreadPool>(base_loop)) {
        pool_->Start(static_cast<int>(thread_num), net::Milliseconds{500});
    }

    ~ActorSystem() { StopAll(); }

    /// @brief 创建 Actor：分配线程 → 注册（持强引用）→ 在其线程内执行 OnStart，返回弱引用句柄。
    ///        可在任意线程调用。
    template <typename ActorT, typename... Args>
    ActorRef<ActorT> Spawn(Args&&... args) {
        auto* loop = pool_->GetNextLoop();
        auto actor = std::make_shared<ActorT>(loop, std::forward<Args>(args)...);
        const IActor::ActorID id = next_id_.fetch_add(1);
        actor->SetID(id);
        actor->SetStopSelfCallback([this, id] { StopSelfFromActor(id); });
        {
            std::lock_guard lock{registry_mutex_};
            registry_[id] = actor;
        }
        ActorRef<ActorT> ref{actor};
        actor->Post([actor] { actor->StartInThread(); });
        return ref;
    }

    /// @brief 按 ActorID 查找：返回类型化弱引用；不存在或类型不匹配则返回空句柄。
    template <typename ActorT>
    ActorRef<ActorT> Get(IActor::ActorID id) {
        std::lock_guard lock{registry_mutex_};
        if (const auto it = registry_.find(id); it != registry_.end()) {
            if (auto actor = std::dynamic_pointer_cast<ActorT>(it->second)) {
                return ActorRef<ActorT>{std::move(actor)};
            }
        }
        return {};
    }

    /// @brief 判断 id 是否仍被注册。
    bool Exists(IActor::ActorID id) {
        std::lock_guard lock{registry_mutex_};
        return registry_.contains(id);
    }

    /// @brief 当前已注册 Actor 数量。
    std::size_t Size() {
        std::lock_guard lock{registry_mutex_};
        return registry_.size();
    }

    /// @brief 创建 Actor 并注册全局名字，供 GetByName 寻址；重名会覆盖旧名字。
    template <typename ActorT, typename... Args>
    ActorRef<ActorT> SpawnNamed(std::string name, Args&&... args) {
        ActorRef<ActorT> ref = Spawn<ActorT>(std::forward<Args>(args)...);
        if (ref) {
            std::lock_guard lock{registry_mutex_};
            names_[std::move(name)] = ref.GetID();
        }
        return ref;
    }

    /// @brief 按名字查找 Actor；不存在或类型不匹配则返回空句柄。
    template <typename ActorT>
    ActorRef<ActorT> GetByName(const std::string& name) {
        IActor::ActorID id = 0;
        {
            std::lock_guard lock{registry_mutex_};
            const auto it = names_.find(name);
            if (it == names_.end()) return {};
            id = it->second;
        }
        return Get<ActorT>(id);
    }

    /// @brief 按 id 停止 Actor：OnStop 在其线程内执行，最后一个强引用在该线程内释放。
    void Stop(IActor::ActorID id) {
        std::shared_ptr<IActor> actor;
        {
            std::lock_guard lock{registry_mutex_};
            if (const auto it = registry_.find(id); it != registry_.end()) {
                actor = it->second;
                registry_.erase(it);
            }
            EraseNamesLocked(id);
        }
        if (actor) {
            actor->Post([actor] { actor->StopInThread(); });
        }
    }

    /// @brief 通过句柄停止 Actor。
    template <typename ActorT>
    void Stop(const ActorRef<ActorT>& ref) {
        Stop(ref.GetID());
    }

    /// @brief 停止所有 Actor，并等待它们的 OnStop 全部执行完毕（优雅停机）。
    ///        需在 EventLoop 线程仍存活时调用（析构时满足）。
    void StopAll() {
        std::vector<std::shared_ptr<IActor>> actors;
        {
            std::lock_guard lock{registry_mutex_};
            actors.reserve(registry_.size());
            for (const auto& [id, actor] : registry_) actors.push_back(actor);
            registry_.clear();
            names_.clear();
        }

        if (actors.empty()) return;

        std::atomic<std::size_t> remaining{actors.size()};
        std::mutex done_mutex;
        std::condition_variable done_cv;

        for (auto& actor : actors) {
            //! 在各自线程内执行 OnStop；全部完成后唤醒等待者
            actor->Post([actor, &remaining, &done_cv] {
                actor->StopInThread();
                if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    done_cv.notify_one();
                }
            });
        }

        std::unique_lock lock{done_mutex};
        done_cv.wait(lock, [&] { return remaining.load(std::memory_order_acquire) == 0; });
    }

private:
    /// @brief 由 Actor::StopSelf 触发：注销并在 Actor 线程内执行 OnStop
    void StopSelfFromActor(IActor::ActorID id) {
        std::shared_ptr<IActor> actor;
        {
            std::lock_guard lock{registry_mutex_};
            const auto it = registry_.find(id);
            if (it == registry_.end()) return; //! 已被停止
            actor = it->second;
            registry_.erase(it);
            EraseNamesLocked(id);
        }
        //! 用 Enqueue（而非 Post）保证 OnStop 在当前任务结束后再执行，避免重入
        actor->Enqueue([actor] { actor->StopInThread(); });
    }

    void EraseNamesLocked(IActor::ActorID id) {
        for (auto it = names_.begin(); it != names_.end();) {
            if (it->second == id) it = names_.erase(it);
            else ++it;
        }
    }

    std::unique_ptr<net::EventLoopThreadPool> pool_;
    std::unordered_map<IActor::ActorID, std::shared_ptr<IActor>> registry_;
    std::unordered_map<std::string, IActor::ActorID> names_;
    std::mutex registry_mutex_; //! 同时保护 registry_ 与 names_
    std::atomic<IActor::ActorID> next_id_{1};
};

} // namespace yy::core::actor
