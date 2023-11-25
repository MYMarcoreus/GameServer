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


template<typename ConnectionType>
class Callback : util::noncopyable {
public:
    virtual ~Callback() = default;

    virtual void OnMessage(const ConnectionType &, const MessagePtr &message) const = 0;
};


template<typename T, typename ConnectionType> requires requires {
    requires std::is_base_of_v<google::protobuf::Message, T>;
}
class CallbackT : public Callback<ConnectionType> {
    static_assert(std::is_base_of_v<google::protobuf::Message, T>, "T must be derived from gpb::Message.");
public:
    //! message的子类回调
    using ProtobufMessageTCallback = std::function<void(const ConnectionType &, const std::shared_ptr<T> &message)>;

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
    using ProtobufMessageCallback = std::function<void(const ConnectionType &, const MessagePtr &message)>;

    explicit ProtobufDispatcher(ProtobufMessageCallback defaultCb) : m_DefaultCallback(defaultCb) {}

    //! 由上层（XXXServer）传递给ProtobufCodec
    void OnProtobufMessage(const ConnectionType &conn, const MessagePtr &message) const {
        const auto it = m_CallbacksMap.find(message->GetDescriptor());
        if (it != m_CallbacksMap.end()) {
            it->second->OnMessage(conn, message);
        } else {
            m_DefaultCallback(conn, message);
        }
    }

    //! 注册回调：回调传递子类指针CallbackT<T>，保存的是基类指针Callback，从而使得可以保存各种Message子类的回调
    template<typename T>
    requires requires {
        requires std::is_base_of_v<google::protobuf::Message, T>;
    }
    void RegisterMessageCallback(const typename CallbackT<T, ConnectionType>::ProtobufMessageTCallback &callback) {
        m_CallbacksMap[T::descriptor()] = std::make_shared<CallbackT<T, ConnectionType>>(callback);
    }


private:
    using CallbackMap = std::map<const google::protobuf::Descriptor *, std::shared_ptr<Callback<ConnectionType> > >;

    CallbackMap m_CallbacksMap;
    ProtobufMessageCallback m_DefaultCallback; //! 未注册
};

}


#endif //LINUXGAMESERVER_PROTOBUFDISPATCHER_H

