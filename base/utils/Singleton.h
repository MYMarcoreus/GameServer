#pragma once

#include <utility>

/// @brief 在首次Instance时会调用子类T的构造函数和析构函数，因此Singleton必须能够调用子类的构造/析构函数,所以子类T必须声明Singleton为其友元
#define SINGLETON_NECESSITY(classname) friend class Singleton<classname>;

/// @brief 因为SingletonPtr使用了std::shared_ptr，而在std::shared_ptr会使用__gnu_cxx::new_allocator调用子类T的构造函数，因此子类需额外将__gnu_cxx::new_allocator声明为其友元
// #define SINGLETON_NECESSITY(classname)  \
//     friend class SingletonPtr<classname>;  \
//     friend class __gnu_cxx::new_allocator<classname>;

// 单例对象在静态区中：懒汉式
template<typename T> //! T将会是Singleton的子类
class Singleton
{
public:
    //! 使用C++11的变参模板，支持有参数的构造函数，但存在同一类型的参数列表参数值不同的Instance使用同一个对象的bug
    template<typename ... Args>
    static T & Instance(Args &&... args)
    {
        /*  local static对象的初始化发生在控制流第一次执行到该对象的初始化语句时，是懒汉式
                在C++11之前，local static对象的初始化存在线程安全问题，可能会造成对象的重复构造，而这需要以双重检查+锁来防止
                在C++11之后，local static对象的初始化是线程安全的，这是因为新的C++标准规定了当一个线程正在初始化一个变量的时候，
            其他线程必须得等到该初始化完成以后才能访问它。
        （non-local static 对象的初始化发生在main函数执行之前，也即main函数之前的单线程启动阶段，所以不存在线程安全问题）*/

        static T* pinstance = new T{std::forward<Args>(args)...}; //! 经测试，在程序结束之后，local static 对象会被自动析构。

        return *pinstance;
    }

protected: //! 单例基类的构造或析构需要被子类(T)继承，然而也不能被外部使用，因此必须为protected
    Singleton() = default;
    virtual ~Singleton() = default;

    // 禁止复制
    Singleton(const Singleton &) = delete;
    Singleton &operator=(const Singleton &) = delete;
};


// 单例对象在堆中：有继承关系时，使用该类
/*
template<typename T>
class SingletonPtr
{
public:
    template<typename ... Args>
    static inline std::shared_ptr<T> InstancePtr(Args &&... args)
    {
        // 使用智能指针局部静态变量：懒汉式，被使用才进行构造
        static std::shared_ptr<T> instance_ptr = std::make_shared<T>(std::forward<Args>(args)...);
        return instance_ptr;
    }

protected: //! 单例基类的构造或析构需要被子类(T)继承，然而也不能被外部使用，因此必须为protected
    SingletonPtr() = default;
    virtual ~SingletonPtr() = default;

    // 禁止复制
    SingletonPtr(const SingletonPtr &) = delete;
    SingletonPtr &operator=(const SingletonPtr &) = delete;
};
*/

