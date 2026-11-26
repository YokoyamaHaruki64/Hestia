/*=============================================================================

 File   : World.h
 Desc   : Game 側に Entity、Script、Process Facade の操作を公開する。

------------------------------------------------------------------------------

 Date   : 2026/11/13
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _HESTIA_GAME_WORLD_H_
#define _HESTIA_GAME_WORLD_H_

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "GameRuntime.h"

namespace HestiaGame
{
    /// @brief 実行中の GameRuntime と Engine の Entity 操作への Facade。
    /// @note GameRuntime と同じスレッドで使用する。未接続時は操作に失敗する。
    class World
    {
    public:
        /// @brief Entity を生成する。失敗時は無効 Handle を返す。
        static Hestia::EntityHandle CreateEntity();

        /// @brief Prefab の階層を即時展開する。
        /// @return 成功時は生成したルート、失敗時は無効 Handle。
        static Hestia::EntityHandle Instantiate(Hestia::AssetHandle<Hestia::Prefab> prefab);

        /// @brief Entity とその子孫の破棄を予約する。
        static void DestroyEntity(Hestia::EntityHandle entity);

        /// @brief Entity が生存中で Active なら true を返す。
        static bool IsActive(Hestia::EntityHandle entity);

        /// @brief Active 設定を更新する。変更・同じ値ともに成功時は true。
        static bool SetActive(Hestia::EntityHandle entity, bool active);

        /// @brief UTF-8 の Entity 名を設定する。成功時は true。
        static bool SetName(Hestia::EntityHandle entity, std::string_view name);

        /// @brief 数値タグを設定する。成功時は true。
        static bool SetTag(Hestia::EntityHandle entity, std::uint32_t tag);

        /// @brief 生存中の Entity から名前が一致する一件を返す。一致なしは無効 Handle。
        static Hestia::EntityHandle FindFirstEntityByName(std::string_view name);

        /// @brief 生存中でタグが一致する全 Entity を返す。非 Active も対象とする。
        static std::vector<Hestia::EntityHandle> FindEntitiesByTag(std::uint32_t tag);

        /// @brief GameRuntime に具象 Script 型を登録する。成功時は true。
        /// @pre GameRuntime の初期化中の型登録工程で呼ぶ。T はコピー構築または noexcept なムーブ構築が可能で、コピーまたはムーブ代入が可能である。
        template<class T>
        static bool RegisterScript(Hestia::AssetGuid guid)
        {
            return s_runtime != nullptr && s_runtime->RegisterScript<T>(guid);
        }

        /// @brief 生存中の Entity に登録済み型の Script を追加する。失敗時は空の参照。
        /// @note 実行中に追加した Script は次のフレームから更新する。
        template<class T, class... Args>
        static ScriptRef<T> AddScript(Hestia::EntityHandle entity, Args&&... args)
        {
            return s_runtime != nullptr ? s_runtime->AddScript<T>(entity, std::forward<Args>(args)...) :
                ScriptRef<T>{};
        }

        /// @brief Entity 上の Script を参照する。該当なしは空の参照。
        template<class T>
        static ScriptRef<T> GetScript(Hestia::EntityHandle entity)
        {
            return s_runtime != nullptr ? s_runtime->GetScript<T>(entity) : ScriptRef<T>{};
        }

        /// @brief Script を削除予約し、参照を無効にする。予約できた場合は true。
        template<class T>
        static bool RemoveScript(Hestia::EntityHandle entity)
        {
            return s_runtime != nullptr && s_runtime->RemoveScript<T>(entity);
        }

        /// @brief Script 自身の有効設定を更新する。変更・同じ値ともに成功時は true。
        /// @note Entity の Active と組み合わせた有効状態が変わると OnEnable / OnDisable を呼ぶ。
        template<class T>
        static bool SetScriptEnabled(Hestia::EntityHandle entity, bool enabled)
        {
            return s_runtime != nullptr && s_runtime->SetScriptEnabled<T>(entity, enabled);
        }

        /// @brief T の Script を保持する生存中の Entity を返す。
        template<class T>
        static std::vector<Hestia::EntityHandle> FindEntitiesWithScript()
        {
            return s_runtime != nullptr ? s_runtime->FindEntitiesWithScript<T>() :
                std::vector<Hestia::EntityHandle>{};
        }

        /// @brief Entity への操作用 Facade を作る。状態は Facade の IsValid で確認する。
        template<class T>
        static T GetProcess(Hestia::EntityHandle entity)
        {
            const auto* binding = GetProcessBinding<T>();
            return T(entity, binding != nullptr ? &binding->m_api : nullptr);
        }

        /// @brief Facade に対応する Process の要求 Component を追加する。受付時は true。
        template<class T>
        static bool AddProcess(Hestia::EntityHandle entity)
        {
            const auto* binding = GetProcessBinding<T>();
            return binding != nullptr && AddProcess(binding->m_process, entity);
        }

        /// @brief Facade に対応する Process の条件を満たす生存中の Entity を返す。
        template<class T>
        static std::vector<Hestia::EntityHandle> FindEntitiesWithProcess()
        {
            const auto* binding = GetProcessBinding<T>();
            return binding != nullptr ? FindEntitiesWithProcess(binding->m_process) :
                std::vector<Hestia::EntityHandle>{};
        }

    private:
        friend class Detail::GameRuntime;

        template<class T>
        static const Hestia::ECSProcessBinding<typename T::API>* GetProcessBinding()
        {
            if (s_runtime == nullptr)
                return nullptr;

            const auto& binding = s_runtime->GetEngineAPI().m_ecsProcess->Get<typename T::API>();
            return binding.m_process.IsValid() ? &binding : nullptr;
        }

        static bool AddProcess(Hestia::ProcessHandle process, Hestia::EntityHandle entity);
        static std::vector<Hestia::EntityHandle> FindEntitiesWithProcess(Hestia::ProcessHandle process);
        static const Hestia::EntityAPI* GetEntityAPI() noexcept;

        inline static Detail::GameRuntime* s_runtime = nullptr;
    };
}

#endif // _HESTIA_GAME_WORLD_H_
