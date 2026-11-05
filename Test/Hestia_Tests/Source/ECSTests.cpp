/*=============================================================================

 File   : ECSTests.cpp
 Desc   : Archetype ECSの登録、Entity、Process契約を検証する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "ECS/Component.h"
#include "ECS/ComponentRegistry.h"
#include "ECS/ECSSystem.h"
#include "ECS/ECSWorld.h"

namespace
{
    struct Position
    {
        int m_value = 0;
        static int s_moveCount;

        Position() noexcept = default;
        Position(Position&& other) noexcept : m_value(other.m_value)
        {
            ++s_moveCount;
        }
        ~Position() noexcept = default;
    };

    int Position::s_moveCount = 0;

    struct Velocity
    {
        int m_value = 0;
    };

    struct alignas(64) OverAligned
    {
        std::byte m_values[64]{};
    };

    struct ThrowOnConstruction
    {
        static bool s_shouldThrow;

        ThrowOnConstruction()
        {
            if (s_shouldThrow)
                throw std::runtime_error("component construction failure");
        }
        ThrowOnConstruction(ThrowOnConstruction&&) noexcept = default;
        ~ThrowOnConstruction() noexcept = default;
    };

    bool ThrowOnConstruction::s_shouldThrow = false;

    std::vector<int> g_processOrder;
    size_t g_seenEntities = 0;
    size_t g_chunkCalls = 0;
    size_t g_velocityCalls = 0;
    size_t g_alignedEntities = 0;
    size_t g_mutationCallbackCount = 0;
    Hestia::ECSWorld* g_callbackWorld = nullptr;
    Hestia::EntityHandle g_callbackCreated{};
    bool g_callbackObservedPending = false;
    bool g_callbackObservedComponentPending = false;
    bool g_callbackMutationDone = false;
    bool g_throwFromProcess = false;

    void VisitPosition(size_t count, const Hestia::Entity*, Position* positions)
    {
        ++g_seenEntities;
        for (size_t index = 0; index < count; ++index)
            ++positions[index].m_value;
    }

    void VisitPair(size_t count, const Hestia::Entity*,
        Position* positions, const Velocity* velocities)
    {
        ++g_chunkCalls;
        for (size_t index = 0; index < count; ++index)
        {
            if (positions[index].m_value == velocities[index].m_value)
                ++g_seenEntities;
        }
    }

    void VisitVelocity(size_t count, const Hestia::Entity*, Velocity*)
    {
        g_velocityCalls += count;
    }

    void ApplyVelocityNoexcept(size_t count, const Hestia::Entity*,
        Position* positions, const Velocity* velocities) noexcept
    {
        for (size_t index = 0; index < count; ++index)
            positions[index].m_value += velocities[index].m_value;
    }

    void VisitDuplicatePosition(size_t, const Hestia::Entity*, Position*, Position*)
    {
    }

    void VisitPositionWithConstAlias(size_t, const Hestia::Entity*,
        Position*, const Position*)
    {
    }

    void VisitOverAligned(size_t count, const Hestia::Entity*, OverAligned*)
    {
        g_alignedEntities += count;
    }

    void VisitPositionAndThrow(size_t, const Hestia::Entity*,
        Position*, ThrowOnConstruction*)
    {
    }

    void VisitPositionFirst(size_t, const Hestia::Entity*, Position*)
    {
        g_processOrder.push_back(1);
    }

    void VisitPositionSecond(size_t, const Hestia::Entity*, Position*)
    {
        g_processOrder.push_back(2);
    }

    void CreateDuringProcess(size_t, const Hestia::Entity*, Position*)
    {
        ++g_mutationCallbackCount;
        if (g_callbackMutationDone)
            return;

        g_callbackMutationDone = true;
        g_callbackCreated = g_callbackWorld->CreateEntity();
        g_callbackWorld->AddProcess(g_callbackCreated,
            Hestia::ProcessKey<&VisitPosition>);
        g_callbackObservedPending = g_callbackWorld->GetEntity(g_callbackCreated) == nullptr;
        g_callbackObservedComponentPending =
            g_callbackWorld->GetComponent<Position>(g_callbackCreated) == nullptr;
    }

    void CancelPendingDuringProcess(size_t, const Hestia::Entity*, Position*)
    {
        if (g_callbackMutationDone)
            return;

        g_callbackMutationDone = true;
        g_callbackCreated = g_callbackWorld->CreateEntity();
        g_callbackWorld->AddProcess(g_callbackCreated,
            Hestia::ProcessKey<&VisitPosition>);
        g_callbackWorld->DestroyEntity(g_callbackCreated);
    }

    void AddVelocityThenDestroy(size_t count, const Hestia::Entity* entities, Position*)
    {
        if (count == 0 || g_callbackMutationDone)
            return;

        g_callbackMutationDone = true;
        const Hestia::EntityHandle handle = entities[0].GetHandle();
        g_callbackWorld->AddProcess(handle, Hestia::ProcessKey<&VisitVelocity>);
        g_callbackWorld->DestroyEntity(handle);
    }

    void ThrowFromProcess(size_t, const Hestia::Entity*, Position*)
    {
        if (g_throwFromProcess)
            throw std::runtime_error("process failure");
    }

    void ContinueAfterThrow(size_t, const Hestia::Entity*, Position*)
    {
        g_processOrder.push_back(3);
    }

    class ECSFixture : public testing::Test
    {
    protected:
        ECSFixture() : m_world(m_registry, 256)
        {
        }

        void SetUp() override
        {
            ASSERT_TRUE(m_registry.Register<Hestia::Name>("Hestia::Name"));
            ASSERT_TRUE(m_registry.Register<Hestia::Tag>("Hestia::Tag"));
            ASSERT_TRUE(m_registry.Register<Hestia::Layer>("Hestia::Layer"));
            ASSERT_TRUE(m_registry.Register<Position>("Test::Position"));
            ASSERT_TRUE(m_registry.Register<Velocity>("Test::Velocity"));
            ASSERT_TRUE(m_registry.Register<OverAligned>("Test::OverAligned"));
            ASSERT_TRUE(m_registry.Register<ThrowOnConstruction>(
                "Test::ThrowOnConstruction"));
            ASSERT_TRUE(m_world.Initialize());

            Position::s_moveCount = 0;
            ThrowOnConstruction::s_shouldThrow = false;
            g_processOrder.clear();
            g_seenEntities = 0;
            g_chunkCalls = 0;
            g_velocityCalls = 0;
            g_alignedEntities = 0;
            g_mutationCallbackCount = 0;
            g_callbackWorld = &m_world;
            g_callbackCreated = {};
            g_callbackObservedPending = false;
            g_callbackObservedComponentPending = false;
            g_callbackMutationDone = false;
            g_throwFromProcess = false;
        }

        void TearDown() override
        {
            m_world.Finalize();
            g_callbackWorld = nullptr;
        }

        Hestia::ComponentRegistry m_registry;
        Hestia::ECSWorld m_world;
    };

    TEST(ComponentRegistry, UsesStableNamesAndRejectsConflictingRegistrations)
    {
        Hestia::ComponentRegistry registry;
        const auto first = registry.Register<Position>("Test::Position");
        ASSERT_TRUE(first);
        EXPECT_EQ(*first, 0x6583654b550cc61aull);
        EXPECT_EQ(registry.Register<Position>("Test::Position"), first);
        EXPECT_FALSE(registry.Register<Position>("Test::RenamedPosition"));
        EXPECT_FALSE(registry.Register<Velocity>("Test::Position"));
        EXPECT_FALSE(registry.GetId<Velocity>());

        Hestia::ComponentRegistry secondRegistry;
        EXPECT_EQ(secondRegistry.Register<Position>("Test::Position"), first);
    }

    TEST(ECSSystem, InitializesDefaultsAndCanFinalizeMoreThanOnce)
    {
        Hestia::ECSSystem system;
        EXPECT_FALSE(system.Initialize({ 0 }));
        ASSERT_TRUE(system.Initialize());

        const Hestia::EntityHandle handle = Hestia::ECSSystem::CreateEntity();
        ASSERT_TRUE(handle.IsValid());
        Hestia::Entity* entity = Hestia::ECSSystem::GetEntity(handle);
        ASSERT_NE(entity, nullptr);
        EXPECT_TRUE(entity->IsActive());
        EXPECT_FALSE(entity->IsDestroy());
        EXPECT_EQ(Hestia::ECSSystem::GetComponent<Hestia::Name>(handle)->m_value,
            "Entity0");
        EXPECT_EQ(Hestia::ECSSystem::GetComponent<Hestia::Tag>(handle)->m_value, 0u);
        EXPECT_EQ(Hestia::ECSSystem::GetComponent<Hestia::Layer>(handle)->m_value, 0u);
        EXPECT_FALSE(Hestia::ECSSystem::RegisterComponent<Position>("Test::Position"));

        system.Finalize();
        system.Finalize();
        EXPECT_EQ(Hestia::ECSSystem::GetEntity(handle), nullptr);
    }

    TEST_F(ECSFixture, ProcessUsesArgumentOrderAndRunsAcrossChunks)
    {
        ASSERT_TRUE(m_world.RegisterProcess<&VisitPair>(Hestia::ECSProcessPhase::Update));
        ASSERT_TRUE(m_world.RegisterProcess<&VisitOverAligned>(Hestia::ECSProcessPhase::Update));

        for (int value = 0; value < 12; ++value)
        {
            const auto handle = m_world.CreateEntity();
            ASSERT_TRUE(handle.IsValid());
            m_world.AddProcess(handle, Hestia::ProcessKey<&VisitPosition>);
            m_world.AddProcess(handle, Hestia::ProcessKey<&VisitPair>);
            m_world.AddProcess(handle, Hestia::ProcessKey<&VisitOverAligned>);
            m_world.GetComponent<Position>(handle)->m_value = value;
            m_world.GetComponent<Velocity>(handle)->m_value = value;
        }

        m_world.Run(Hestia::ECSProcessPhase::Update);
        EXPECT_EQ(g_seenEntities, 12u);
        EXPECT_GT(g_chunkCalls, 1u);
        EXPECT_EQ(g_alignedEntities, 12u);
    }

    TEST_F(ECSFixture, ProcessTraitsPreserveArgumentOrderAndAccess)
    {
        const auto process = Hestia::ECSProcess::Create<&VisitPair>(
            m_registry, Hestia::ECSProcessPhase::Update);
        ASSERT_TRUE(process);
        const auto components = process->GetComponents();
        ASSERT_EQ(components.size(), 2u);
        EXPECT_EQ(components[0].m_type, *m_registry.GetId<Position>());
        EXPECT_EQ(components[0].m_access, Hestia::ECSComponentAccess::Write);
        EXPECT_EQ(components[1].m_type, *m_registry.GetId<Velocity>());
        EXPECT_EQ(components[1].m_access, Hestia::ECSComponentAccess::Read);
    }

    TEST_F(ECSFixture, ProcessInvokesOrdinaryAndNoexceptFunctions)
    {
        Hestia::ArchetypeSignature signature;
        signature.m_types = {
            *m_registry.GetId<Position>(), *m_registry.GetId<Velocity>() };
        Hestia::Archetype archetype(m_registry, signature, 256);
        Hestia::Chunk& chunk = archetype.GetOrCreateWritableChunk();
        ASSERT_GE(chunk.GetCapacity(), 2u);
        const size_t positionColumn = archetype.FindColumn(*m_registry.GetId<Position>());
        const size_t velocityColumn = archetype.FindColumn(*m_registry.GetId<Velocity>());
        for (size_t row = 0; row < 2; ++row)
        {
            chunk.ConstructEntity(row, { static_cast<uint32_t>(row), 1 }, true);
            for (size_t column = 0; column < archetype.GetLayout().m_components.size(); ++column)
                chunk.ConstructDefault(column, row);
            chunk.CommitRow(row);
        }

        auto* positions = static_cast<Position*>(chunk.GetElement(positionColumn, 0));
        auto* velocities = static_cast<Velocity*>(chunk.GetElement(velocityColumn, 0));
        positions[0].m_value = 10;
        positions[1].m_value = 20;
        velocities[0].m_value = 3;
        velocities[1].m_value = 5;

        auto ordinary = Hestia::ECSProcess::Create<&VisitPosition>(
            m_registry, Hestia::ECSProcessPhase::Update);
        auto noThrow = Hestia::ECSProcess::Create<&ApplyVelocityNoexcept>(
            m_registry, Hestia::ECSProcessPhase::Update);
        ASSERT_TRUE(ordinary);
        ASSERT_TRUE(noThrow);
        ASSERT_TRUE(ordinary->MatchesArchetype(archetype));
        ASSERT_TRUE(noThrow->MatchesArchetype(archetype));
        ordinary->RegisterArchetype(archetype);
        noThrow->RegisterArchetype(archetype);
        EXPECT_EQ(noThrow->GetInvoke(), Hestia::ProcessKey<&ApplyVelocityNoexcept>);

        ordinary->Run();
        EXPECT_EQ(positions[0].m_value, 11);
        EXPECT_EQ(positions[1].m_value, 21);
        noThrow->Run();
        EXPECT_EQ(positions[0].m_value, 14);
        EXPECT_EQ(positions[1].m_value, 26);
        EXPECT_EQ(velocities[0].m_value, 3);
        EXPECT_EQ(velocities[1].m_value, 5);
    }

    TEST_F(ECSFixture, ProcessCreationRejectsDuplicateComponent)
    {
        EXPECT_FALSE(Hestia::ECSProcess::Create<&VisitDuplicatePosition>(
            m_registry, Hestia::ECSProcessPhase::Update));
    }

    TEST_F(ECSFixture, ProcessCreationRejectsConstAliasOfSameComponent)
    {
        EXPECT_FALSE(Hestia::ECSProcess::Create<&VisitPositionWithConstAlias>(
            m_registry, Hestia::ECSProcessPhase::Update));
    }

    TEST_F(ECSFixture, ProcessRegistrationRejectsDuplicateFunctionAcrossPhases)
    {
        ASSERT_TRUE(m_world.RegisterProcess<&VisitPosition>(Hestia::ECSProcessPhase::Update));
        EXPECT_FALSE(m_world.RegisterProcess<&VisitPosition>(Hestia::ECSProcessPhase::LateUpdate));
    }

    TEST_F(ECSFixture, SamePhaseProcessesRunInRegistrationOrder)
    {
        ASSERT_TRUE(m_world.RegisterProcess<&VisitPositionFirst>(Hestia::ECSProcessPhase::Update));
        ASSERT_TRUE(m_world.RegisterProcess<&VisitPositionSecond>(Hestia::ECSProcessPhase::Update));
        const auto handle = m_world.CreateEntity();
        m_world.AddProcess(handle, Hestia::ProcessKey<&VisitPositionFirst>);
        m_world.AddProcess(handle, Hestia::ProcessKey<&VisitPositionSecond>);

        m_world.Run(Hestia::ECSProcessPhase::Update);
        EXPECT_EQ(g_processOrder, (std::vector<int>{ 1, 2 }));
    }

    TEST_F(ECSFixture, AddPreservesExistingValuesAndRemoveProtectsSharedRequirements)
    {
        ASSERT_TRUE(m_world.RegisterProcess<&VisitPosition>(Hestia::ECSProcessPhase::Update));
        ASSERT_TRUE(m_world.RegisterProcess<&VisitPair>(Hestia::ECSProcessPhase::Update));
        const auto handle = m_world.CreateEntity();
        m_world.AddProcess(handle, Hestia::ProcessKey<&VisitPosition>);
        m_world.GetComponent<Position>(handle)->m_value = 42;

        m_world.AddProcess(handle, Hestia::ProcessKey<&VisitPair>);
        ASSERT_NE(m_world.GetComponent<Position>(handle), nullptr);
        EXPECT_EQ(m_world.GetComponent<Position>(handle)->m_value, 42);
        ASSERT_NE(m_world.GetComponent<Velocity>(handle), nullptr);

        m_world.RemoveProcess(handle, Hestia::ProcessKey<&VisitPosition>);
        EXPECT_NE(m_world.GetComponent<Position>(handle), nullptr);

        m_world.RemoveProcess(handle, Hestia::ProcessKey<&VisitPair>);
        EXPECT_NE(m_world.GetComponent<Position>(handle), nullptr);
        EXPECT_EQ(m_world.GetComponent<Velocity>(handle), nullptr);
    }

    TEST_F(ECSFixture, ConstructionFailureLeavesSourceEntityUnchanged)
    {
        ASSERT_TRUE(m_world.RegisterProcess<&VisitPosition>(Hestia::ECSProcessPhase::Update));
        ASSERT_TRUE(m_world.RegisterProcess<&VisitPositionAndThrow>(
            Hestia::ECSProcessPhase::LateUpdate));
        const auto handle = m_world.CreateEntity();
        m_world.AddProcess(handle, Hestia::ProcessKey<&VisitPosition>);
        m_world.GetComponent<Position>(handle)->m_value = 77;
        const int movesBeforeFailure = Position::s_moveCount;

        ThrowOnConstruction::s_shouldThrow = true;
        m_world.AddProcess(handle, Hestia::ProcessKey<&VisitPositionAndThrow>);
        ThrowOnConstruction::s_shouldThrow = false;

        EXPECT_EQ(m_world.GetComponent<Position>(handle)->m_value, 77);
        EXPECT_EQ(m_world.GetComponent<ThrowOnConstruction>(handle), nullptr);
        EXPECT_EQ(Position::s_moveCount, movesBeforeFailure);
    }

    TEST_F(ECSFixture, ProcessChangesAreDeferredUntilTheWholeProcessReturns)
    {
        ASSERT_TRUE(m_world.RegisterProcess<&CreateDuringProcess>(Hestia::ECSProcessPhase::Update));
        ASSERT_TRUE(m_world.RegisterProcess<&VisitPosition>(Hestia::ECSProcessPhase::Update));
        Hestia::ArchetypeSignature signature;
        signature.m_types = {
            *m_registry.GetId<Hestia::Name>(),
            *m_registry.GetId<Hestia::Tag>(),
            *m_registry.GetId<Hestia::Layer>(),
            *m_registry.GetId<Position>() };
        const Hestia::Archetype layout(m_registry, signature, 256);
        const size_t expectedChunks = (12 + layout.GetLayout().m_capacity - 1) /
            layout.GetLayout().m_capacity;
        for (int index = 0; index < 12; ++index)
        {
            const auto initial = m_world.CreateEntity();
            m_world.AddProcess(initial, Hestia::ProcessKey<&CreateDuringProcess>);
        }

        m_world.Run(Hestia::ECSProcessPhase::Update);

        EXPECT_TRUE(g_callbackObservedPending);
        EXPECT_TRUE(g_callbackObservedComponentPending);
        EXPECT_EQ(g_mutationCallbackCount, expectedChunks);
        ASSERT_NE(m_world.GetEntity(g_callbackCreated), nullptr);
        EXPECT_NE(m_world.GetComponent<Position>(g_callbackCreated), nullptr);
    }

    TEST_F(ECSFixture, DestroyingPendingCreationCancelsItAndAdvancesGeneration)
    {
        ASSERT_TRUE(m_world.RegisterProcess<&VisitPosition>(Hestia::ECSProcessPhase::Update));
        ASSERT_TRUE(m_world.RegisterProcess<&CancelPendingDuringProcess>(
            Hestia::ECSProcessPhase::Update));
        const auto initial = m_world.CreateEntity();
        m_world.AddProcess(initial, Hestia::ProcessKey<&CancelPendingDuringProcess>);

        m_world.Run(Hestia::ECSProcessPhase::Update);
        EXPECT_EQ(m_world.GetEntity(g_callbackCreated), nullptr);

        const auto recycled = m_world.CreateEntity();
        EXPECT_EQ(recycled.m_index, g_callbackCreated.m_index);
        EXPECT_NE(recycled.m_generation, g_callbackCreated.m_generation);
    }

    TEST_F(ECSFixture, DestroyReservationSkipsAlreadyQueuedComponentChanges)
    {
        ASSERT_TRUE(m_world.RegisterProcess<&VisitVelocity>(Hestia::ECSProcessPhase::Update));
        ASSERT_TRUE(m_world.RegisterProcess<&AddVelocityThenDestroy>(
            Hestia::ECSProcessPhase::Update));
        const auto handle = m_world.CreateEntity();
        m_world.AddProcess(handle, Hestia::ProcessKey<&AddVelocityThenDestroy>);

        m_world.Run(Hestia::ECSProcessPhase::Update);

        ASSERT_NE(m_world.GetEntity(handle), nullptr);
        EXPECT_TRUE(m_world.GetEntity(handle)->IsDestroy());
        EXPECT_EQ(m_world.GetComponent<Velocity>(handle), nullptr);
        EXPECT_EQ(g_velocityCalls, 0u);
        m_world.EndFrame();
        EXPECT_EQ(m_world.GetEntity(handle), nullptr);
    }

    TEST_F(ECSFixture, DestroyReservationKeepsActiveSettingUntilEndFrame)
    {
        ASSERT_TRUE(m_world.RegisterProcess<&VisitPosition>(Hestia::ECSProcessPhase::Update));
        const auto handle = m_world.CreateEntity();
        m_world.AddProcess(handle, Hestia::ProcessKey<&VisitPosition>);
        m_world.DestroyEntity(handle);

        Hestia::Entity* entity = m_world.GetEntity(handle);
        ASSERT_NE(entity, nullptr);
        EXPECT_FALSE(entity->IsActive());
        EXPECT_TRUE(entity->IsDestroy());
        m_world.EndFrame();
        EXPECT_EQ(m_world.GetEntity(handle), nullptr);
    }

    TEST_F(ECSFixture, ProcessExceptionStopsCurrentRun)
    {
        ASSERT_TRUE(m_world.RegisterProcess<&ThrowFromProcess>(Hestia::ECSProcessPhase::Update));
        ASSERT_TRUE(m_world.RegisterProcess<&ContinueAfterThrow>(Hestia::ECSProcessPhase::Update));
        const auto handle = m_world.CreateEntity();
        m_world.AddProcess(handle, Hestia::ProcessKey<&ThrowFromProcess>);
        m_world.AddProcess(handle, Hestia::ProcessKey<&ContinueAfterThrow>);
        g_throwFromProcess = true;

        m_world.Run(Hestia::ECSProcessPhase::Update);
        EXPECT_TRUE(g_processOrder.empty());
    }

    TEST_F(ECSFixture, InactiveEntitiesRemainInProcessRowsAndHandlesRejectStaleIndices)
    {
        ASSERT_TRUE(m_world.RegisterProcess<&VisitPosition>(Hestia::ECSProcessPhase::Update));
        const auto original = m_world.CreateEntity();
        m_world.AddProcess(original, Hestia::ProcessKey<&VisitPosition>);
        m_world.GetEntity(original)->SetActive(false);

        m_world.Run(Hestia::ECSProcessPhase::Update);
        EXPECT_EQ(g_seenEntities, 1u);

        Hestia::EntityHandle invalid{ 0xc0000000u, 1 };
        EXPECT_FALSE(invalid.IsValid());
        EXPECT_EQ(m_world.GetEntity(invalid), nullptr);
        m_world.DestroyEntity(original);
        m_world.EndFrame();
        EXPECT_EQ(m_world.GetEntity(original), nullptr);

        const auto recycled = m_world.CreateEntity();
        EXPECT_EQ(recycled.m_index, original.m_index);
        EXPECT_NE(recycled.m_generation, original.m_generation);
        EXPECT_EQ(m_world.GetEntity(original), nullptr);
    }

    TEST_F(ECSFixture, ChunkLayoutHonorsCapacityAndOverAlignedComponents)
    {
        Hestia::ArchetypeSignature signature;
        signature.m_types = {
            *m_registry.GetId<Position>(), *m_registry.GetId<OverAligned>() };
        Hestia::Archetype archetype(m_registry, signature, 512);
        const Hestia::ChunkLayout& layout = archetype.GetLayout();
        EXPECT_EQ(layout.m_byteSize, 512u);
        EXPECT_GT(layout.m_capacity, 0u);

        for (const Hestia::ComponentLayout& component : layout.m_components)
        {
            EXPECT_EQ(component.m_offset % component.m_info->alignment, 0u);
            EXPECT_LE(component.m_offset + layout.m_capacity * component.m_info->size,
                layout.m_byteSize);
        }

        Hestia::Chunk chunk(layout);
        const auto overAlignedColumn = std::find_if(layout.m_components.begin(),
            layout.m_components.end(), [](const Hestia::ComponentLayout& component)
            {
                return component.m_info->alignment == alignof(OverAligned);
            });
        ASSERT_NE(overAlignedColumn, layout.m_components.end());
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(chunk.GetElement(
            static_cast<size_t>(overAlignedColumn - layout.m_components.begin()), 0)) %
            alignof(OverAligned), 0u);

        const Hestia::Archetype invalidArchetype(m_registry, signature, 32);
        EXPECT_FALSE(invalidArchetype.IsValid());
    }

    TEST_F(ECSFixture, SwapRemoveKeepsMovedEntityResolvable)
    {
        ASSERT_TRUE(m_world.RegisterProcess<&VisitPosition>(Hestia::ECSProcessPhase::Update));
        const auto first = m_world.CreateEntity();
        const auto removed = m_world.CreateEntity();
        const auto moved = m_world.CreateEntity();
        const std::array handles{ first, removed, moved };
        for (const auto handle : handles)
            m_world.AddProcess(handle, Hestia::ProcessKey<&VisitPosition>);

        m_world.GetComponent<Position>(first)->m_value = 1;
        m_world.GetComponent<Position>(removed)->m_value = 2;
        m_world.GetComponent<Position>(moved)->m_value = 3;
        m_world.DestroyEntity(removed);
        m_world.EndFrame();

        EXPECT_EQ(m_world.GetEntity(removed), nullptr);
        ASSERT_NE(m_world.GetEntity(moved), nullptr);
        EXPECT_EQ(m_world.GetComponent<Position>(moved)->m_value, 3);
        EXPECT_EQ(m_world.GetComponent<Position>(first)->m_value, 1);
    }
}
