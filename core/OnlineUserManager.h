#ifndef ____ONLINEUSERMANAGER_H
#define ____ONLINEUSERMANAGER_H

#include "UserBaseData.h"
#include "ObjectPool.h"

namespace yy::core {


class OnlineUserManager
{
public:
    OnlineUserManager() = default;

    explicit OnlineUserManager(size_t max_connection);

    void Init(size_t max_connection)
    {
        m_numConnection = 0;
        m_maxConnection = max_connection;
        m_online_users.reserve(max_connection);
        m_fd2index.reserve(max_connection + 10);

        // 建立对象池
        for(int i = 0 ; i < max_connection ; ++i) {
            m_online_users.push_back(std::make_shared<UserBaseData>());
            m_fd2index.push_back(SIZE_MAX);
        }
    }

    /// @brief
    void addUser(util::Socket sock);

    /// @brief 线程安全地回收用户数据对象：将要回收的对象移和最后一个有效用户交换，然后size--
    void delUser(util::Socket sock_del);

    UserBaseData::ptr findUserByIndex(size_t useridx);

    UserBaseData::ptr findUserBySockfd(int sockfd);

    size_t size() const { return m_numConnection; }
private:
    size_t m_numConnection{};
    size_t m_maxConnection{};
    std::vector<UserBaseData::ptr> m_online_users; // 用户连接数据（本身就是一个对象池）
    std::vector<size_t>            m_fd2index;     // 保存套接字文件描述符到索引的映射
    mutable std::mutex             m_mutex;
};


}


#endif //____ONLINEUSERMANAGER_H
