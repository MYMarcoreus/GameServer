#include "OnlineUserManager.h"

namespace yy::core {

OnlineUserManager::OnlineUserManager(size_t max_connection)
{
    Init(max_connection);
}

void OnlineUserManager::addUser(util::Socket sock)
{
    std::lock_guard lockGuard{m_mutex};

    // //! 容量超上限，则一个一个push
    // if(m_numConnection >= m_maxConnection) {
    //     m_online_users.push_back(std::make_shared<UserBaseData>());
    //     m_maxConnection++;
    // }

    m_fd2index[sock.get_fd()] = m_numConnection;
    m_online_users[m_numConnection]->Init(sock);
    m_numConnection++;
}

void OnlineUserManager::delUser(util::Socket sock_del)
{
    std::lock_guard lockGuard{m_mutex};

    size_t idx_del = m_fd2index[sock_del.get_fd()];
    int fd_del  = m_online_users[idx_del]->sock.get_fd();
    size_t idx_back = m_numConnection - 1;
    int fd_back = m_online_users[idx_back]->sock.get_fd();

    // 重置数据
    m_online_users[idx_del]->Reset();
    m_fd2index[fd_del] = SIZE_MAX;

    // 将要回收的对象移到尾部（和尾部对象交换）
    //FIXME 实际上是智能指针内部指针的交换，若find返回智能指针的引用，会造成问题，详见findUserByIndex()？
    std::swap(m_online_users[idx_del], m_online_users[idx_back]);
    std::swap(m_fd2index[fd_del], m_fd2index[fd_back]);

    m_numConnection--;
}

/*! 不要返回引用，即使取到智能指针后，UserDisconnect后delUser()也能让别的函数检测出来用户已关闭连接
  ! 如果返回引用，delUser()会进行智能指针内部指针的交换，
  ! 在同一轮循环中，若先对用户A进行了操作，之后进行了内部指针的交换，再次使用该智能指针时，会操作用户B，这明显是不对的 */
UserBaseData::ptr OnlineUserManager::findUserByIndex(size_t useridx)
{
    return m_online_users.at(useridx);
}

UserBaseData::ptr OnlineUserManager::findUserBySockfd(int sockfd)
{
    size_t idx = m_fd2index.at(sockfd);
    return idx == SIZE_MAX ? nullptr : m_online_users.at(idx);
}


}