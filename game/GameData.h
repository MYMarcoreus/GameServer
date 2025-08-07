#pragma once

#include<memory>

#include "core_definations.h"

namespace yy::protocol::app
{
class RoomDetailData;
class AccountBaseData;
}

namespace yy::app {

template<class T>
using Ptr = std::shared_ptr<T>;

using UID_t = core::UID_t;
using ROOM_ID_t = uint64_t;
using core::UserConnectionPtr;
using core::MessagePtr;

using RoomDetailDataPtr  = std::shared_ptr<protocol::app::RoomDetailData>;


}

