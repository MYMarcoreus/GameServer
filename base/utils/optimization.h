#ifndef ____OPTIMIZATION_H
#define ____OPTIMIZATION_H


#if defined __GNUC__ || defined __llvm__
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率成立
#   define LIKELY(x)       __builtin_expect(!!(x), 1)
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率不成立
#   define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#   define LIKELY(x)      (x)
#   define UNLIKELY(x)      (x)
#endif



#endif //____OPTIMIZATION_H

