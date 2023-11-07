#ifndef ____WRAP_SIGSET_H
#define ____WRAP_SIGSET_H

#include <csignal>
#include <initializer_list>


namespace yy::util {

class SigSet
{
public:
    SigSet(): sigset_{} { clear(); }

    /// @brief 传入单信号
    explicit SigSet(int SIGXXXX);

    /// @brief 使用`{}`列表传入多个信号
    SigSet(std::initializer_list<int> siglist);

    /// @brief 清空信号集
    void clear();

    /// @brief 设置全部信号
    void fill();

    /// @brief 将单个信号加入信号集
    void add(int signum);

    /// @brief 将单个信号从信号集中删除
    void del(int signum);

    /// @brief 查询指定信号是否存在于信号集中
    [[nodiscard]] bool has_sig(int signum) const;

    /// @brief 获取内部的sigset_t
    sigset_t &get_sigset() { return sigset_; }

private:
    sigset_t sigset_;
};


}


#endif // !____WRAP_SIGSET_H
