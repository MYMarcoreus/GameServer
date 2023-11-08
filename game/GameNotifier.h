#ifndef ____GAMENOTIFIER_H
#define ____GAMENOTIFIER_H

#include "IServer.h"


namespace yy::app{

extern void AppNotifier_Secutiry  (const yy::net::TcpConnectionPtr &, int32_t);
extern void AppNotifier_Disconnect(const yy::net::TcpConnectionPtr &, int32_t);

}



#endif //____GAMENOTIFIER_H
