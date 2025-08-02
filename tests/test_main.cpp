#include "util_functions.h"
#include <iostream>

#include "Timestamp.h"

int main()
{
    std::cout << yy::util::GenerateToken() << std::endl;
    std::cout << yy::util::GenerateRoomID() << std::endl;
    const auto tick_interval = std::chrono::duration_cast<yy::net::Nanoseconds>(std::chrono::duration<double>(1.0 / 128));
    std::cout << tick_interval << std::endl;
    std::cout << std::chrono::duration<double>(1.0 / 128) << std::endl;
}
