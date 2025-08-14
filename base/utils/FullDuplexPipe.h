#pragma once
#ifdef  ____LINUX

#include <cstddef>


namespace yy::util {

class FullDuplexPipe
{
public:
    FullDuplexPipe();

    ~FullDuplexPipe();

    int Write(const void * __buf, size_t __n);

    int Read(void * __buf, size_t __n);

    // 一般作写端write(sideW)
    int sideW() const { return pipefds_[1];}

    // 一般作读端read(sideR)
    int sideR() const { return pipefds_[0];}

private:
    int pipefds_[2];
};


}



#endif
