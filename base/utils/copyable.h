#pragma once
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
