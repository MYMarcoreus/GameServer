#include "PriorityQueueTimerManager.h"
#include "Timer.h"
#include "EventLoop.h"

namespace yy::net {



PriorityQueueTimerManager::PriorityQueueTimerManager(EventLoop * loop)
    : TimerManager(loop)
{}


PriorityQueueTimerManager::~PriorityQueueTimerManager()
{
    //
}

TimerID PriorityQueueTimerManager::AddTimer(F_TaskCallback cb, const Timestamp expiredTime, const Microseconds interval)
{
    TimerPtr timer = std::make_shared<Timer>(m_TimerCounter++, std::move(cb), expiredTime, interval);
    m_loop->RunCallbackInLoop([this, timer](){AddTimerInLoop(timer);});
    return timer->GetID();
}

TimerID PriorityQueueTimerManager::AddTimer(const TimerPtr& timer)
{
    m_loop->RunCallbackInLoop([this, timer](){AddTimerInLoop(timer);});
    return timer->GetID();
}

TimerPtr PriorityQueueTimerManager::CreateTimer(Timestamp expiredTime, Microseconds interval)
{
    return std::make_shared<Timer>(m_TimerCounter++, nullptr, expiredTime, interval);
}

void PriorityQueueTimerManager::CancelTimer(TimerID timer_id)
{
    m_loop->RunCallbackInLoop([timer_id, this]() { CancelTimerInLoop(timer_id); });
}

int PriorityQueueTimerManager::HandleExpiredTimersInLoop()
{
    m_loop->AssertInLoopingThread();

    if (m_timers.empty()) {
        return 0;
    }
    int expiredCount = 0;
    const int max_id = m_TimerCounter.load(std::memory_order_relaxed);
    while (!m_timers.empty()) {
        const auto node = m_timers.top();

        if (net::Timestamp::Now() < node->GetExpireTime())
            break; // 没有到期的timer
        if (node->GetID() > max_id)
            break; // process newly added timer at next tick

        if(!node->IsCanceled()) {
            expiredCount++;

            node->ExecuteCallback();

            if(node->IsRepeated()) {
                node->Restart();
                return expiredCount;
            }
        }

        m_timers.pop();
        m_timersref.erase(node->GetID());
    }
    return expiredCount;
}

Timestamp PriorityQueueTimerManager::GetEarliestExpiredTimeInLoop() {
    m_loop->AssertInLoopingThread();

    return m_timers.empty() ? Timestamp{} : m_timers.top()->GetExpireTime();
}

void PriorityQueueTimerManager::AddTimerInLoop(const TimerPtr& timer) {
    m_loop->AssertInLoopingThread();

    m_timersref[timer->GetID()] = timer;
    m_timers.push(timer);
}

void PriorityQueueTimerManager::CancelTimerInLoop(const TimerID timer_id) {
    m_loop->AssertInLoopingThread();

    auto iter = m_timersref.find(timer_id);
    if (iter != m_timersref.end()) {
        const auto node = iter->second;
        node->SetCanceled();
    }
}

bool PriorityQueueTimerManager::TimerComparator::operator()(const TimerPtr& a, const TimerPtr& b) const
{
    //! 如果有一个指针为空，则按照原生指针的地址来比较
    return (!a or !b) ? a.get() < b.get() : *a < *b;
}

}
