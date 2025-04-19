
#ifndef GAMESERVER_CLASSREGISTER_H
#define GAMESERVER_CLASSREGISTER_H

#include "FactoryX.h"  // 引入FactoryX
#include <any>
#include <utility>
#include <vector>

namespace yy::util {

// 注册一个只支持默认构造函数的类
template<typename Base, typename Derived>
class ClassRegister {
public:
    explicit ClassRegister(const std::string& className) {
        FactoryX<Base>::registerClass(className, []() -> std::shared_ptr<Base> {
            return std::make_shared<Derived>();
        });
    }
};


} // yy::util

#endif //GAMESERVER_CLASSREGISTER_H
