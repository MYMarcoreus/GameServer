#pragma clang diagnostic push
#pragma ide diagnostic ignored "NotImplementedFunctions"
#pragma ide diagnostic ignored "OCUnusedTemplateParameterInspection"

#ifndef LINUXGAMESERVER_SFINAE_H
#define LINUXGAMESERVER_SFINAE_H

#include <iostream>
#include <iterator>
#include <type_traits>
#include <vector>
#include <list>

namespace yy::util {



# if __cplusplus > 201703L
template <typename T>
concept is_iterable_container_v = requires(T t) {
    std::begin(t);  // Check if begin function exists
    std::end(t);    // Check if end function exists
};
#elif __cplusplus >= 201103L
template <typename T>
struct is_iterable_container {
private:
    //! 逗号运算符表达式。逗号运算符表达式会按顺序依次计算每个子表达式，并返回最后一个子表达式的结果。
    //! 在这里，如果 std::begin 和 std::end 是否都能成功调用，那么逗号表达式将会来到std::true_type{}处，该逗号表达式的值就为true_type
    //! 否则，该函数就不会生成，在被调用到时便会查找到备选函数test(...)
    template <typename C>
    static auto test(int) -> decltype(std::begin(std::declval<C>()), std::end(std::declval<C>()), std::true_type{});

    //! 参数为... ，这代表该函数可以匹配任何参数，这是一个备选函数，
    //! 当test(int)编译时推断返回值类型时失败（不是stl容器），便会选择到`auto test(...) -> std::false_type`
    template <typename>
    static auto test(...) -> std::false_type;

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename _Tp>
constexpr bool is_iterable_container_v = is_iterable_container<_Tp>::value;
#endif




}

#endif //LINUXGAMESERVER_SFINAE_H


#pragma clang diagnostic pop

