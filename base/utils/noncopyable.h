#ifndef ____NONCOPYABLE_H
#define ____NONCOPYABLE_H

namespace yy::util
{

// 继承该类以禁止拷贝
class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete;
    void operator=(const noncopyable&) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};

}

#endif //____NONCOPYABLE_H

