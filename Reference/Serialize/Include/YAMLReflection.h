#pragma once

#include <string_view>
#include <tuple>
#include <utility>

// 反射定義を持たない型は、Reflect<T>を条件とした部分特殊化対象にならない。
template <typename T>
struct Reflect;

// 反射対象型のメンバを列挙する特殊化を簡潔に記述する。
#define REFLECT_BEGIN(type) \
    template <> struct Reflect<type> \
    { \
        using ReflectedType = type; \
        static constexpr auto Members() \
        { \
            return std::make_tuple(

#define REFLECT_FIELD(member) \
    std::make_pair(std::string_view{ #member }, &ReflectedType::member)

#define REFLECT_END() \
            ); \
        } \
    };

// privateメンバをReflect特殊化から参照するため、対象型の本体内で使用する。
#define REFLECT_FRIEND(type) friend struct ::Reflect<type>;
