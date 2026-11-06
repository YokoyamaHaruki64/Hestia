/*=============================================================================

 File   : ECSWorld.cpp
 Desc   : Entity配置、Process実行、構造変更を調停する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "ECS/ECSWorld.h"

#include <algorithm>
#include <cassert>
#include <new>
#include <string>
#include <type_traits>

#include "ECS/Component.h"
#include "ECS/Entity.h"
#include "Log/Logger.h"
#include "Transform/TransformData.h"

namespace Hestia
{
    namespace
    {
        // EntityHandleのindexは30bit、generationは32bitで表す
        constexpr uint32_t MAX_ENTITY_INDEX = 0x3fffffffu;

        // 構造変更中の状態を一時的に保持するためのスコープフラグ
        class ScopedFlag
        {
        public:
            explicit ScopedFlag(bool& flag) noexcept
                : m_flag(flag),
                  m_previous(flag)
            {
                m_flag = true;
            }

            ~ScopedFlag()
            {
                m_flag = m_previous;
            }

        private:
            bool& m_flag;
            bool m_previous;
        };

        // 構造変更中の状態を一時的に保持し、終了時に関数を呼び出すためのスコープクラス
        template<class Function>
        class ScopeExit
        {
        public:
            explicit ScopeExit(Function function) noexcept(
                std::is_nothrow_move_constructible_v<Function>)
                : m_function(std::move(function))
            {
                static_assert(std::is_nothrow_invocable_v<Function&>);
            }

            ~ScopeExit() noexcept
            {
                if (m_isActive)
                    m_function();
            }

            void Release() noexcept { m_isActive = false; }

        private:
            Function m_function;
            bool m_isActive = true;
        };

        template<class Function>
        ScopeExit(Function) -> ScopeExit<Function>;
    }

    ECSWorld::ECSWorld(const ComponentRegistry& registry, size_t chunkByteSize)
        : m_registry(&registry),
          m_chunkByteSize(chunkByteSize)
    {
    }

    ECSWorld::~ECSWorld()
    {
        Finalize();
    }

    bool ECSWorld::Initialize()
    {
        if (m_isInitialized || m_chunkByteSize == 0)
            return false;

        // Entity生成とComponent取り外し保護で共用する必須IDを集める
        m_nameType = m_registry->GetId<Name>();
        const std::optional<ComponentTypeId> required[] =
        {
            m_nameType,
            m_registry->GetId<Tag>(),
            m_registry->GetId<Layer>(),
            m_registry->GetId<Transform>(),
            m_registry->GetId<TransformCache>(),
            m_registry->GetId<TransformHierarchy>()
        };

        m_requiredTypes.clear();

        for (const auto id : required)
        {
            if (!id)
                return false;

            m_requiredTypes.push_back(*id);
        }

        // 取り外し時に必須かどうかを二分探索できるよう、ID順に揃える
        std::sort(m_requiredTypes.begin(), m_requiredTypes.end());

        // 必須型が揃ってからWorldの使用を許可する
        m_isInitialized = true;
        m_acceptsProcessRegistration = true;
        return true;
    }

    void ECSWorld::Finalize()
    {
        if (m_isRunningProcess || m_isApplyingChanges || m_isEndingFrame)
        {
            Logger::Error("ECSWorld::Finalize is unavailable in the current ECS state");
            return;
        }

        // Registry情報を参照するProcessとArchetypeを先に破棄する
        ScopedFlag applyingChanges(m_isApplyingChanges);
        m_isRunningProcess = false;
        m_isEndingFrame = false;
        m_pendingChanges.clear();
        m_pendingDestroy.clear();

        m_processIndices.clear();
        m_processes.clear();
        m_archetypes.clear();
        m_entitySlots.clear();
        m_freeIndices.clear();
        m_nameType.reset();
        m_requiredTypes.clear();
        m_isInitialized = false;
        m_acceptsProcessRegistration = false;
    }

    bool ECSWorld::CanRegisterProcess(ECSProcessInvoke key) const
    {
        // 初回の実行・Entity生成までは登録を許可し、同じ関数の重複も拒否する
        if (!m_acceptsProcessRegistration || key == nullptr ||
            m_isRunningProcess || m_isApplyingChanges || m_isEndingFrame)
        {
            Logger::Error("ECSWorld::RegisterProcess is unavailable in the current ECS state");
            return false;
        }

        if (m_processIndices.contains(key))
        {
            Logger::Error("Duplicate ECS Process registration");
            return false;
        }

        return true;
    }

    bool ECSWorld::RegisterProcessImpl(
        ECSProcessInvoke key,
        std::unique_ptr<ECSProcess> process)
    {
        // 登録完了までProcessをWorldのProcess一覧へ追加しない
        for (const auto& [signature, archetype] : m_archetypes)
        {
            if (process->MatchesArchetype(*archetype))
                process->RegisterArchetype(*archetype);
        }

        // Map登録後のProcess追加で再確保しないよう、両コンテナを先に準備する
        m_processes.reserve(m_processes.size() + 1);
        m_processIndices.reserve(m_processIndices.size() + 1);
        const size_t index = m_processes.size();
        if (!m_processIndices.emplace(key, index).second)
        {
            Logger::Error("ECS Process registration failed");
            return false;
        }

        m_processes.push_back(std::move(process));
        return true;
    }

    bool ECSWorld::ConfigureChunkSize(size_t chunkByteSize) noexcept
    {
        if (m_isInitialized || chunkByteSize == 0)
            return false;

        m_chunkByteSize = chunkByteSize;
        return true;
    }

    bool ECSWorld::ReserveSlot(EntityHandle& handle)
    {
        // Free Slotがあれば再利用し、なければ新しいSlotを追加する
        if (!m_freeIndices.empty())
        {
            const uint32_t index = m_freeIndices.back();
            m_freeIndices.pop_back();
            EntitySlot& slot = m_entitySlots[index];
            slot.m_state = EntitySlotState::PendingCreate;
            slot.m_archetype = nullptr;
            slot.m_chunk = nullptr;
            slot.m_row = 0;
            handle = { index, slot.m_generation };
            return true;
        }

        if (m_entitySlots.size() > MAX_ENTITY_INDEX)
            return false;

        // 予約Slotを後でnoexceptに返却できるよう、返却先の容量を先に確保する
        m_freeIndices.reserve(m_entitySlots.size() + 1);
        const uint32_t index = static_cast<uint32_t>(m_entitySlots.size());
        m_entitySlots.emplace_back();
        EntitySlot& slot = m_entitySlots.back();
        slot.m_generation = 1;
        slot.m_state = EntitySlotState::PendingCreate;
        handle = { index, slot.m_generation };
        return true;
    }

    void ECSWorld::ReleaseSlot(uint32_t index) noexcept
    {
        // generationを進めて旧Handleを無効化し、予約済みの空きSlot列へ返す
        assert(index < m_entitySlots.size());
        EntitySlot& slot = m_entitySlots[index];
        slot.m_state = EntitySlotState::Free;
        slot.m_archetype = nullptr;
        slot.m_chunk = nullptr;
        slot.m_row = 0;
        ++slot.m_generation;

        // generation 0は無効Handle用に予約する
        if (slot.m_generation == 0)
            slot.m_generation = 1;

        assert(m_freeIndices.size() < m_freeIndices.capacity());
        m_freeIndices.push_back(index);
    }

    EntitySlot* ECSWorld::FindSlot(EntityHandle handle) noexcept
    {
        // Handleの形式・index・generationとSlot状態を照合して特定する
        if (!handle.IsValid() || handle.m_index >= m_entitySlots.size())
            return nullptr;

        EntitySlot& slot = m_entitySlots[handle.m_index];
        if (slot.m_generation != handle.m_generation ||
            slot.m_state == EntitySlotState::Free)
            return nullptr;

        return &slot;
    }

    const EntitySlot* ECSWorld::FindSlot(EntityHandle handle) const noexcept
    {
        if (!handle.IsValid() || handle.m_index >= m_entitySlots.size())
            return nullptr;

        const EntitySlot& slot = m_entitySlots[handle.m_index];
        if (slot.m_generation != handle.m_generation ||
            slot.m_state == EntitySlotState::Free)
            return nullptr;

        return &slot;
    }

    EntitySlot* ECSWorld::FindAliveSlot(EntityHandle handle) noexcept
    {
        // PendingCreateにはまだEntity実体がないためAliveだけを返す
        EntitySlot* slot = FindSlot(handle);
        if (slot == nullptr || slot->m_state != EntitySlotState::Alive)
            return nullptr;

        return slot;
    }

    const EntitySlot* ECSWorld::FindAliveSlot(EntityHandle handle) const noexcept
    {
        const EntitySlot* slot = FindSlot(handle);
        if (slot == nullptr || slot->m_state != EntitySlotState::Alive)
            return nullptr;

        return slot;
    }

    EntityHandle ECSWorld::CreateEntity()
    {
        if (!m_isInitialized || m_isApplyingChanges || m_isEndingFrame)
        {
            Logger::Error("CreateEntity is unavailable in the current ECS state");
            return {};
        }

        // Entity生成以降はWorldの実行設定が始まるためProcess登録を閉じる
        m_acceptsProcessRegistration = false;

        if (m_isRunningProcess)
        {
            // Slot予約後にqueue確保で失敗してHandleだけ残る事態を避ける
            m_pendingChanges.reserve(m_pendingChanges.size() + 1);
        }

        // Handleを先に予約し、実行中なら生成を次の変更反映まで保留する
        EntityHandle handle;
        if (!ReserveSlot(handle))
        {
            Logger::Error("EntityHandle reservation failed");
            return {};
        }

        if (m_isRunningProcess)
        {
            m_pendingChanges.push_back({ handle, nullptr, ChangeKind::Create });
            return handle;
        }

        ApplyChange({ handle, nullptr, ChangeKind::Create });
        return FindAliveSlot(handle) != nullptr ? handle : EntityHandle{};
    }

    bool ECSWorld::ApplyCreate(EntityHandle handle)
    {
        EntitySlot* slot = FindSlot(handle);
        if (slot == nullptr || slot->m_state != EntitySlotState::PendingCreate)
            return false;

        const auto nameColumnId = m_nameType;
        if (!nameColumnId)
            return false;

        // 必須Componentだけで構成するArchetypeを取得する
        ArchetypeSignature signature;
        signature.m_types = m_requiredTypes;

        Archetype* archetype = GetOrCreateArchetype(std::move(signature));
        if (archetype == nullptr)
            return false;

        Chunk& chunk = archetype->GetOrCreateWritableChunk();
        const size_t row = chunk.GetEntityCount();

        if (chunk.ConstructEntity(row, handle, true) == nullptr)
            return false;

        // 必須列を構築して初期Nameを設定し、成功してからSlotとrowをAliveにする
        size_t constructedColumns = 0;
        ScopeExit rollback([&]() noexcept
        {
            while (constructedColumns > 0)
                chunk.DestroyComponent(--constructedColumns, row);

            chunk.DestroyEntity(row);
        });

        // 必須Componentは既定値で生成し、NameだけEntity indexから初期値を設定する
        for (; constructedColumns < archetype->GetLayout().m_components.size();
            ++constructedColumns)
            chunk.ConstructDefault(constructedColumns, row);

        const size_t nameColumn = archetype->FindColumn(*nameColumnId);
        Name* name = static_cast<Name*>(chunk.GetElement(nameColumn, row));
        name->m_value = "Entity" + std::to_string(handle.m_index);

        chunk.CommitRow(row);
        slot->m_state = EntitySlotState::Alive;
        slot->m_archetype = archetype;
        slot->m_chunk = &chunk;
        slot->m_row = row;
        rollback.Release();
        return true;
    }

    Entity* ECSWorld::GetEntity(EntityHandle handle) noexcept
    {
        if (m_isApplyingChanges)
            return nullptr;

        EntitySlot* slot = FindAliveSlot(handle);
        return slot == nullptr ? nullptr : slot->m_chunk->GetEntity(slot->m_row);
    }

    const Entity* ECSWorld::GetEntity(EntityHandle handle) const noexcept
    {
        if (m_isApplyingChanges)
            return nullptr;

        const EntitySlot* slot = FindAliveSlot(handle);
        return slot == nullptr ? nullptr : slot->m_chunk->GetEntity(slot->m_row);
    }

    void ECSWorld::AppendChange(ChangeRequest request)
    {
        // 要求保存に失敗した場合は例外を操作境界へ伝える
        m_pendingChanges.push_back(request);
    }

    void ECSWorld::DestroyEntity(EntityHandle handle)
    {
        if (!m_isInitialized || m_isApplyingChanges || m_isEndingFrame)
        {
            Logger::Error("DestroyEntity is unavailable in the current ECS state");
            return;
        }

        EntitySlot* slot = FindSlot(handle);
        if (slot == nullptr)
        {
            Logger::Error("DestroyEntity received an invalid EntityHandle");
            return;
        }

        // PendingCreateは生成要求を取消し、AliveはFrame末尾の破棄列へ送る
        if (slot->m_state == EntitySlotState::PendingCreate)
        {
            if (m_isRunningProcess)
                AppendChange({ handle, nullptr, ChangeKind::CancelCreate });

            if (!m_isRunningProcess)
                ReleaseSlot(handle.m_index);
            return;
        }

        Entity* entity = slot->m_chunk->GetEntity(slot->m_row);
        if (entity->IsDestroy())
            return;

        // 実体を辿れる間に、まだ破棄予約していない子孫を集める
        std::vector<EntityHandle> subtree{ handle };
        for (size_t index = 0; index < subtree.size(); ++index)
        {
            const auto* hierarchy = GetComponent<TransformHierarchy>(subtree[index]);
            for (EntityHandle child : hierarchy->m_children)
            {
                const Entity* childEntity = GetEntity(child);
                if (childEntity != nullptr && !childEntity->IsDestroy())
                    subtree.push_back(child);
            }
        }

        // 一部だけ予約される事態を避けるため、全件分のqueue容量を先に確保する
        const size_t requiredCapacity = m_pendingDestroy.size() + subtree.size();
        const size_t pendingCapacity = m_pendingDestroy.capacity();
        if (requiredCapacity > pendingCapacity)
            m_pendingDestroy.reserve(std::max(requiredCapacity, pendingCapacity + pendingCapacity / 2));

        // 集めたEntityを一括で予約し、実体の解放はEndFrameまで遅らせる
        for (EntityHandle descendant : subtree)
        {
            m_pendingDestroy.push_back(descendant);
            EntitySlot* descendantSlot = FindAliveSlot(descendant);
            descendantSlot->m_chunk->GetEntity(descendantSlot->m_row)->MarkDestroy();
        }
    }

    void ECSWorld::AddProcess(EntityHandle handle, ECSProcessInvoke process)
    {
        if (!m_isInitialized || m_isApplyingChanges || m_isEndingFrame)
        {
            Logger::Error("AddProcess is unavailable in the current ECS state");
            return;
        }

        if (!m_processIndices.contains(process))
        {
            Logger::Error("AddProcess references an unregistered Process");
            return;
        }

        EntitySlot* slot = FindSlot(handle);
        if (slot == nullptr || (slot->m_state == EntitySlotState::Alive &&
            slot->m_chunk->GetEntity(slot->m_row)->IsDestroy()))
        {
            Logger::Error("AddProcess received an invalid or destroy-reserved EntityHandle");
            return;
        }

        const ChangeRequest request{ handle, process, ChangeKind::AddProcess };
        if (m_isRunningProcess)
        {
            AppendChange(request);
            return;
        }

        ApplyChange(request);
    }

    void ECSWorld::RemoveProcess(EntityHandle handle, ECSProcessInvoke process)
    {
        if (!m_isInitialized || m_isApplyingChanges || m_isEndingFrame)
        {
            Logger::Error("RemoveProcess is unavailable in the current ECS state");
            return;
        }

        if (!m_processIndices.contains(process))
        {
            Logger::Error("RemoveProcess references an unregistered Process");
            return;
        }

        EntitySlot* slot = FindSlot(handle);
        if (slot == nullptr || (slot->m_state == EntitySlotState::Alive &&
            slot->m_chunk->GetEntity(slot->m_row)->IsDestroy()))
        {
            Logger::Error("RemoveProcess received an invalid or destroy-reserved EntityHandle");
            return;
        }

        const ChangeRequest request{ handle, process, ChangeKind::RemoveProcess };
        if (m_isRunningProcess)
        {
            AppendChange(request);
            return;
        }

        ApplyChange(request);
    }

    Archetype* ECSWorld::GetOrCreateArchetype(ArchetypeSignature signature)
    {
        // Signatureを正規化して、同じComponent構成のLayoutを共有する
        std::sort(signature.m_types.begin(), signature.m_types.end());
        signature.m_types.erase(std::unique(signature.m_types.begin(),
            signature.m_types.end()), signature.m_types.end());

        // すでに存在するArchetypeを返す
        const auto existing = m_archetypes.find(signature.m_types);
        if (existing != m_archetypes.end())
            return existing->second.get();

        // 新しいArchetypeの構築
        auto archetype = std::make_unique<Archetype>(
            *m_registry, std::move(signature), m_chunkByteSize);

        if (!archetype->IsValid())
            return nullptr;

        Archetype* result = archetype.get();

        // Processに新しいArchetypeを登録する
        try
        {
            for (const auto& process : m_processes)
            {
                if (process->MatchesArchetype(*result))
                    process->RegisterArchetype(*result);
            }

            // 構築済みArchetypeをMapに登録する。例外が発生した場合はProcess登録を巻き戻す
            auto it = m_archetypes.try_emplace(result->GetSignature().m_types).first;
            it->second = std::move(archetype);
        }
        catch (...)
        {
            for (const auto& process : m_processes)
                process->UnregisterArchetype(*result);

            Logger::Error("ECS Archetype registration failed");
            return nullptr;
        }
        return result;
    }

    void ECSWorld::ApplyChange(const ChangeRequest& request)
    {
        // queue反映時点でHandleと破棄状態を確認し、無効化された要求を捨てる
        ScopedFlag applyingChanges(m_isApplyingChanges);

        try
        {
            EntitySlot* slot = FindSlot(request.m_entity);

            // 新しいEntityの構築
            if (request.m_kind == ChangeKind::Create)
            {
                // 生成要求の反映に失敗した場合はSlotを解放して無効化する
                if (slot != nullptr &&
                    slot->m_state == EntitySlotState::PendingCreate &&
                    !ApplyCreate(request.m_entity))
                    ReleaseSlot(request.m_entity.m_index);
                return;
            }

            // FlushChangesの事前走査でSlotを解放済みのため、ここでは何もしない
            if (request.m_kind == ChangeKind::CancelCreate)
                return;

            // ↓構造変更系

            // Aliveかつ破棄予約済みのEntityは構造変更を受け付けない
            if (slot == nullptr || slot->m_state != EntitySlotState::Alive ||
                slot->m_chunk->GetEntity(slot->m_row)->IsDestroy())
                return;

            const auto processIt = m_processIndices.find(request.m_process);
            if (processIt == m_processIndices.end())
                return;

            // Processの要求に従ってArchetypeを移行する
            ApplyComponentChange(*slot, *m_processes[processIt->second],
                request.m_kind == ChangeKind::AddProcess);
        }
        catch (...)
        {
            // 例外が発生した場合は、Create要求ならSlotを解放して無効化する
            EntitySlot* slot = FindSlot(request.m_entity);
            if (request.m_kind == ChangeKind::Create && slot != nullptr &&
                slot->m_state == EntitySlotState::PendingCreate)
                ReleaseSlot(request.m_entity.m_index);

            Logger::Error("ECS change request failed");
        }
    }

    void ECSWorld::ApplyComponentChange(EntitySlot& slot,
        ECSProcess& process, bool add)
    {
        // 現在のSignatureへProcess要求を加減して移行先構成を作る
        const ArchetypeSignature& current = slot.m_archetype->GetSignature();
        ArchetypeSignature destination;
        destination.m_types = current.m_types;
        const auto requirements = process.GetComponents();

        if (add)
        {// 要求を一度すべて追加してから重複を除去する
            for (const ECSProcessComponent& component : requirements)
                destination.m_types.push_back(component.m_type);
        }
        else
        {
            // 現在のArchetypeに一致する他Processの要求Componentを集める
            std::unordered_set<ComponentTypeId> protectedTypes;
            for (const auto& other : m_processes)
            {
                if (other.get() == &process || !other->MatchesArchetype(*slot.m_archetype))
                    continue;

                for( const ECSProcessComponent& component : other->GetComponents())
                {
                    const ComponentTypeId type = component.m_type;
                    if (std::binary_search(m_requiredTypes.begin(), m_requiredTypes.end(), type))
                        continue;

                    protectedTypes.insert(type);
                }            
            }

            // 保護されないComponentは移行先から削除する
            auto requirements = process.GetComponents();
            for(auto& component : requirements)
            {
                const ComponentTypeId type = component.m_type;
                if (std::binary_search(m_requiredTypes.begin(), m_requiredTypes.end(), type))
                    continue;

                if (protectedTypes.contains(type))
                    continue;

                destination.m_types.erase(std::remove(destination.m_types.begin(),
                    destination.m_types.end(), type), destination.m_types.end());
            }
        }

        std::sort(destination.m_types.begin(), destination.m_types.end());

        // 重複するComponentを除去する
        destination.m_types.erase(std::unique(destination.m_types.begin(),
            destination.m_types.end()), destination.m_types.end());

        if (destination == current)
            return;

        Archetype* archetype = GetOrCreateArchetype(std::move(destination));
        if (archetype == nullptr || !MoveEntity(slot, *archetype))
            Logger::Error("ECS structure change preparation failed");
    }

    bool ECSWorld::MoveEntity(EntitySlot& slot, Archetype& destination)
    {
        // 移行元のArchetype、Chunk、rowを保持する
        Archetype* const sourceArchetype = slot.m_archetype;
        Chunk* const sourceChunk = slot.m_chunk;
        const size_t sourceRow = slot.m_row;
        const Entity* sourceEntity = sourceChunk->GetEntity(sourceRow);
        const auto& sourceColumns = sourceArchetype->GetLayout().m_components;

        // 移行先のChunkを取得し、移行先rowを確定する
        Chunk& destinationChunk = destination.GetOrCreateWritableChunk();
        const size_t destinationRow = destinationChunk.GetEntityCount();
        const auto& destinationColumns = destination.GetLayout().m_components;

        // 移行先rowにEntityを構築する。失敗した場合は移行元を変更せずに終了する
        if (destinationChunk.ConstructEntityCopy(destinationRow, *sourceEntity) == nullptr)
            return false;

        // 不足分の構築が成功するまで移行元とSlotを変更しない
        size_t processedColumns = 0;
        try
        {
            for (; processedColumns < destinationColumns.size(); ++processedColumns)
            {
                const ComponentTypeId destType = destinationColumns[processedColumns].m_type;

                // 移行元に存在しないComponentはデフォルト構築する
                if (sourceArchetype->FindColumn(destType) >= sourceColumns.size())
                    destinationChunk.ConstructDefault(processedColumns, destinationRow);
            }
        }
        catch (...)
        {
            // 失敗した列は未構築なので、それより前に構築できた不足分だけを破棄する
            while (processedColumns > 0)
            {
                const size_t column = --processedColumns;
                const ComponentTypeId destType = destinationColumns[column].m_type;

                if (sourceArchetype->FindColumn(destType) >= sourceColumns.size())
                    destinationChunk.DestroyComponent(column, destinationRow);
            }
            destinationChunk.DestroyEntity(destinationRow);
            return false;
        }

        // 以降のMoveと破棄はnoexceptなので、共通列を移してから移行元を片付ける
        for (size_t column = 0; column < sourceColumns.size(); ++column)
        {
            const ComponentTypeId type = sourceColumns[column].m_type;
            const size_t destinationColumn = destination.FindColumn(type);

            if (destinationColumn < destinationColumns.size())
                destinationChunk.MoveConstruct(destinationColumn, destinationRow,
                    *sourceChunk, column, sourceRow);

            sourceChunk->DestroyComponent(column, sourceRow);
        }

        // 移行完了後にSlotを更新し、元rowをSwap Removeする
        destinationChunk.CommitRow(destinationRow);
        slot.m_archetype = &destination;
        slot.m_chunk = &destinationChunk;
        slot.m_row = destinationRow;

        sourceChunk->DestroyEntity(sourceRow);
        RemoveRow(*sourceChunk, sourceRow);
        return true;
    }

    void ECSWorld::RemoveRow(Chunk& chunk, size_t row) noexcept
    {
        const size_t lastRow = chunk.GetEntityCount() - 1;
        if (row != lastRow)
        {
            // 行順は維持せず末尾rowで穴を埋め、移動したEntityのSlotを更新する
            const Entity* lastEntity = chunk.GetEntity(lastRow);
            const EntityHandle movedHandle = lastEntity->GetHandle();
            *chunk.GetEntity(row) = *lastEntity;    // 移動

            for (size_t column = 0; column < chunk.GetLayout().m_components.size(); ++column)
            {
                chunk.MoveConstruct(column, row, chunk, column, lastRow);
                chunk.DestroyComponent(column, lastRow);
            }

            chunk.DestroyEntity(lastRow);
            EntitySlot* movedSlot = FindAliveSlot(movedHandle);
            assert(movedSlot != nullptr);
            movedSlot->m_chunk = &chunk;
            movedSlot->m_row = row;
        }

        chunk.RemoveLastRow();
    }

    void ECSWorld::FlushChanges()
    {
        if (m_pendingChanges.empty())
            return;

        ScopedFlag applyingChanges(m_isApplyingChanges);
        std::vector<ChangeRequest> changes;
        // 現在の要求列を固定し、後続要求と混ざらない形で順に反映する
        changes.swap(m_pendingChanges);

        // 生成取消を先に確定し、同じ予約HandleのCreateを無効にする
        for (const ChangeRequest& request : changes)
        {
            if (request.m_kind != ChangeKind::CancelCreate)
                continue;

            EntitySlot* slot = FindSlot(request.m_entity);
            // CancelCreateはPendingCreateのみに有効で、AliveやFreeは無視する
            if (slot != nullptr && slot->m_state == EntitySlotState::PendingCreate)
                ReleaseSlot(request.m_entity.m_index);
        }

        for (const ChangeRequest& request : changes)
            ApplyChange(request);
    }

    void ECSWorld::FlushDestroy()
    {
        ScopedFlag applyingChanges(m_isApplyingChanges);

        // 破棄順やSwap Removeに依存しないよう、rowの解放前に親の子一覧から外す
        const ComponentTypeId hierarchyType = *m_registry->GetId<TransformHierarchy>();
        for (EntityHandle handle : m_pendingDestroy)
        {
            EntitySlot* slot = FindAliveSlot(handle);
            if (slot == nullptr)
                continue;

            const auto* hierarchy = static_cast<TransformHierarchy*>(slot->m_chunk->GetElement(
                slot->m_archetype->FindColumn(hierarchyType), slot->m_row));
            EntitySlot* parent = FindAliveSlot(hierarchy->m_parent);
            if (parent == nullptr)
                continue;

            auto* parentHierarchy = static_cast<TransformHierarchy*>(parent->m_chunk->GetElement(
                parent->m_archetype->FindColumn(hierarchyType), parent->m_row));
            auto& children = parentHierarchy->m_children;
            children.erase(std::remove(children.begin(), children.end(), handle), children.end());
        }

        // 下のfor文は軽量化対象

        // EndFrameで破棄予約を確定し、Component、row、Slotの順に解放する
        for (EntityHandle handle : m_pendingDestroy)
        {
            EntitySlot* slot = FindAliveSlot(handle);
            if (slot == nullptr)
                continue;

            Entity* entity = slot->m_chunk->GetEntity(slot->m_row);
            if (!entity->IsDestroy())
                continue;

            Chunk* chunk = slot->m_chunk;
            const size_t row = slot->m_row;

            // Componentの削除
            for (size_t column = 0; column < slot->m_archetype->GetLayout().m_components.size(); ++column)
                chunk->DestroyComponent(column, row);

            // Entityの削除
            chunk->DestroyEntity(row);

            // rowをSwap Removeして、削除したEntityのSlotを解放する
            RemoveRow(*chunk, row);
            ReleaseSlot(handle.m_index);
        }

        m_pendingDestroy.clear();
    }

    void ECSWorld::Run(ECSProcessPhase phase)
    {
        if (!m_isInitialized || m_isRunningProcess || m_isApplyingChanges || m_isEndingFrame)
        {
            Logger::Error("ECSWorld::Run is unavailable in the current ECS state");
            return;
        }

        // 何も一致しないphaseでも、Run開始後のProcess追加は認めない
        m_acceptsProcessRegistration = false;

        // 同phaseのProcessを登録順に走らせ、各Process全体の終了後に変更queueを反映する
        for (const auto& process : m_processes)
        {
            if (process->GetPhase() != phase)
                continue;

            bool processFailed = false;

            // 例外を記録し、保留要求の反映後に現在のRunを中断する
            try
            {
                ScopedFlag runningProcess(m_isRunningProcess);
                process->Run();
            }
            catch (...)
            {
                Logger::Error("ECS Process threw an exception");
                processFailed = true;
            }

            // Chunk間では構造を固定し、Process全体の終了後に要求を反映する
            FlushChanges();
            if (processFailed)
                return;
        }
    }

    void ECSWorld::EndFrame()
    {
        if (!m_isInitialized || m_isRunningProcess || m_isApplyingChanges || m_isEndingFrame)
        {
            Logger::Error("ECSWorld::EndFrame is unavailable in the current ECS state");
            return;
        }

        // Entityがない最初のフレームでも、実行開始後のProcess追加は認めない
        m_acceptsProcessRegistration = false;

        // 残った変更を先に適用し、その後に破棄予約済みEntityを実体から除く
        m_isEndingFrame = true;
        FlushChanges();
        FlushDestroy();
        m_isEndingFrame = false;
    }
}
