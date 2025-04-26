#ifndef LINUXGAMESERVER_PROTOBUFDISPATCHER_H
#define LINUXGAMESERVER_PROTOBUFDISPATCHER_H

#include "core_definations.h"
#include "net_definations.h"
#include "noncopyable.h"

#include <functional>
#include <map>
#include <cassert>
#include <google/protobuf/message.h>

namespace yy::core {

//! 为了
template<typename ConnectionType>
class Callback : util::noncopyable {
public:
    virtual ~Callback() = default;

    virtual void OnMessage(const ConnectionType &, const MessagePtr &message) const = 0;
};

//! 用于Protobuf
template<typename ConnectionType, typename T> requires requires {
    requires std::is_base_of_v<google::protobuf::Message, T>;
}
class CallbackT : public Callback<ConnectionType> {
    static_assert(std::is_base_of_v<google::protobuf::Message, T>, "T must be derived from gpb::Message.");
public:
    //! message的子类回调
    using ProtobufMessageTCallback = std::function<void(
            const ConnectionType &,
            const std::shared_ptr<T> &message //! 针对特定的protobuf消息类
    )>;

    CallbackT(const ProtobufMessageTCallback &callback) : m_Callback(callback) {}

    void OnMessage(const ConnectionType &conn, const MessagePtr &message) const override {
        //! 基类指针转换为子类指针
        std::shared_ptr<T> concrete = std::static_pointer_cast<T>(message);
        assert(concrete != NULL);
        m_Callback(conn, concrete);
    }

private:
    ProtobufMessageTCallback m_Callback;
};

template<typename ConnectionType>
class ProtobufDispatcher {
public:
    using ProtobufMessageCallback = std::function<void(
            const ConnectionType &,
            const MessagePtr &message) //! ConnectionType未知的消息类型，因此用MessagePtr引用向上传递
    >;

    explicit ProtobufDispatcher(ProtobufMessageCallback unknownCb) : m_UnknownCallback(unknownCb) {}

    //! 该函数会作为回调被上层（XXXServer）传递给ProtobufCodec
    void OnProtobufMessage(const ConnectionType &conn, const MessagePtr &message) const {
        const auto it = m_CallbacksMap.find(message->GetDescriptor());
        if (it != m_CallbacksMap.end()) {
            //! ConnectionType已知已注册该消息，直接处理之。
            it->second->OnMessage(conn, message);
        } else {
            //! ConnectionType未知的消息，需要向上层传递
            m_UnknownCallback(conn, message);
        }
    }

    //! 注册回调：回调传递子类指针CallbackT<T>，保存的是基类指针Callback，从而使得可以保存各种Message子类的回调
    template<typename T>
    requires requires {
        requires std::is_base_of_v<google::protobuf::Message, T>;
    }
    void RegisterMessageCallback(const typename CallbackT<ConnectionType, T>::ProtobufMessageTCallback & callback) {
        //! 子类指针交给map内的父类指针存储
        m_CallbacksMap[T::descriptor()] = std::make_shared< CallbackT<ConnectionType, T> >(callback);
    }


private:
    using CallbackMap = std::map<
            const google::protobuf::Descriptor *,
            std::shared_ptr< Callback<ConnectionType> > //! 能够指向“所有消息类型的回调函数”
    >;

    CallbackMap m_CallbacksMap;
    ProtobufMessageCallback m_UnknownCallback; //! ConnectionType未知的消息，调用该函数向上层传递。
};

}


#endif //LINUXGAMESERVER_PROTOBUFDISPATCHER_H

