#ifndef ____GAMENOTIFIER_H
#define ____GAMENOTIFIER_H

#include "IServer.h"


namespace yy::app{

extern void AppNotifier_Connect   (const core::UserBaseData::ptr &, int32_t);
extern void AppNotifier_Secutiry  (const core::UserBaseData::ptr &, int32_t);
extern void AppNotifier_Disconnect(const core::UserBaseData::ptr &, int32_t);
extern void AppNotifier_Command   (const core::UserBaseData::ptr &, int32_t);

}



#endif //____GAMENOTIFIER_H
