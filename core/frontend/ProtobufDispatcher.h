#pragma once

#include "core_definations.h"
#include "net_definations.h"
#include "noncopyable.h"

#include <functional>
#include <map>
#include <cassert>
#include <google/protobuf/message.h>

namespace yy::core {


template<typename T>
concept IsNetworkChannelType =
    std::is_same_v<T, net::TcpConnectionPtr> ||
    std::is_same_v<T, net::UdpSessionPtr> ||
    std::is_same_v<T, UserConnectionPtr>
;

// template<IsNetworkChannelType NetworkChannelType>






//! 为了
template<IsNetworkChannelType NetworkChannelType>
class Callback : util::noncopyable {
public:
    virtual ~Callback() = default;

    virtual void OnMessage(const NetworkChannelType &, const MessagePtr &message) const = 0;
};

//! 用于Protobuf
template<IsNetworkChannelType NetworkChannelType, IsProtobufMessage T>
class CallbackT final : public Callback<NetworkChannelType> {
public:
    //! message的子类回调
    using ProtobufMessageTCallback = std::function<void(
            const NetworkChannelType &,
            const std::shared_ptr<T> &message //! 针对特定的protobuf消息类
    )>;

    CallbackT(const ProtobufMessageTCallback &callback) : m_Callback(callback) {}

    void OnMessage(const NetworkChannelType &conn, const MessagePtr &message) const override {
        //! 基类指针转换为子类指针
        std::shared_ptr<T> concrete = std::static_pointer_cast<T>(message);
        assert(concrete != NULL);
        m_Callback(conn, concrete);
    }

private:
    ProtobufMessageTCallback m_Callback;
};



template<IsNetworkChannelType NetworkChannelType>
class ProtobufDispatcher {
public:
    using ProtobufMessageCallback = std::function<void(
            const NetworkChannelType &,
            const MessagePtr &message) //! ConnectionType未知的消息类型，因此用MessagePtr引用向上传递
    >;

    explicit ProtobufDispatcher(ProtobufMessageCallback unknownCb) : m_UnknownCallback(unknownCb) {}

    //! 该函数会作为回调被上层（XXXServer）传递给ProtobufCodec
    void OnProtobufMessage(const NetworkChannelType &conn, const MessagePtr &message) const {
        const auto it = m_CallbacksMap.find(message->GetDescriptor());
        if (it != m_CallbacksMap.end()) {
            //! ConnectionType已知已注册该消息，直接处理之。
            it->second->OnMessage(conn, message); // OnTcpHeart
        } else {
            //! ConnectionType未知的消息，需要向上层传递
            m_UnknownCallback(conn, message);
        }
    }

    //! 注册回调：回调传递子类指针CallbackT<T>，保存的是基类指针Callback，从而使得可以保存各种Message子类的回调
    template<IsProtobufMessage T>
    void RegisterMessageCallback(const typename CallbackT<NetworkChannelType, T>::ProtobufMessageTCallback & callback) {
        //! 子类指针交给map内的父类指针存储
        m_CallbacksMap[T::descriptor()] = std::make_shared< CallbackT<NetworkChannelType, T> >(callback);
    }


private:
    using CallbackMap = std::unordered_map <
            const google::protobuf::Descriptor *,
            std::shared_ptr< Callback<NetworkChannelType> > //! 能够指向“所有消息类型的回调函数”
    >;

    CallbackMap m_CallbacksMap;
    ProtobufMessageCallback m_UnknownCallback; //! ConnectionType未知的消息，调用该函数向上层传递。
};

}
