#ifndef ____IGAMEBASE_H
#define ____IGAMEBASE_H

#include "UserBaseData.h"

namespace yy::app {

class IGameBase
{
public:
    using ptr = std::shared_ptr<IGameBase>;
public:

    virtual void Init() = 0;

    virtual void Update() = 0;

    virtual void AppCommand(const yy::core::UserBaseData::ptr &, int32_t) = 0;
protected:
    IGameBase() = default;
    virtual ~IGameBase() = default;
};


}

#endif //____IGAMEBASE_H
