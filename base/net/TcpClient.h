#pragma once
#include <mutex>
#include "socket_definations.h"
#include "net_definations.h"
#include "RWLock.h"

namespace yy::net {

class TcpClient {
public:
    TcpClient(EventLoop * loop,
        const int32_t send_bytes_one, const int32_t send_bytes_max,
        const int32_t recv_bytes_one, const int32_t recv_bytes_max,
        const uint8_t xor_code, IPAddressPtr serverAddr = nullptr);


    ~TcpClient();

    void Connect(const IPAddressPtr& server_addr = nullptr);
    void Disconnect();
    void StopConnecting();

    //! TcpClient对外暴露m_Connection
    TcpConnectionPtr GetConnection();


    void SetCanAutoRetry(const bool CanRetry) { m_CanAutoRetry = CanRetry; }
    void SetConnectionEstablishedCallback(const F_ConnectionEstablishedCallback &connectionEstablishedCallback) {
        m_ConnectionEstablishedCallback = connectionEstablishedCallback;
    }
    void SetMessageCallback(const F_TcpMessageCallback &messageCallback) {
        m_MessageCallback = messageCallback;
    }
    void SetConnectionWriteCompleteCallback(const F_ConnectionWriteCompleteCallback &connectionWriteCompleteCallback) {
        m_ConnectionWriteCompleteCallback = connectionWriteCompleteCallback;
    }

    IPAddressPtr GetServerAddr() { return m_ServerAddr; }

private:
    void NewConnection(SocketApiWrapper::socket_t sockfd);

    void RemoveConnection(TcpConnectionPtr conn);

private:
    int32_t m_send_bytes_one;
    int32_t m_send_bytes_max;
    int32_t m_recv_bytes_one;
    int32_t m_recv_bytes_max;
    uint8_t m_xorCode;

    EventLoop *         m_Loop;
    TcpConnectionPtr    m_Connection;
    ConnectorPtr        m_Connector;
    std::string         m_Name;
    bool                m_CanAutoRetry;
    bool                m_IsStarted;
    uint64_t            m_NextConnID;
    IPAddressPtr        m_ServerAddr;
    yy::util::RWMutex   m_ConnectionMutex;

    F_ConnectionEstablishedCallback   m_ConnectionEstablishedCallback;
    F_TcpMessageCallback              m_MessageCallback;
    F_ConnectionWriteCompleteCallback m_ConnectionWriteCompleteCallback;
};


}
