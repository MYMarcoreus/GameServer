#pragma once

#include<cstdint>
#include<cstring>
#include<memory>
#include<UserConnection.h>
#include"room.pb.h"

namespace yy::app {


using UID_t = uint64_t;

template<class T>
using Ptr = std::shared_ptr<T>;


}

