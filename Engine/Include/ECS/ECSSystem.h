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
#include <span>
#include <string_view>

#include "API/ECSProcessAPI.h"
#include "ECS/ComponentRegistry.h"
#include "ECS/ECSWorld.h"

#include "Common/Include/GameRuntimeAPI.h"

namespace Hestia
{
    struct Prefab;
    using ScriptGuidsByEntity = std::unordered_map<EntityHandle, std::vector<AssetGuid>, EntityHandleHash>;

    /// @brief SceneのEntity展開後に復元するScript型参照。
    struct SceneScriptAttachment
    {
        EntityHandle m_entity;
        AssetGuid m_guid;
    };
    struct ECSSettings
    {
        size_t m_chunkByteSize = 16 * 1024; // 16KB
    };

    /// @brief Engineが所有するComponentRegistryと単一ECSWorldの入口。
    class ECSSystem
    {
        friend class TransformProcess;

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

        /// @brief GameRuntimeが使用するEntity操作の関数テーブルを作成する。
        static EntityAPI CreateEntityAPI() noexcept;

        /// @brief GameRuntime の接続に使う型付き Process API を借用する。
        const ECSProcessAPI& GetProcessAPI() const noexcept { return m_processAPI; }

        /// @brief 実際のActive設定変更を通知する関数を接続する。
        /// @param context 通知関数の非所有コンテキスト
        /// @param callback 通知関数。nullptrを指定すると接続を解除する
        static void SetEntityActiveChangedCallback(void* context,
            void (*callback)(void*, EntityHandle, bool)) noexcept;

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

        /// @brief 必須型を含む指定構成でEntityを生成または予約する
        /// @param componentTypes 必須型を含むすべての登録済みComponent型ID
        /// @return 成功時はEntityHandle、失敗時は無効Handle
        static EntityHandle CreateEntity(std::span<const ComponentTypeId> componentTypes);

        /// @brief Entityを生成取消または破棄予約する。
        /// @param handle 対象EntityのHandle
        /// @note Transformの子孫も同時に破棄予約する。実体の解放はEndFrame
        static void DestroyEntity(EntityHandle handle);

        /// @brief Handleに対応するEntityを借用する。
        /// @param handle 対象EntityのHandle
        /// @return 生存中Entityへの非所有ポインタ。無効Handle・生成待ちではnullptr
        /// @note 構造変更でポインタは無効になるため、長期保持にはHandleを使う
        static Entity* GetEntity(EntityHandle handle) noexcept;

        /// @brief 生存中のEntityからComponent構成と保存フィールドを取得する
        /// @param handle 現在のWorld内の対象Entity
        /// @return 取得したスナップショット。対象が無効か参照できない場合は空
        static std::optional<SerializedEntity> SerializeEntity(EntityHandle handle);

        /// @brief ルートと子孫のスナップショットを取得する。失敗時は空
        static std::optional<SerializedEntity> SaveEntity(EntityHandle root);
        /// @brief 取得済みのScript型参照を使って階層を保存する。
        static std::optional<SerializedEntity> SaveEntity(EntityHandle root, const ScriptGuidsByEntity& scripts);
        /// @brief Engineが所有するGameRuntimeの照会を、利用期間だけ借用する。
        static void BindScriptAttachmentQuery(void* runtime, ScriptAttachmentQuery query) noexcept;
        /// @brief 対象Entityに付いている反映済みScriptのGUIDを取得する。
        static std::optional<ScriptGuidsByEntity> QueryScriptAttachments(std::span<const EntityHandle> entities);
        /// @brief 指定された全Componentを持つArchetypeを構造変更まで借用する。
        static std::vector<const Archetype*> QueryArchetypes(std::span<const ComponentTypeId> required);
        /// @brief Script終了処理を済ませ、登録を残したままWorldを空にする。
        static bool ClearSceneEntities(void* context, void (*clearScripts)(void*));
        /// @brief 全ルートを展開し、新しいHandleとScript型の対応を返す。
        static bool InstantiateScene(std::span<const SerializedEntity> roots,
            std::vector<SceneScriptAttachment>& scripts);

        /// @brief YAML入出力で参照する登録情報を借用する。ECS未初期化時はnullptr
        static const ComponentRegistry* GetRegistry() noexcept;

        /// @brief Prefabを即時展開し、ルートHandleを返す。失敗時は無効Handle
        static EntityHandle Instantiate(const Prefab& prefab);
        static EntityHandle Instantiate(AssetHandle<Prefab> handle);
        /// @brief 展開するルートのLocal TRSを指定する
        static EntityHandle Instantiate(const Prefab& prefab,
            const Vector3& position, const Quaternion& rotation, const Vector3& scale);
        static EntityHandle Instantiate(AssetHandle<Prefab> handle,
            const Vector3& position, const Quaternion& rotation, const Vector3& scale);

        /// @brief EntityのActive設定を変更し、変更した場合だけ通知する。
        /// @return 設定を受理した場合はtrue、操作できないEntityはfalse
        static bool SetActive(EntityHandle handle, bool active);

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
            if (s_instance == nullptr)
                Logger::Error("ECSSystem::AddProcess called while ECS is unavailable");
            else
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
        EntityHandle InstantiateImpl(const Prefab& prefab,
            const Vector3* position, const Quaternion* rotation, const Vector3* scale);
        EntityHandle InstantiateNode(const SerializedEntity& source, EntityHandle parent,
            const Vector3* position = nullptr, const Quaternion* rotation = nullptr, const Vector3* scale = nullptr,
            std::vector<SceneScriptAttachment>* sceneScripts = nullptr);
        EntityHandle FailPrefabNode(EntityHandle handle, std::string_view reason);
        /// @brief Initialize中に標準Componentを登録してWorldを初期化する
        bool RegisterECS();

        template<class API, auto Function>
        bool RegisterGameProcess(ECSProcessPhase phase)
        {
            auto& binding = m_processAPI.Get<API>();
            if (binding.m_process.IsValid() || !m_world.RegisterProcess<Function>(phase))
                return false;

            // ECS の登録が成功してから、追加・検索に使うインデックスを公開する
            binding.m_process = { m_world.m_processes.size() - 1 };
            return true;
        }

        ECSProcessInvoke ResolveProcess(ProcessHandle process) const noexcept;

        ComponentRegistry m_registry;
        ECSWorld m_world;
        void* m_scriptRuntime = nullptr;
        ScriptAttachmentQuery m_scriptQuery = nullptr;
        ECSProcessAPI m_processAPI;
        bool m_isInitializing = false;
        bool m_isInitialized = false;

        static ECSSystem* s_instance;
    };
}

#endif // _ECS_SYSTEM_H_
