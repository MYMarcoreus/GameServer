#include "wrap_file.h"
#include "log.h"

#include <string>
#include <cstring>
#include <iostream>
#include <filesystem>

namespace yy::util {

#ifdef ____DEBUG
#define FATAL_ERROR_FILE_SYS(cond, funname) \
if( cond ){ \
    YLOG_FATAL("file<%s> "#funname"() error", fullname_.c_str());\
    throw std::system_error{ errno, std::system_category() }; \
}
#else
#define FATAL_ERROR_FILE_SYS(cond, funname) \
if( cond ){ \
    YLOG_FATAL("file<%s> "#funname"() error", fullname_.c_str());\
}
#endif


// 接管文件描述符fd
File::File(const FileDescriper& fd) : FileDescriper(fd) {}

File::File(const std::string & pathname, int O_XXXX)
    : FileDescriper(this->Open(pathname, O_XXXX, 0))
{}

File::File(const std::string & pathname, int O_XXXX, mode_t mode)
    : FileDescriper( this->Open(pathname, O_XXXX, mode) )
{}

int File::Open(const std::string & pathname, int O_XXXX, mode_t mode)
{
    bool flag_exist = false;
    if (!isExist(pathname)) {
        O_XXXX |= O_CREAT;
        flag_exist = true;
    }
    int ret = ::open(pathname.c_str(), O_XXXX, mode);
    FATAL_ERROR_FILE_SYS(ret < 0, open)
    fd_ = ret;

    if(flag_exist)
        std::cout << "已创建文件：" << pathname << std::endl;
    else
        std::cout << "已打开文件：" << pathname << std::endl;

    fullname_ = File::Realpath(pathname);

    return ret;
}

std::string File::getFullname() const { return fullname_; }

std::string File::getDirname() const { return File::Dirname(fullname_); }

std::string File::getBasename() const { return File::Basename(fullname_); }

off_t File::get_size() const
{
    off_t current_pos = this->Lseek(SEEK_CUR, 0); // 保存当前文件指针位置
    off_t file_size = this->Lseek(SEEK_END, 0);   // 获取文件大小 
    this->Lseek(SEEK_SET, current_pos);           // 恢复文件指针位置
    return file_size;
}

off_t File::get_offset() const
{
    return this->Lseek(SEEK_CUR, 0);
}

off_t File::Lseek(int whence, off_t offset) const
{
    off_t ret = ::lseek(fd_, offset, whence);
    FATAL_ERROR_FILE_SYS(ret < 0, lseek)
    return ret;
}

void File::Truncate(off_t length) const
{
    int ret = ::ftruncate(fd_, length);
    FATAL_ERROR_FILE_SYS(ret < 0, ftruncate)
}

struct stat File::Stat() const
{
    struct stat statbuf{};
    int ret = ::fstat(fd_, &statbuf);
    FATAL_ERROR_FILE_SYS(ret < 0, fstat)
    return statbuf;
}

struct stat File::Lstat() const
{
    struct stat statbuf{};
    int ret = ::lstat(fullname_.c_str(), &statbuf);
    FATAL_ERROR_FILE_SYS(ret < 0, lstat)
    return statbuf;
}


void File::Chmod(mode_t mode) const
{
    int ret = ::fchmod(fd_, mode);
    FATAL_ERROR_FILE_SYS(ret < 0, fchmod)
}

bool File::Access(int type) const
{
    // 因为如果没有指定权限的话，access将返回-1并设置errno告知无权限的原因
    // 这实际不是真正的错误，因此需要保存之前的errno执行完后恢复。
    int olderrno = errno;
    errno = 0;
    int ret = ::access(fullname_.c_str(), type);
    if (errno != 0 && ret == -1) {
        fprintf(stderr, "access(%s) returns -1: %s", fullname_.c_str(), strerror(errno));
    }
    errno = olderrno;
    return ret == 0;
}

bool File::Access(const std::string & pathname, int type)
{
    int olderrno = errno;
    errno = 0;
    int ret = ::access(pathname.c_str(), type);
    if (errno != 0 && ret == -1) {
        fprintf(stderr, "access(%s) returns -1: %s", pathname.c_str(), strerror(errno));
    }
    errno = olderrno;
    return ret == 0;
}

struct stat File::Stat(const std::string & pathname)
{
    struct stat st{};
    int ret = ::stat(pathname.c_str(), &st);
    if (errno != 0 && ret < 0) YLOG_FATAL("stat(%s) returns -1: %s", pathname.c_str(), strerror(errno))
    return st;
}


void File::set_filename(const char *newFilename)
{
    for (int i = 0; newFilename[i] != '\0'; i++) {
        if (newFilename[i] == '/')
            YLOG_FATAL("error：set_filename()不能传入路径，只能传入文件的新名字，若要更改文件路径，请使用别的函数")
    }

    std::string old_fullname = fullname_;
    fullname_ = File::Dirname(fullname_) + '/' + newFilename;
//    int ret = ::rename(old_fullname.c_str(), fullname_.c_str());
    std::error_code errorCode;
    std::filesystem::rename(old_fullname, fullname_, errorCode);
    FATAL_ERROR_FILE_SYS(errorCode, rename)
}

std::string File::Realpath(const std::string& pathname) {

    // std::string fullname{};
    // 获取绝对路径
    // auto p_heap = ::realpath(pathname.c_str(), nullptr);
    // if(p_heap == nullptr)
    //     fullname = "";
    // else
    //     std::string(p_heap).swap(fullname); // 将动态分配的p的内存控制权交给fullname_管理

    return std::filesystem::absolute(pathname);
}

std::string File::Dirname(const std::string& fullname) {
    auto pos = fullname.find_last_of('/');
    return pos == std::string::npos ? "" :  fullname.substr(0, pos);
}

std::string File::Basename(const std::string& fullname) {
    auto pos = fullname.find_last_of('/');
    return pos == std::string::npos ? "" : fullname.substr(pos + 1);
}


}
