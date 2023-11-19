#ifndef GAMESERVER_CROSS_PLATFORM_DEFINES_H
#define GAMESERVER_CROSS_PLATFORM_DEFINES_H



#ifdef ____WINDOWS
#include <sys/stat.h>
#include <io.h>
#include <Windows.h>
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
#define OPEN(file, mode)            _open(file, mode, S_IWRITE | S_IREAD)
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

