#include <vector>
#include <string>
#include <iostream>
#include <format>

struct Person {
    std::string name;
    int age{};

    Person(std::string n, int a) : name(std::move(n)), age(a) {
        std::cout << std::format("Constructor called: {}\n", name);
    }

    Person(const Person& obj) {
        std::cout << std::format("Copy constructor called: {}\n", obj.name);
    }

    Person(Person&& obj) noexcept {
        std::cout << std::format("Move constructor called: {}\n", obj.name);
    }

};

int main() {
    std::vector<Person> v;
    v.reserve(100);

    // 调用构造函数
    Person p("Tom", 30); std::cout  << std::endl;

    // 调用push_back(const value_type& __x)
    v.push_back(p);                        // 调用：拷贝构造
    // 调用push_back(value_type&& __x)
    v.push_back(Person("Jerry", 28));      // 调用：构造临时对象 + 移动临时对象
    
    /* 都调用
        template<typename... _Args>
        #if __cplusplus > 201402L
            _GLIBCXX20_CONSTEXPR
            reference
        #else
            void
        #endif
            emplace_back(_Args&&... __args);
        #endif                                  
    */    
    v.emplace_back(p);                     // 调用：拷贝构造
    v.emplace_back("Alice", 25);           // 调用：构造

    return 0;
}
