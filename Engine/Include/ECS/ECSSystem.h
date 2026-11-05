/*=============================================================================

 File   : ECSSystem.h
 Desc   : Engine内のECS初期化とWorld操作のstatic入口を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ECS_SYSTEM_H_
#define _ECS_SYSTEM_H_

#include <cstddef>
#include <optional>
#include <string_view>

#include "ECS/ComponentRegistry.h"
#include "ECS/ECSWorld.h"

namespace Hestia
{
    struct ECSSettings
    {
        size_t m_chunkByteSize = 16 * 1024; // 16KB
    };

    /// @brief Engineが所有するComponentRegistryと単一ECSWorldの入口。
    class ECSSystem
    {
    public:
        ECSSystem();
        ~ECSSystem();

        ECSSystem(const ECSSystem&) = delete;
        ECSSystem& operator=(const ECSSystem&) = delete;

        /// @brief 内部ECSを初期化し、標準Componentを登録する。
        /// @param settings Chunk容量などのECS設定
        /// @return 初期化成功時はtrue
        bool Initialize(const ECSSettings& settings = {});

        /// @brief WorldとRegistryを解放する。未初期化でも安全。
        void Finalize();

        /// @brief 初期化中にComponent型と決定的な名前を登録する。
        /// @param name Component IDの生成に使うUTF-8名
        /// @return 登録成功時はComponentTypeId、それ以外は空
        template<class T>
        static std::optional<ComponentTypeId> RegisterComponent(std::string_view name)
        {
            if (s_instance == nullptr || !s_instance->m_isInitializing)
                return std::nullopt;

            return s_instance->m_registry.Register<T>(name);
        }

        /// @brief Entityを作成または生成待ちとして予約する。
        /// @return 成功時はEntityHandle、失敗時は無効Handle
        static EntityHandle CreateEntity();

        /// @brief Entityを生成取消または破棄予約する。
        /// @param handle 対象EntityのHandle
        static void DestroyEntity(EntityHandle handle);

        /// @brief Handleに対応するEntityを借用する。
        /// @param handle 対象EntityのHandle
        /// @return 生存中Entityへの非所有ポインタ。無効Handle・生成待ちではnullptr
        /// @note 構造変更でポインタは無効になるため、長期保持にはHandleを使う
        static Entity* GetEntity(EntityHandle handle) noexcept;

        /// @brief Handleに対応するComponentを借用する。
        /// @param handle 対象EntityのHandle
        /// @return Componentへの非所有ポインタ。Entity無効またはComponent不在ではnullptr
        /// @note 構造変更でポインタは無効になるため、長期保持にはHandleを使う
        template<class T>
        static T* GetComponent(EntityHandle handle) noexcept
        {
            return s_instance == nullptr ? nullptr : s_instance->m_world.GetComponent<T>(handle);
        }

        /// @brief EntityにProcess要求Componentを追加する。
        template<auto Function>
        static void AddProcess(EntityHandle handle)
        {
            s_instance == nullptr ?
                Logger::Error("ECSSystem::AddProcess called while ECS is unavailable") :
                s_instance->m_world.AddProcess(handle, ProcessKey<Function>);
        }

        /// @brief Entityから他Processに不要なProcess要求Componentを削除する。
        template<auto Function>
        static void RemoveProcess(EntityHandle handle)
        {
            s_instance == nullptr ?
                Logger::Error("ECSSystem::RemoveProcess called while ECS is unavailable") :
                s_instance->m_world.RemoveProcess(handle, ProcessKey<Function>);
        }

        /// @brief 指定phaseのProcessを実行する。
        /// @param phase 実行するProcessのphase
        static void Run(ECSProcessPhase phase)
        {
            if (s_instance == nullptr)
                Logger::Error("ECSSystem::Run called while ECS is unavailable");
            else
                s_instance->RunImpl(phase);
        }

        /// @brief 破棄予約済みEntityを実際に破棄する。
        static void EndFrame()
        {
            if (s_instance == nullptr)
                Logger::Error("ECSSystem::EndFrame called while ECS is unavailable");
            else
                s_instance->EndFrameImpl();
        }

        /// @brief 指定phaseのProcessを実行する。
        void RunImpl(ECSProcessPhase phase);

        /// @brief 破棄予約済みEntityを実際に破棄する。
        void EndFrameImpl();

    private:
        /// @brief Initialize中に標準Componentを登録してWorldを初期化する
        bool RegisterECS();

        ComponentRegistry m_registry;
        ECSWorld m_world;
        bool m_isInitializing = false;
        bool m_isInitialized = false;

        static ECSSystem* s_instance;
    };
}

#endif // _ECS_SYSTEM_H_
