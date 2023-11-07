#ifndef ____RWLOCK_H
#define ____RWLOCK_H

#include <shared_mutex>
#include <mutex>

namespace yy::util {

using RWLock  = std::shared_mutex; // 读锁
using ReadLockGuard  = std::shared_lock<std::shared_mutex>; // 读锁
using WriteLockGuard = std::unique_lock<std::shared_mutex>; // 写锁

}


#endif //____RWLOCK_H
