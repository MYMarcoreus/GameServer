#pragma once
#include "core_definations.h"
#include "game.pb.h"

namespace yy::app::logic
{
using PlayerBaseDataPtr = std::shared_ptr<protocol::app::PlayerBaseData>;

class Player {
public:
    Player(const core::UserConnectionPtr& conn, const PlayerBaseDataPtr& basedata): conn_{conn}, basedata_{basedata} { }

    core::UID_t get_uid() const { return basedata_->account_data().uid(); }
    core::UserConnectionPtr get_conn() const { return conn_; }
    PlayerBaseDataPtr get_base_data() const { return basedata_; }
private:
    core::UserConnectionPtr conn_;
    PlayerBaseDataPtr basedata_;
};

using PlayerPtr = std::shared_ptr<Player>;

}
