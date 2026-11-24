/*=============================================================================

 File   : SerializeFields.h
 Desc   : Componentの保存フィールド宣言を型ごとの読み書き操作に変換する。

------------------------------------------------------------------------------

 Date   : 2026/11/19
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _SERIALIZE_FIELDS_H_
#define _SERIALIZE_FIELDS_H_

#include <functional>
#include <limits>
#include <optional>
#include <string_view>
#include <tuple>
#include <typeindex>
#include <type_traits>
#include <utility>

#include "Common/Include/Math/Quaternion.h"
#include "Common/Include/Math/Vector3.h"
#include "Serialize/SerializedEntity.h"

namespace Hestia
{
    namespace SerializeDetail
    {
        template<class T>
        inline constexpr bool IsVector = false;

        template<class Element, class Allocator>
        inline constexpr bool IsVector<std::vector<Element, Allocator>> = true;

        template<class T>
        SerializedValue ToSerializedValue(const T& value)
        {
            using Value = std::remove_cvref_t<T>;

            // スカラーは型を保ったまま、整数と浮動小数の幅を揃える
            if constexpr (
                std::is_same_v<Value, bool> ||
                std::is_same_v<Value, std::string> ||
                std::is_same_v<Value, AssetRef>)
                return { value };

            else if constexpr (std::is_integral_v<Value> && sizeof(Value) <= sizeof(uint64_t))
            {
                using Integer = std::conditional_t<std::is_signed_v<Value>, int64_t, uint64_t>;
                return { static_cast<Integer>(value) };
            }
            else if constexpr (std::is_same_v<Value, float> || std::is_same_v<Value, double>)
                return { static_cast<double>(value) };

            // 数学型の成分順を固定し、vectorの要素は同じ変換に戻す
            else if constexpr (std::is_same_v<Value, Vector3>)
                return { SerializedValue::Array{ {double{value.x}}, {double{value.y}}, {double{value.z}} } };

            else if constexpr (std::is_same_v<Value, Quaternion>)
                return { SerializedValue::Array{
                    {double{value.x}}, {double{value.y}}, {double{value.z}}, {double{value.w}} } };

            else if constexpr (IsVector<Value>)
            {
                using Element = typename Value::value_type;
                SerializedValue::Array items;
                items.reserve(value.size());
                for (const Element& item : value)
                    items.push_back(ToSerializedValue(item));

                return { std::move(items) };
            }
            else
                static_assert(!std::is_same_v<Value, Value>, "Unsupported serialize field type");
        }

        // YAMLとモデルから得た同じ保存値を、Registryが宣言したC++型へ戻す
        template<class T>
        std::optional<T> FromSerializedValue(const SerializedValue& value)
        {
            if constexpr (
                std::is_same_v<T, bool> ||
                std::is_same_v<T, std::string> ||
                std::is_same_v<T, AssetRef>)
            {
                if (const auto* item = std::get_if<T>(&value.m_value))
                    return *item;
            }
            else if constexpr (std::is_integral_v<T>)
            {
                if (const auto* item = std::get_if<int64_t>(&value.m_value))
                {
                    if constexpr (std::is_signed_v<T>)
                    {
                        if (*item >= std::numeric_limits<T>::lowest() && *item <= std::numeric_limits<T>::max())
                            return static_cast<T>(*item);
                    }
                    else if (*item >= 0 && static_cast<uint64_t>(*item) <= std::numeric_limits<T>::max())
                        return static_cast<T>(*item);
                }
                if (const auto* item = std::get_if<uint64_t>(&value.m_value);
                    item && *item <= static_cast<uint64_t>(std::numeric_limits<T>::max()))
                    return static_cast<T>(*item);
            }
            else if constexpr (std::is_floating_point_v<T>)
            {
                if (const auto* item = std::get_if<double>(&value.m_value))
                {
                    if (*item == std::numeric_limits<double>::infinity() ||
                        *item == -std::numeric_limits<double>::infinity() || *item != *item ||
                        (*item >= std::numeric_limits<T>::lowest() && *item <= std::numeric_limits<T>::max()))
                        return static_cast<T>(*item);
                }
                if (const auto* item = std::get_if<int64_t>(&value.m_value))
                    return static_cast<T>(*item);
                if (const auto* item = std::get_if<uint64_t>(&value.m_value))
                    return static_cast<T>(*item);
            }
            else if constexpr (std::is_same_v<T, Vector3> || std::is_same_v<T, Quaternion> || IsVector<T>)
            {
                const auto* items = std::get_if<SerializedValue::Array>(&value.m_value);
                if (!items)
                    return std::nullopt;

                if constexpr (IsVector<T>)
                {
                    T result;
                    result.reserve(items->size());
                    for (const auto& item : *items)
                    {
                        auto decoded = FromSerializedValue<typename T::value_type>(item);
                        if (!decoded)
                            return std::nullopt;
                        result.push_back(std::move(*decoded));
                    }
                    return result;
                }
                else
                {
                    constexpr size_t count = std::is_same_v<T, Vector3> ? 3 : 4;
                    if (items->size() != count)
                        return std::nullopt;
                    float values[count];
                    for (size_t i = 0; i < count; ++i)
                    {
                        const auto decoded = FromSerializedValue<float>((*items)[i]);
                        if (!decoded)
                            return std::nullopt;
                        values[i] = *decoded;
                    }
                    if constexpr (count == 3)
                        return T(values[0], values[1], values[2]);
                    else
                        return T(values[0], values[1], values[2], values[3]);
                }
            }
            return std::nullopt;
        }
    }

    /// @brief 保存フィールドのキー・値型・読み書き操作
    /// @note 呼び出し中だけ、同じ生存Entityの登録済みComponentとHandleを借用する
    struct SerializeFieldDescriptor
    {
        std::string_view name;
        std::type_index valueType;
        std::function<SerializedValue(const void*, EntityHandle)> read;
        bool (*accepts)(const SerializedValue&);
        std::function<bool(void*, EntityHandle, const SerializedValue&)> write;
    };

    /// @brief SerializeFieldDescriptorを1個作るラムダをフィールドごとに保持する
    /// @note Fields...は各ラムダの型の並び。異なる型も同じtupleに格納できる
    template<class... Fields>
    class SerializeFieldList
    {
    public:
        std::tuple<Fields...> fields;

        /// @brief メンバポインタから読み書き操作を作る
        template<class Component, class Value>
        constexpr auto operator()(std::string_view name, Value Component::* member) const
        {
            return Append([name, member]
            {
                return SerializeFieldDescriptor{
                    name, typeid(Value),
                    [member](const void* component, EntityHandle)
                    {
                        return SerializeDetail::ToSerializedValue(static_cast<const Component*>(component)->*member);
                    },
                    [](const SerializedValue& value) { return SerializeDetail::FromSerializedValue<Value>(value).has_value(); },
                    [member](void* component, EntityHandle, const SerializedValue& value)
                    {
                        auto typed = SerializeDetail::FromSerializedValue<Value>(value);
                        if (!typed)
                            return false;

                        static_cast<Component*>(component)->*member = std::move(*typed);
                        return true;
                    }
                };
            });
        }

        /// @brief EntityHandleを取る既存のGetterとSetterを読み書きに使う
        template<class Value>
        constexpr auto operator()(std::string_view name, Value (*get)(EntityHandle),
            bool (*set)(EntityHandle, const Value&)) const
        {
            return Append([name, get, set]
            {
                return SerializeFieldDescriptor{
                    name, typeid(Value),
                    [get](const void*, EntityHandle entity) { return SerializeDetail::ToSerializedValue(get(entity)); },
                    [](const SerializedValue& value) { return SerializeDetail::FromSerializedValue<Value>(value).has_value(); },
                    [set](void*, EntityHandle entity, const SerializedValue& value)
                    {
                        const auto typed = SerializeDetail::FromSerializedValue<Value>(value);
                        return typed && set(entity, *typed);
                    }
                };
            });
        }

    private:
        template<class Field>
        constexpr auto Append(Field field) const
        {
            // 既存のラムダと今回のラムダを連結し、1項目増えた型のリストを返す
            // 次のoperator()はこの戻り値を使ってさらに1項目を追加する
            return SerializeFieldList<Fields..., Field>{ std::tuple_cat(fields, std::tuple{ field }) };
        }
    };
}

// 各 (キー, ...) は直前の戻り値に対するoperator()呼び出しになる
// 例: SerializeFieldList<>{}("a", &T::a)("b", &T::b)
#define SERIALIZE_FIELDS(...) \
    static constexpr auto SerializeFields() { return ::Hestia::SerializeFieldList<>{} __VA_ARGS__; }

#endif // _SERIALIZE_FIELDS_H_
