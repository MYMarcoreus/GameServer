#ifndef ____COPYABLE_H
#define ____COPYABLE_H

namespace yy::util
{

// 继承该类以禁止拷贝
class copyable
{
protected:
    copyable() = default;
    ~copyable() = default;
};

}

#endif //____COPYABLE_H
