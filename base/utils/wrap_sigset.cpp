#include "wrap_sigset.h"
#include "log.h"
#include<stdexcept>
#include<exception>

namespace yy::util {

#ifdef ____DEBUG
#define ERROR_SIGSET(cond, funname) \
if( cond ){ \
    YLOG_FATAL("SigSet "#funname"() error") \
    throw std::system_error{ errno, std::system_category() }; \
}
#else
#define ERROR_SIGSET(cond, funname) \
if( cond ){ \
    YLOG_FATAL("SigSet "#funname"() error") \
}
#endif


SigSet::SigSet(int signum)
{
    this->clear();
    this->add(signum);
}

SigSet::SigSet(std::initializer_list<int> siglist)
{
    this->clear();
    for (auto signum: siglist)
        this->add(signum);
}

void SigSet::clear()
{
    ERROR_SIGSET(sigemptyset(&sigset_) < 0, sigemptyset);
}

void SigSet::fill()
{
    ERROR_SIGSET(sigfillset(&sigset_) < 0, sigfillset);
}

void SigSet::add(int signum)
{
    ERROR_SIGSET(sigaddset(&sigset_, signum) < 0, sigaddset);
}

void SigSet::del(int signum)
{
    ERROR_SIGSET(sigdelset(&sigset_, signum) < 0, sigdelset);
}

bool SigSet::has_sig(int signum) const
{
    int ret = sigismember(&sigset_, signum);
    ERROR_SIGSET(ret < 0, sigdelset);
    return ret == 1;
}


}