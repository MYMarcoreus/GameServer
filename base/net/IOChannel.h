#ifndef LINUXGAMESERVER_CHANNEL_H
#define LINUXGAMESERVER_CHANNEL_H

#include "PollerEvent.h"
#include "socket_definations.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>


namespace yy::net {

class EventLoop;

class IOChannel {
public:
    using F_EventCallback = std::function<void()>;

    enum class State: uint8_t {
        eNew = 0,
        eAdded = 1,
        eDeleted = 2
    };

    ///@brief 传入channel的所有者以及channel对应的文件描述符（套接字）
    IOChannel(EventLoop *owner_loop, SocketApiWrapper::socket_t fd, const std::string & name);

    ~IOChannel();

    ///@brief 核心函数，根据发生的事件来调用对应的回调函数
    void HandleHappenedEvent();


    /*! GETTER !*/
    SocketApiWrapper::socket_t GetFD() { return m_FD; }
    IOChannel::State             GetState() { return m_State; }
    EventLoop *                GetOwnerLoop() { return m_OwnerLoop; }
    PollerEvent                GetInterestedEvent(){ return m_InterestedEvent; }
    PollerEvent                GetHappenedEvent(){ return m_HappenedEvent; }
    std::string                GetName() const { return m_Name; }


    bool IsEnableReading  () { return m_InterestedEvent.HasEvent(PollerEvent::eReadEvent) ; };
    bool IsEnableWriting  () { return m_InterestedEvent.HasEvent(PollerEvent::eWriteEvent); };
    bool IsNoneEvent      () { return m_InterestedEvent.HasNoneEvent(); };

    /*! SETTER !*/
    //! Set Interested Events：注意这些也需要在Channel所在Loop运行，即RunCallbackInLoop！
    void  EnableReading()  { m_InterestedEvent.AddEvent(PollerEvent::eReadEvent ); UpdateFromPoller(); };
    void DisableReading()  { m_InterestedEvent.DelEvent(PollerEvent::eReadEvent ); UpdateFromPoller(); };
    void  EnableWriting()  { m_InterestedEvent.AddEvent(PollerEvent::eWriteEvent); UpdateFromPoller(); };
    void DisableWriting()  { m_InterestedEvent.DelEvent(PollerEvent::eWriteEvent); UpdateFromPoller(); };
    // Set Callbacks
    void SetReadCallback (F_EventCallback cb) { m_ReadCallback  = std::move(cb); }
    void SetWriteCallback(F_EventCallback cb) { m_WriteCallback = std::move(cb); }
    void SetCloseCallback(F_EventCallback cb) { m_CloseCallback = std::move(cb); }
    void SetErrorCallback(F_EventCallback cb) { m_ErrorCallback = std::move(cb); }

    void SetState(IOChannel::State newState) { m_State = newState; }
    void SetHappendedEvent(PollerEvent event) { m_HappenedEvent = event;  }

    /// @brief TcpConnection会有对应的一个Channel成员，m_tie用于绑定TcpConnection对象，
    ///        监控TcpConnection的生命周期，防止TcpConnection生命周期到时引用到失效对象。
    void Tie(const std::shared_ptr<void> &obj);

    /// @brief 清空感兴趣的事件，通知EventLoop让Poller将channel从底层数据结构删除
    void ResetAndRemoveFromPoller();
private:

    void HandleEventWithTie();

    ///@brief 在Channel的事件改变后，通知EventLoop让Poller更新Poller实际对应的底层数据结构
    void UpdateFromPoller();


private:
    EventLoop *         m_OwnerLoop; // Channel的间接所有者（Channel的直接所有者是Poller）
    IOChannel::State      m_State;     // Channel在底层数据结构的状态（未加入，已加入，被删除）
    /*! 一个Channel只能负责一个文件描述符fd的的IO事件分发（但它并不拥有并管理这个fd的生命周期），它会将该fd上的不同IO事件分发至不同的回调函数 */
    SocketApiWrapper::socket_t   m_FD;        // Channel对应的文件描述符（套接字）
    std::string         m_Name;

    PollerEvent m_InterestedEvent{}; // 要监视的事件
    PollerEvent m_HappenedEvent{};   // Poller::PollWait返回后填入的已发生的事件

    F_EventCallback m_ReadCallback{};
    F_EventCallback m_WriteCallback{};
    F_EventCallback m_CloseCallback{};
    F_EventCallback m_ErrorCallback{};

    bool            m_IsAddedToLoop;

    bool                m_IsTied;
    std::weak_ptr<void> m_Tie;
};

} // yy::net

#endif //LINUXGAMESERVER_CHANNEL_H

