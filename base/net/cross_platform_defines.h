#ifndef GAMESERVER_CROSS_PLATFORM_DEFINES_H
#define GAMESERVER_CROSS_PLATFORM_DEFINES_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif




#ifdef ____MSVC
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <basetsd.h>

    #define NUM_WRITE_IOVEC 16
    #define IOV_TYPE WSABUF
    #define IOV_PTR_FIELD buf
    #define IOV_LEN_FIELD len
    #define IOV_LEN_TYPE unsigned long
#endif
#ifdef ____GNUC

    #define IOV_TYPE struct iovec
    #define IOV_PTR_FIELD iov_base
    #define IOV_LEN_FIELD iov_len
    #define IOV_LEN_TYPE size_t


#endif





#ifdef ____WINDOWS
    #include <sys/stat.h>
    #include <io.h>
    #include <windows.h>
    typedef SSIZE_T ssize_t;
    using sighandler_t = void(*)(int);
#else
    #include <sys/time.h>
    #include <unistd.h>
#endif

#include <fcntl.h>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <fstream>


#ifdef ____WINDOWS
    #define OPEN(file, mode)        _open(file, mode, S_IWRITE | S_IREAD)
    #define WRITE(fd, data, len)    _write(fd, data, len)
    #define FLUSH(fd)               _commit(fd)
    #define CLOSE(fd)               _close(fd)
    #define SLEEP(s)                Sleep(1000 * s)
#endif // ____WINDOWS


#ifdef ____LINUX
    #define OPEN(file, mode)        open(file, mode, S_IRUSR | S_IWUSR | S_IRGRP)
    #define WRITE(fd, data, len)    write(fd, data, len);
    #define FLUSH(fd)               fsync(fd)
    #define CLOSE(fd)               close(fd)
    #define SLEEP(s)                sleep(s)
#endif // ____LINUX

#endif //GAMESERVER_CROSS_PLATFORM_DEFINES_H

