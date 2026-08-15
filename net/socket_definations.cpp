#include "socket_definations.h"
#include "util_functions.h"


namespace yy::SocketApiWrapper {

std::string SocketResult::GetErrorInfo() const {
    return yy::util::GetErrorInfo(errorCode);
}

bool SocketResult::HasError() const {
#ifdef ____WINDOWS
    return result == SOCKET_ERROR;
#elif defined(____LINUX)
    return result < 0;
#else
#error Platform not supported
#endif
}


}