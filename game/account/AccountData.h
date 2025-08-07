#pragma once
#include <cstdint>
#include <string>

namespace yy::app::account
{

struct AccountData
{
    std::string username;
    std::string password;
    uint64_t    uid;
};

}
