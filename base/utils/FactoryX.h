#pragma once

//! C++实现简单的反射机制来完成简单工厂方法（根据类名来生成对象）

#include <iostream>
#include <string>
#include <map>
#include <functional>
#include <memory>
#include <any>

namespace yy::util {


//! 设计一个工厂类，类中有一个std::map，用于保存类名和创建实例的回调函数。通过类工厂来动态创建类对象；
//! 当程序开始运行时，将回调函数存入std::unordered_map （哈希表）里面，类名字做为map的key值；
//! 依据传入的类名，生产对应的具体对象的工厂（当然要成功得到对象还得先调用REG_CLASS宏传入类名完成注册）
// 简单工厂类，只支持默认构造函数
template<typename Base>
class FactoryX {
public:
    using Creator = std::function<std::shared_ptr<Base>()>;

    static void registerClass(const std::string &className, Creator creator) {
        getMap()[className] = std::move(creator);
        std::cout << "class<" << className << "> 已注册\n";
    }

    static std::shared_ptr<Base> CreateObject(const std::string &className) {
        auto &map = getMap();
        auto it = map.find(className);
        if (it == map.end()) {
            std::cerr << "class<" << className << "> 未注册\n";
            return nullptr;
        }
        return it->second();
    }

private:
    static std::unordered_map <std::string, Creator> &getMap() {
        static std::unordered_map <std::string, Creator> constructor;
        return constructor;
    }
};


} // yy::util
