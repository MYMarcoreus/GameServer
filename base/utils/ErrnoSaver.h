#pragma once
#include<cerrno>

namespace yy::util {

// 构造时保存原errno，析构时恢复原errno
class ErrnoSaver {
public:
    ErrnoSaver() : saved_errno_{errno} {}
    ~ErrnoSaver() { errno = saved_errno_; }
    operator int() const { return saved_errno_; } // NOLINT(google-explicit-constructor)
    int operator()() const { return saved_errno_; }

private:
    const int saved_errno_;
};

} // yy::util
