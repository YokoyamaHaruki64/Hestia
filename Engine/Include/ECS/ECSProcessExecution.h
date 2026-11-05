/*=============================================================================

 File   : ECSProcessExecution.h
 Desc   : Process関数型のTraitsと共通呼び出し入口を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ECS_PROCESS_EXECUTION_H_
#define _ECS_PROCESS_EXECUTION_H_

#include <cassert>
#include <cstddef>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "ECS/Chunk.h"
#include "Log/Logger.h"

namespace Hestia
{
    /// @brief Engineから指定されるProcess実行phase
    enum class ECSProcessPhase
    {
        FrameBegin,
        Update,
        LateUpdate,
        PreRender,
        FixedUpdate,
    };

    // @brief Process関数が要求するComponentの型とアクセス種別
    enum class ECSComponentAccess
    {
        Read,
        Write,
    };

    // @brief Process関数が要求するComponentの型とアクセス種別
    struct ECSProcessComponent
    {
        ComponentTypeId m_type = 0;
        ECSComponentAccess m_access = ECSComponentAccess::Read;
    };

    /// @brief ProcessをChunkと列番号で呼び出す共通入口
    using ECSProcessInvoke = void (*)(Chunk&, std::span<const size_t>);

    /// @brief void(size_t, const Entity*, Components*...)形式の関数をProcessへ接続するTraits
    /// @note 関数はfree関数またはstatic関数とし、const Component*をReadとして扱う
    template<class FunctionType>
    struct ECSProcessFunctionTraits;

    // 固定形式に対応する特殊化
    template<class... Components>
    struct ECSProcessFunctionTraits<void (*)(size_t, const Entity*, Components*...)>
    {
        /// @note const Component*はRead、それ以外のComponent*はWriteとして登録する
        /// Invoke対象にはinactiveや破棄予約中を含む全rowが渡るため、必要に応じてIsActiveで除外する
        template<auto Function>
        static void Invoke(Chunk& chunk, std::span<const size_t> columnIndices)
        {
            // make_index_sequenceでComponentsの数だけの整数列を生成し、InvokeImplに渡す
            InvokeImpl<Function>(chunk, columnIndices,
                std::make_index_sequence<sizeof...(Components)>{});
        }

        /// @brief Process関数が要求するComponent型とアクセス種別を取得する
        /// @param registry ComponentRegistryの参照
        /// @return 要求するComponent型とアクセス種別の配列
        static std::optional<std::vector<ECSProcessComponent>> BuildComponents(
            const ComponentRegistry& registry)
        {
            std::vector<ECSProcessComponent> result;
            result.reserve(sizeof...(Components));

            if (!(AppendComponent<Components>(registry, result) && ...))
                return std::nullopt;

            return result;
        }

    private:
        template<class T>
        static bool AppendComponent(const ComponentRegistry& registry,
            std::vector<ECSProcessComponent>& components)
        {
            using Component = std::remove_const_t<T>;
            static_assert(!std::is_same_v<Component, Entity>);  // EntityはProcess関数の引数として固定で渡されるため、Componentとして登録しない
            static_assert(!std::is_pointer_v<Component>);       // Component型を見るためポインタは弾く

            const auto id = registry.GetId<Component>();
            if (!id)
            {
                Logger::Error("ECS Process requires an unregistered Component");
                return false;
            }

            components.push_back({ *id, std::is_const_v<T>
                ? ECSComponentAccess::Read : ECSComponentAccess::Write });
            return true;
        }

        template<auto Function, size_t... Indices>
        static void InvokeImpl(Chunk& chunk,
            std::span<const size_t> columnIndices,
            std::index_sequence<Indices...>)
        {
            assert(columnIndices.size() == sizeof...(Components));

            const std::span<const Entity> entities = chunk.GetEntities();
            // SoA列はrow 0から連続するため、Entity数を各Component列の要素数として渡す
            Function(entities.size(), entities.data(),
                static_cast<Components*>(chunk.GetElement(columnIndices[Indices], 0))...);
        }
    };

    // noexcept版の特殊化
    template<class... Components>
    struct ECSProcessFunctionTraits<void (*)(size_t, const Entity*, Components*...) noexcept>
        : ECSProcessFunctionTraits<void (*)(size_t, const Entity*, Components*...)>
    {
    };

    /// @brief WorldがFunctionの重複登録を判定するための共通入口
    template<auto Function>
    inline constexpr ECSProcessInvoke ProcessKey =
        &ECSProcessFunctionTraits<decltype(Function)>::template Invoke<Function>;
}

#endif // _ECS_PROCESS_EXECUTION_H_
