#ifndef ____GAMEDATA_H
#define ____GAMEDATA_H

#include<cstdint>
#include<cstring>
#include<memory>
#include<UserConnection.h>
#include"player.pb.h"

namespace yy::app {


using UID_t = uint64_t;

template<class T>
using Ptr = std::shared_ptr<T>;


}

#endif //____GAMEDATA_H

