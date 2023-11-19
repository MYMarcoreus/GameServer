#ifndef LINUXGAMESERVER_TCPCLIENT_H
#define LINUXGAMESERVER_TCPCLIENT_H

#include "socket_definations.h"
#include "net_definations.h"

namespace yy::net {

class TcpClient {
public:
    TcpClient(EventLoop * loop, IPAddressPtr serverAddr);
    ~TcpClient();

    void Connect();
    void Disconnect();
    void StopConnecting();

    //! TcpClient对外暴露m_Connection
    TcpConnectionPtr GetConnection() { return m_Connection; }


    void SetCanAutoRetry(bool CanRetry) { m_CanAutoRetry = CanRetry; }
    void SetConnectionEstablishedCallback(const F_ConnectionEstablishedCallback &connectionEstablishedCallback) {
        m_ConnectionEstablishedCallback = connectionEstablishedCallback;
    }
    void SetMessageCallback(const F_MessageCallback &messageCallback) {
        m_MessageCallback = messageCallback;
    }
    void SetConnectionWriteCompleteCallback(const F_ConnectionWriteCompleteCallback &connectionWriteCompleteCallback) {
        m_ConnectionWriteCompleteCallback = connectionWriteCompleteCallback;
    }

    IPAddressPtr GetServerAddr() { return m_ServerAddr; }

private:
    void NewConnection(SocketApiWrapper::socket_t sockfd);

    void RemoveConnection(TcpConnectionPtr conn);

    void InitLog();

    EventLoop *      m_Loop;
    TcpConnectionPtr m_Connection;
    ConnectorPtr     m_Connector;
    std::string      m_Name;
    bool             m_CanAutoRetry;
    bool             m_IsStarted;
    uint64_t         m_NextConnID{0};
    IPAddressPtr     m_ServerAddr;
    std::mutex       m_ConnectionMutex;

    F_ConnectionEstablishedCallback   m_ConnectionEstablishedCallback;
    F_MessageCallback                 m_MessageCallback;
    F_ConnectionWriteCompleteCallback m_ConnectionWriteCompleteCallback;

};


}

#endif //LINUXGAMESERVER_TCPCLIENT_H

