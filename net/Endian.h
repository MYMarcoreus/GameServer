#pragma once

#include "socket_definations.h"

namespace yy::net
{
inline uint64_t host_to_network64(const uint64_t host64) {
    // 先拆成两个32位部分，再分别htonl，拼回64位
    return  static_cast<uint64_t>(htonl((uint32_t)(host64 >> 32))) |
           (static_cast<uint64_t>(htonl((uint32_t)(host64 & 0xFFFFFFFF))) << 32);
}

inline uint32_t host_to_network32(const uint32_t host32) {
    // return host32;
    return htonl(host32);
}

inline uint16_t host_to_network16(const uint16_t host16) {
    // return host16;
    return htons(host16);
}

inline uint64_t network_to_host64(const uint64_t net64) {
    return  static_cast<uint64_t>(ntohl((uint32_t)(net64 >> 32))) |
           (static_cast<uint64_t>(ntohl((uint32_t)(net64 & 0xFFFFFFFF))) << 32);
}

inline uint32_t network_to_host32(const uint32_t net32) {
    // return net32;
    return ntohl(net32);
}

inline uint16_t network_to_host16(const uint16_t net16) {
    // return net16;
    return ntohs(net16);
}
}