#include "PriorityQueueTimerManager.h"
#include "Timer.h"
#include "EventLoop.h"

namespace yy::net {

bool PriorityQueueTimerManager::TimerComparator::operator()(const Timer *a, const Timer *b) const {
    return !(*a < *b);
}


PriorityQueueTimerManager::PriorityQueueTimerManager(EventLoop * loop)
    : TimerManager(loop)
{}


PriorityQueueTimerManager::~PriorityQueueTimerManager()
{
    for (auto& kv : m_timersref)
    {
        delete(kv.second);
    }
}

TimerID PriorityQueueTimerManager::AddTimer(F_TaskCallback cb, const Timestamp expiredTime, const Microseconds interval)
{
    Timer * timer = new Timer{m_TimerCounter.fetch_add(1, std::memory_order_relaxed), std::move(cb), expiredTime, interval};
    m_loop->RunCallbackInLoop([timer, this]() { AddTimerInLoop(timer); });
    return timer->GetID();
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
    int max_id = m_TimerCounter.load(std::memory_order_relaxed);

    while (!m_timers.empty()) {
        Timer* node = m_timers.top();

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
        delete node;
    }
    return expiredCount;
}

Timestamp PriorityQueueTimerManager::GetEarliestExpiredTimeInLoop() {
    m_loop->AssertInLoopingThread();

    return m_timers.empty() ? Timestamp{} : m_timers.top()->GetExpireTime();
}

void PriorityQueueTimerManager::AddTimerInLoop(Timer * node) {
    m_loop->AssertInLoopingThread();

    m_timersref[node->GetID()] = node;
    m_timers.push(node);
}

void PriorityQueueTimerManager::CancelTimerInLoop(TimerID timer_id) {
    m_loop->AssertInLoopingThread();

    auto iter = m_timersref.find(timer_id);
    if (iter != m_timersref.end()) {
        Timer* node = iter->second;

        node->SetCanceled();
    }
}


}