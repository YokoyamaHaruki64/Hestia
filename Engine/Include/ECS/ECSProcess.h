/*=============================================================================

 File   : ECSProcess.h
 Desc   : Component条件と対象ChunkへのProcess呼び出し情報を保持する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ECS_PROCESS_H_
#define _ECS_PROCESS_H_

#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ECS/ECSProcessExecution.h"
#include "Log/Logger.h"

namespace Hestia
{
    class Archetype;

    // @brief Archetypeと要求ComponentのArchetype内の列。要求順に格納される。
    struct ECSProcessArchetype
    {
        Archetype* m_archetype = nullptr;
        std::vector<size_t> m_columnIndices;
    };

    /// @brief ECSのSにおける処理単位。ArchetypeとChunkを横断してProcess関数を呼び出す。
    /// @note ECSProcessはWorldの所有で、ArchetypeはProcessの所有ではない
    class ECSProcess
    {
    public:
        ECSProcess() = default;
        ECSProcess(const ECSProcess&) = delete;
        ECSProcess& operator=(const ECSProcess&) = delete;
        ECSProcess(ECSProcess&&) noexcept = default;
        ECSProcess& operator=(ECSProcess&&) noexcept = default;

        /// @brief 関数型から条件・共通入口を作成する
        /// @return 作成成功時はProcess、要求Componentが未登録または重複する場合は空
        template<auto Function>
        static std::optional<ECSProcess> Create(const ComponentRegistry& registry,
            ECSProcessPhase phase)
        {
            // 要求するComponent型の判定用
            using Traits = ECSProcessFunctionTraits<decltype(Function)>;

            ECSProcess process;
            process.m_phase = phase;
            auto components = Traits::BuildComponents(registry);
            if (!components)
                return std::nullopt;
            process.m_components = std::move(*components);

            // 引数順を保ったまま、const違いも同じComponent IDとして重複を拒否する
            for (size_t i = 0; i < process.m_components.size(); ++i)
            {
                for (size_t j = 0; j < i; ++j)
                {
                    if (process.m_components[i].m_type == process.m_components[j].m_type)
                    {
                        Logger::Error("ECS Process has a duplicate Component requirement");
                        return std::nullopt;
                    }
                }
            }

            // 共通入口を登録する
            process.m_invoke = &Traits::template Invoke<Function>;
            return process;
        }

        /// @brief 登録された実行phaseを取得する。
        ECSProcessPhase GetPhase() const noexcept { return m_phase; }

        /// @brief World検索に使う共通入口を取得する。
        ECSProcessInvoke GetInvoke() const noexcept { return m_invoke; }

        /// @brief Component要求とアクセス種別を取得する。
        std::span<const ECSProcessComponent> GetComponents() const noexcept { return m_components; }

        /// @brief ArchetypeがProcessの要求Componentをすべて含むか確認する
        /// @return すべて含む: true / 不足がある: false
        bool MatchesArchetype(const Archetype& archetype) const noexcept;

        /// @brief 一致するArchetypeと列対応を登録する
        /// @pre MatchesArchetypeがtrueを返すArchetypeを渡す
        void RegisterArchetype(Archetype& archetype);

        /// @brief 指定Archetypeへの対応を取り消す。
        void UnregisterArchetype(const Archetype& archetype) noexcept;

        /// @brief 登録済みChunkを順次処理する。
        void Run();

    private:
        ECSProcessPhase m_phase = ECSProcessPhase::Update;  // 実行タイミング
        std::vector<ECSProcessComponent> m_components;      // Processが要求するComponentとアクセス種別
        std::vector<ECSProcessArchetype> m_archetypes;      // Processが実行対象とするArchetypeと列対応のキャッシュ
        ECSProcessInvoke m_invoke = nullptr;                // 実行関数の共通入口
    };

}

#endif // _ECS_PROCESS_H_
