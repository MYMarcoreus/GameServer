#ifndef ____GAMEDATA_H
#define ____GAMEDATA_H

#include<cstdint>
#include<cstring>
#include<memory>
#include<UserBaseData.h>
#include"player.pb.h"

namespace yy::app {

#pragma pack(push, packing)
#pragma pack(1)

using UID_t = uint32_t;

template<class T>
using Ptr = std::shared_ptr<T>;


#pragma pack(pop, packing)
}

#endif //____GAMEDATA_H
