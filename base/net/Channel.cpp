#include "Channel.h"
#include "EventLoop.h"
#include "log.h"

namespace yy::net {

Channel::Channel(EventLoop *owner_loop, SocketApiWrapper::socket_t fd, const std::string &name)
    : m_FD(fd),
    m_OwnerLoop(owner_loop),
    m_State(State::eNew),
    m_IsTied{false},
    m_IsAddedToLoop{false},
    m_Name{name}
{}

Channel::~Channel() {
    //! channel并不管理m_FD的生命周期，m_FD的生命周期由其创建者（TcpConnection内的Socket）负责
    // m_OwnerLoop->RunCallbackInLoop([this]() {
        // if( (int)m_InterestedEvent != 0 )
        //     DisableAllEvent();
        // if(m_IsAddedToLoop)
        //     RemoveFromLoop();
    // });
    //! 需要在channel的定义者处编写：
    //!  m_Channel->DisableAllEvent();
    //!  m_Channel->DisableAllEvent();
}


// 因为Channel不能include"Poller"，所以需要先调用EventLoop的update，再让它调用Poller的update，来修改Poll的底层数据结构
void Channel::UpdateFromLoop() { m_OwnerLoop->UpdateChannel(this); m_IsAddedToLoop = true ; }

void Channel::RemoveFromLoop() { m_OwnerLoop->RemoveChannel(this); m_IsAddedToLoop = false; }

void Channel::Tie(const std::shared_ptr<void> &obj) {
    m_Tie = obj;
    m_IsTied = true;
}

void Channel::HandleHappenedEvent() {
    //! 保证在事件处理过程中，channel的直接所有者（TCPConnection的智能指针）至少有一个引用计数，保证在事件处理过程中，所有者不会被销毁
    if(m_IsTied) {
        if(std::shared_ptr<void> tie = m_Tie.lock()) {
            HandleEventWithTie();
        }
    }
    else
    {
        HandleEventWithTie();
    }
}



void Channel::HandleEventWithTie() {
    if(m_HappenedEvent.IsCloseEvent() && m_CloseCallback)
    {
        YLOG_TRACE("▓▓▓▓▓▓▓▓▓▓▓▓文件描述符<{}>发生关闭事件，进行处理！▓▓▓▓▓▓▓▓▓▓▓▓" , m_FD)
        m_CloseCallback();
    }

    if(m_HappenedEvent.IsErrorEvent() && m_ErrorCallback)
    {
        YLOG_TRACE("▓▓▓▓▓▓▓▓▓▓▓▓文件描述符<{}>发生错误事件，进行处理！▓▓▓▓▓▓▓▓▓▓▓▓", m_FD)
        m_ErrorCallback();
    }

    if(m_HappenedEvent.IsReadEvent() && m_ReadCallback)
    {
        YLOG_TRACE("▓▓▓▓▓▓▓▓▓▓▓▓文件描述符<{}>发生读事件，进行处理！▓▓▓▓▓▓▓▓▓▓▓▓", m_FD)
        m_ReadCallback();
    }

    if(m_HappenedEvent.IsWriteEvent() && m_WriteCallback)
    {
        YLOG_TRACE("▓▓▓▓▓▓▓▓▓▓▓▓文件描述符<{}>发生写事件，进行处理！▓▓▓▓▓▓▓▓▓▓▓▓", m_FD)
        m_WriteCallback();
    }
}





} // yy::net
