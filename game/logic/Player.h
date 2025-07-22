#pragma once
#include "core_definations.h"
#include "player.pb.h"

namespace yy::app::logic
{

using core::UserConnectionPtr;
using PlayerBaseDataPtr = std::shared_ptr<protocol::app::PlayerBaseData>;
using UID_t = uint64_t;


class Player {
public:
    Player(const UserConnectionPtr& conn, const PlayerBaseDataPtr& basedata): conn_{conn}, basedata_{basedata} { }

    UID_t GetUID() const { return basedata_->uid(); }
    UserConnectionPtr GetConn() const { return conn_; }
    PlayerBaseDataPtr GetBaseData() const { return basedata_; }
private:
    UserConnectionPtr conn_;
    PlayerBaseDataPtr basedata_;
};

using PlayerPtr = std::shared_ptr<Player>;

}
