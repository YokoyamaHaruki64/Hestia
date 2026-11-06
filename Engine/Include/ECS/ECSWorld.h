/*=============================================================================

 File   : ECSWorld.h
 Desc   : Entity、Archetype、Processと構造変更を調停する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ECS_WORLD_H_
#define _ECS_WORLD_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ECS/Archetype.h"
#include "ECS/ECSProcess.h"
#include "ECS/ECSProcessExecution.h"

namespace Hestia
{
    class ECSSystem;

    /// @brief 一つのComponentRegistryに属し、Entity・Archetype・Processを所有するECS World。
    /// @note Registryは非所有参照のため、Worldより長く存続する
    class ECSWorld
    {
        friend class ECSSystem;

        // 構造変更要求の種類
        enum class ChangeKind
        {
            Create,
            AddProcess,
            RemoveProcess,
            CancelCreate,
        };

        // 構造変更要求の内容
        struct ChangeRequest
        {
            EntityHandle m_entity;
            ECSProcessInvoke m_process = nullptr;
            ChangeKind m_kind = ChangeKind::Create;
        };

    public:
        explicit ECSWorld(const ComponentRegistry& registry,size_t chunkByteSize = 16 * 1024);
        ~ECSWorld();

        ECSWorld(const ECSWorld&) = delete;
        ECSWorld& operator=(const ECSWorld&) = delete;

        /// @brief 必須Componentを接続し、Entity操作を利用可能にする。
        bool Initialize();

        /// @brief Process参照、Chunk、Registry依存を順に解放する。
        void Finalize();

        /// @brief World初期化後、実行開始前にProcessを登録する。
        /// @note 同じFunctionはphaseに関係なく一度だけ登録でき、同phase内は登録順に実行する
        template<auto Function>
        bool RegisterProcess(ECSProcessPhase phase)
        {
            const ECSProcessInvoke key = ProcessKey<Function>;
            if (!CanRegisterProcess(key))
                return false;

            try
            {
                auto created = ECSProcess::Create<Function>(*m_registry, phase);
                if (!created)
                {
                    Logger::Error("ECS Process registration failed: invalid or duplicate Component requirements");
                    return false;
                }

                auto process = std::make_unique<ECSProcess>(std::move(*created));
                return RegisterProcessImpl(key, std::move(process));
            }
            catch (...)
            {
                Logger::Error("ECS Process registration failed");
                return false;
            }
        }

        /// @brief 新しいHandleを予約してEntityを作成する。
        /// @note Process実行中は生成を保留し、反映まではGetEntityとGetComponentがnullptrを返す
        /// @return 成功時はEntityHandle、失敗時は無効Handle
        EntityHandle CreateEntity();

        /// @brief Entityを生成取消、または破棄予約する。
        /// @param handle 対象EntityのHandle
        /// @note 生成待ちは取消し、生存中のEntityと子孫は破棄予約してEndFrameまで実体を保持する
        void DestroyEntity(EntityHandle handle);

        /// @brief Handleの現在位置からEntityを取得する。
        /// @param handle 対象EntityのHandle
        /// @return 生存中Entityへの非所有ポインタ。無効Handle・生成待ち・構造変更中はnullptr
        /// @note 返すポインタは構造変更で無効になるため、長期保持にはHandleを使う
        Entity* GetEntity(EntityHandle handle) noexcept;
        const Entity* GetEntity(EntityHandle handle) const noexcept;

        /// @brief Componentへの非所有ポインタを取得する。
        /// @param handle 対象EntityのHandle
        /// @return Componentへの非所有ポインタ。無効Handle・生成待ち・Component不在・構造変更中はnullptr
        /// @note 返すポインタは構造変更で無効になるため、長期保持にはHandleを使う
        template<class T>
        T* GetComponent(EntityHandle handle) noexcept
        {
            if (m_isApplyingChanges)
                return nullptr;

            using Component = std::remove_const_t<T>;
            const auto id = m_registry->GetId<Component>();
            EntitySlot* slot = FindAliveSlot(handle);
            if (!id || slot == nullptr)
                return nullptr;

            const size_t column = slot->m_archetype->FindColumn(*id);
            if (column >= slot->m_archetype->GetLayout().m_components.size())
                return nullptr;

            return static_cast<T*>(slot->m_chunk->GetElement(column, slot->m_row));
        }

        /// @brief Processが要求するComponent構成を追加する。
        /// @param handle 対象EntityのHandle
        /// @param process 登録済みProcessの共通入口
        /// @note 既存値を維持し、Process実行中は現在のProcess完了後に反映する
        void AddProcess(EntityHandle handle, ECSProcessInvoke process);

        /// @brief Process固有Componentを構成から除く。
        /// @param handle 対象EntityのHandle
        /// @param process 登録済みProcessの共通入口
        /// @note 他の一致Processと必須Componentが要求する型は残す
        void RemoveProcess(EntityHandle handle, ECSProcessInvoke process);

        /// @brief 指定phaseのProcessを登録順に実行する。
        /// @param phase 実行するProcess phase
        /// @note Process中の構造変更要求は、そのProcessの全Chunk実行後に反映する
        /// Processが例外を送出した場合は記録後にRunを中断する
        void Run(ECSProcessPhase phase);

        /// @brief 破棄予約済みEntityを実際に削除する。
        /// @note Entityの実体とComponentはこの呼び出しまで維持する
        void EndFrame();

    private:

        bool CanRegisterProcess(ECSProcessInvoke key) const;
        bool RegisterProcessImpl(ECSProcessInvoke key,
            std::unique_ptr<ECSProcess> process);

        bool ConfigureChunkSize(size_t chunkByteSize) noexcept;
        bool ReserveSlot(EntityHandle& handle);     // 新しいHandleを予約してSlotを確保する
        void ReleaseSlot(uint32_t index) noexcept;  // Slotを解放してHandleを無効化する

        EntitySlot* FindAliveSlot(EntityHandle handle) noexcept;                // AliveのSlotを返す。破棄予約済みも実体解放までは取得可能
        const EntitySlot* FindAliveSlot(EntityHandle handle) const noexcept;    // 無効Handle・生成待ちはnullptr

        EntitySlot* FindSlot(EntityHandle handle) noexcept;                // 指定Handleに対応するSlotを返す。無効Handleはnullptr
        const EntitySlot* FindSlot(EntityHandle handle) const noexcept;    // 指定Handleに対応するSlotを返す。無効Handleはnullptr

        Archetype* GetOrCreateArchetype(ArchetypeSignature signature);     // Component型IDの集合をキーにArchetypeを検索し、未登録なら新規作成する

        bool ApplyCreate(EntityHandle handle);                              // 生成待ちのEntityをChunkに構築して生存状態にする

        // 保留中の構造変更Requestを反映する
        // AddProcessまたはRemoveProcessの構造変更を反映する
        void ApplyChange(const ChangeRequest& request); 

        // ApplyChangeのComponent追加・削除処理
        void ApplyComponentChange(
            EntitySlot& slot,
            ECSProcess& process,
            bool add);

        bool MoveEntity(EntitySlot& slot, Archetype& destination);
        void RemoveRow(Archetype& archetype, Chunk& chunk, size_t row) noexcept;
        void FlushChanges();
        void FlushDestroy();
        void AppendChange(ChangeRequest request);

        const ComponentRegistry* m_registry = nullptr;
        size_t m_chunkByteSize = 16 * 1024;
        std::optional<ComponentTypeId> m_nameType;
        std::vector<ComponentTypeId> m_requiredTypes;

        std::vector<EntitySlot> m_entitySlots;  // EntitySlot
        std::vector<uint32_t> m_freeIndices;    // 空きSlotのindex

        // Component型IDの正規化済み集合をキーにArchetypeを所有する
        std::map<std::vector<ComponentTypeId>, std::unique_ptr<Archetype>> m_archetypes;

        std::vector<std::unique_ptr<ECSProcess>> m_processes;// Processの所有
        std::unordered_map<ECSProcessInvoke, size_t> m_processIndices;  // Process共通入口からm_processesのindexを検索する

        std::vector<ChangeRequest> m_pendingChanges;    // 保留中の構造変更Request
        std::vector<EntityHandle> m_pendingDestroy;     // 保留中の破棄予約済みEntity

        bool m_isInitialized = false;
        bool m_acceptsProcessRegistration = false;
        bool m_isRunningProcess = false;
        bool m_isApplyingChanges = false;
        bool m_isEndingFrame = false;
    };
}

#endif // _ECS_WORLD_H_
