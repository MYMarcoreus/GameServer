#pragma once

#include<cstdint>
#include<cstring>
#include<memory>

#include "UserConnection.h"

namespace yy::app {

template<class T>
using Ptr = std::shared_ptr<T>;

}

