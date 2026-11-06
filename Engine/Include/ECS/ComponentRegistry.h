/*=============================================================================

 File   : ComponentRegistry.h
 Desc   : Component型の識別子と型操作を管理する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _COMPONENT_REGISTRY_H_
#define _COMPONENT_REGISTRY_H_

#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <unordered_map>

#include "Log/Logger.h"

namespace Hestia
{
    using ComponentTypeId = uint64_t;

    struct ComponentInfo
    {
        ComponentTypeId id = 0;
        std::string name;
        size_t size = 0;
        size_t alignment = 0;
        void (*construct)(void*) = nullptr;
        void (*destroy)(void*) noexcept = nullptr;
        void (*moveConstruct)(void*, void*) noexcept = nullptr;
    };

    /// @brief 登録名とComponent型の対応および寿命操作を所有する。
    class ComponentRegistry
    {
    public:
        ComponentRegistry() = default;
        ComponentRegistry(const ComponentRegistry&) = delete;
        ComponentRegistry& operator=(const ComponentRegistry&) = delete;

        /// @brief UTF-8登録名から決定的な型IDを作り、型操作を登録する。
        /// @param name 名前空間を含むComponent型名。
        /// @return 登録に成功した型ID。競合・不正な名前や型では空。
        /// @note 内部のメモリ確保で発生した例外は呼び出し元へ伝播する
        /// @note Componentはデフォルト構築可能で、Move構築と破棄がnoexceptであること
        /// 同じ型・同じ名前の再登録は同じIDを返し、別名・別型・ID衝突は拒否する
        /// IDは登録名から生成するため、名前変更後の互換性は保証しない
        template<class T>
        std::optional<ComponentTypeId> Register(std::string_view name)
        {
            using Component = std::remove_cv_t<T>;  // constの除去
            static_assert(std::is_same_v<T, Component>);

            if (name.empty())
                return std::nullopt;

            if constexpr (
                !std::is_default_constructible_v<Component> ||      // デフォルト構築可能であること
                !std::is_nothrow_destructible_v<Component> ||       // 破棄がnoexceptであること
                !std::is_nothrow_move_constructible_v<Component>)   // Move構築がnoexceptであること
            {
                std::string error = "Component registration failed for " + std::string(name) + ": ";
                if(!std::is_default_constructible_v<Component>)
                    error += "not default constructible, ";
                if(!std::is_nothrow_destructible_v<Component>)
                    error += "not nothrow destructible, ";
                if(!std::is_nothrow_move_constructible_v<Component>)
                    error += "not nothrow move constructible, ";

                Logger::Error(error);

                return std::nullopt;
            }
            else
            {
                // Infoの作成と登録を行う。型と名前の衝突はStoreで確認する
                ComponentInfo info;
                info.name.assign(name);

                info.id = HashName(name);
                info.size = sizeof(Component);
                info.alignment = alignof(Component);

                // デフォルトコンストラクタ、デストラクタ、ムーブコンストラクタを登録する
                info.construct = [](void* destination)
                {
                    ::new (destination) Component();
                };
                info.destroy = [](void* object) noexcept
                {
                    static_cast<Component*>(object)->~Component();
                };
                info.moveConstruct = [](void* destination, void* source) noexcept
                {
                    ::new (destination) Component(std::move(*static_cast<Component*>(source)));
                };

                return Store<Component>(std::move(info));
            }
        }

        /// @brief 登録済みの型からComponentTypeIdを取得する。
        /// @return 登録済みの場合は型ID、それ以外は空。
        template<class T>
        std::optional<ComponentTypeId> GetId() const
        {
            const auto it = m_typeIds.find(std::type_index(typeid(std::remove_cv_t<T>)));
            if (it == m_typeIds.end())
                return std::nullopt;

            return it->second;
        }

        /// @brief 型IDに対応する登録情報を取得する。
        /// @param id 検索するComponent型ID。
        /// @return 登録情報への非所有ポインタ。未知のIDではnullptr。
        const ComponentInfo* GetInfo(ComponentTypeId id) const;

        /// @brief 登録をすべて解除する。
        void Finalize();

    private:
        static ComponentTypeId HashName(std::string_view name) noexcept;

        template<class T>
        std::optional<ComponentTypeId> Store(ComponentInfo info)
        {
            const std::type_index type(typeid(T));
            const auto typeIt = m_typeIds.find(type);
            // typeIDから登録済みの型情報がある場合は、名前が一致するか確認する
            if (typeIt != m_typeIds.end())
            {
                const ComponentInfo* existing = GetInfo(typeIt->second);
                if (existing != nullptr && existing->name == info.name)
                    return existing->id;

                return std::nullopt;
            }

            if (m_components.contains(info.id))
                return std::nullopt;

            // ここまでで型と名前の衝突がないことを確認したので、両方の索引に登録する
            const ComponentTypeId id = info.id;
            m_components.emplace(id, std::move(info));
            m_typeIds.emplace(type, id);

            return id;
        }

        std::unordered_map<ComponentTypeId, ComponentInfo> m_components;
        std::unordered_map<std::type_index, ComponentTypeId> m_typeIds;
    };
}

#endif // _COMPONENT_REGISTRY_H_
