/*=============================================================================

 File   : TransformTests.cpp
 Desc   : Transformの公開操作、階層更新、特異な親とECS寿命の契約を検証する

------------------------------------------------------------------------------

 Date   : 2026/11/05
 Author : Yokoyama Haruki

=============================================================================*/

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <crtdbg.h>
#include <limits>
#include <vector>

#include "ECS/ECSSystem.h"
#include "ECS/Component.h"
#include "Game/Transform.h"
#include "Transform/TransformProcess.h"

namespace
{
    using Hestia::ECSSystem;
    using Hestia::ParentMode;
    using HestiaGame::Transform;

    constexpr float TOLERANCE = 2.0e-4f;

    void ExpectVector(const Vector3& actual, const Vector3& expected)
    {
        EXPECT_NEAR(actual.x, expected.x, TOLERANCE);
        EXPECT_NEAR(actual.y, expected.y, TOLERANCE);
        EXPECT_NEAR(actual.z, expected.z, TOLERANCE);
    }

    void ExpectMatrix(const Matrix4x4_SIMD& actual, const Matrix4x4_SIMD& expected)
    {
        for (size_t index = 0; index < 16; ++index)
            EXPECT_NEAR(actual.At(index), expected.At(index), TOLERANCE) << index;
    }

    void ExpectRotation(const Quaternion& actual, const Quaternion& expected)
    {
        ExpectMatrix(Quaternion_SIMD::Load(actual).ToMatrix(), Quaternion_SIMD::Load(expected).ToMatrix());
    }

    void ExpectFinite(const Matrix4x4_SIMD& matrix)
    {
        for (size_t index = 0; index < 16; ++index)
            EXPECT_TRUE(std::isfinite(matrix.At(index))) << index;
    }

    Matrix4x4_SIMD Compose(const Vector3& position, const Quaternion& rotation, const Vector3& scale)
    {
        Matrix4x4_SIMD result = Matrix4x4_SIMD::Scaling(scale.x, scale.y, scale.z);
        result *= Quaternion_SIMD::Load(rotation).ToMatrix();
        result.Translate(position.x, position.y, position.z);
        return result;
    }

    std::vector<float> g_renderPositions;

    void CollectWorldMatrices(size_t count, const Hestia::Entity*, const Hestia::TransformCache* caches)
    {
        for (size_t index = 0; index < count; ++index)
            g_renderPositions.push_back(caches[index].m_worldMatrix[3][0]);
    }

    class TransformTests : public testing::Test
    {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(m_system.Initialize({ 1024 }));
        }

        void TearDown() override
        {
            m_system.Finalize();
        }

        Transform Create()
        {
            return Transform(ECSSystem::CreateEntity());
        }

        Hestia::ECSSystem m_system;
    };

    TEST_F(TransformTests, EveryEntityStartsWithProtectedIdentityTransform)
    {
        RecordProperty("target", "ECSSystem::CreateEntity / RemoveProcess / Transform defaults");
        const auto handle = ECSSystem::CreateEntity();
        Transform transform(handle);
        ASSERT_TRUE(transform.IsValid());
        ASSERT_NE(ECSSystem::GetComponent<Hestia::Transform>(handle), nullptr);
        ASSERT_NE(ECSSystem::GetComponent<Hestia::TransformCache>(handle), nullptr);
        ASSERT_NE(ECSSystem::GetComponent<Hestia::TransformHierarchy>(handle), nullptr);
        ExpectVector(transform.GetLocalPosition(), Vector3::Zero());
        ExpectRotation(transform.GetLocalRotation(), Quaternion::Identity());
        ExpectVector(transform.GetLocalScale(), Vector3::One());
        ExpectMatrix(transform.GetWorldMatrix(), Matrix4x4_SIMD::Identity());
        EXPECT_FALSE(transform.GetParent().IsValid());

        ECSSystem::RemoveProcess<&Hestia::TransformProcess::Run>(handle);
        EXPECT_NE(ECSSystem::GetComponent<Hestia::Transform>(handle), nullptr);
        EXPECT_NE(ECSSystem::GetComponent<Hestia::TransformCache>(handle), nullptr);
        EXPECT_NE(ECSSystem::GetComponent<Hestia::TransformHierarchy>(handle), nullptr);
    }

    TEST_F(TransformTests, LazyGetUpdatesAncestorsAndRepeatedChangesReachDescendants)
    {
        RecordProperty("target", "Transform setters / GetWorldMatrix / dirty propagation");
        Transform root = Create();
        Transform child = Create();
        Transform leaf = Create();
        ASSERT_TRUE(root.SetLocalPosition({ 10, 0, 0 }));
        ASSERT_TRUE(child.SetLocalPosition({ 1, 0, 0 }));
        ASSERT_TRUE(leaf.SetLocalPosition({ 2, 0, 0 }));
        ASSERT_TRUE(child.SetParent(root, ParentMode::KeepLocal));
        ASSERT_TRUE(leaf.SetParent(child, ParentMode::KeepLocal));
        ExpectVector(leaf.GetWorldPosition(), { 13, 0, 0 });

        ASSERT_TRUE(root.SetLocalPosition({ 20, 0, 0 }));
        ASSERT_TRUE(root.SetLocalPosition({ 30, 0, 0 }));
        ExpectVector(root.GetWorldPosition(), { 30, 0, 0 });
        ASSERT_TRUE(root.SetLocalPosition({ 40, 0, 0 }));
        ExpectVector(leaf.GetWorldPosition(), { 43, 0, 0 });
        ASSERT_TRUE(child.SetLocalPosition({ 3, 0, 0 }));
        ExpectVector(leaf.GetWorldPosition(), { 45, 0, 0 });
    }

    TEST_F(TransformTests, PreRenderPublishesAContiguousMatrixOnlyColumnIncludingInactive)
    {
        RecordProperty("target", "TransformProcess::Run / TransformCache column");
        static_assert(sizeof(Hestia::TransformCache) == sizeof(Matrix4x4_SIMD));
        const auto parentHandle = ECSSystem::CreateEntity();
        const auto childHandle = ECSSystem::CreateEntity();
        Transform parent(parentHandle);
        Transform child(childHandle);
        ASSERT_TRUE(parent.SetLocalPosition({ 4, 0, 0 }));
        ASSERT_TRUE(child.SetLocalPosition({ 3, 0, 0 }));
        ASSERT_TRUE(child.SetParent(parent, ParentMode::KeepLocal));
        ECSSystem::GetEntity(parentHandle)->SetActive(false);
        ECSSystem::Run(Hestia::ECSProcessPhase::PreRender);

        const auto* first = ECSSystem::GetComponent<Hestia::TransformCache>(parentHandle);
        const auto* second = ECSSystem::GetComponent<Hestia::TransformCache>(childHandle);
        ASSERT_EQ(reinterpret_cast<uintptr_t>(second) - reinterpret_cast<uintptr_t>(first),
            sizeof(Hestia::TransformCache));
        g_renderPositions.clear();
        CollectWorldMatrices(2, nullptr, first);
        ASSERT_EQ(g_renderPositions.size(), 2u);
        EXPECT_FLOAT_EQ(g_renderPositions[0], 4.0f);
        EXPECT_FLOAT_EQ(g_renderPositions[1], 7.0f);
        ExpectVector(child.GetWorldPosition(), { 7, 0, 0 });
    }

    TEST_F(TransformTests, HierarchiesCrossChunksAndKeepFacadeValidAfterSwapRemove)
    {
        RecordProperty("target", "Transform hierarchy across Chunk / EndFrame swap remove");
        Transform root = Create();
        const auto removed = ECSSystem::CreateEntity();
        std::vector<Transform> children;
        for (int index = 0; index < 40; ++index)
        {
            Transform child = Create();
            ASSERT_TRUE(child.SetLocalPosition({ static_cast<float>(index), 0, 0 }));
            ASSERT_TRUE(child.SetParent(root, ParentMode::KeepLocal));
            children.push_back(child);
        }
        ECSSystem::DestroyEntity(removed);
        ECSSystem::EndFrame();
        ASSERT_TRUE(root.SetLocalPosition({ 8, 0, 0 }));
        ECSSystem::Run(Hestia::ECSProcessPhase::PreRender);
        for (size_t index = 0; index < children.size(); ++index)
        {
            EXPECT_TRUE(children[index].IsValid());
            ExpectVector(children[index].GetWorldPosition(), { 8.0f + index, 0, 0 });
        }
    }

    TEST_F(TransformTests, WorldRotationComposesQuaternionsIndependentlyOfScale)
    {
        RecordProperty("target", "Transform::GetWorldRotation / SetWorldRotation");
        Transform parent = Create();
        Transform child = Create();
        const Quaternion parentRotation = Quaternion::FromAxisAngle(Vector3::Up(), 0.6f);
        const Quaternion localRotation = Quaternion::FromAxisAngle(Vector3::Right(), 0.4f);
        ASSERT_TRUE(parent.SetLocalRotation(parentRotation));
        ASSERT_TRUE(parent.SetLocalScale({ 2, 3, 4 }));
        ASSERT_TRUE(child.SetLocalRotation(localRotation));
        ASSERT_TRUE(child.SetParent(parent, ParentMode::KeepLocal));
        ExpectRotation(child.GetWorldRotation(), localRotation * parentRotation);

        const Quaternion desired = Quaternion::FromAxisAngle(Vector3::Forward(), 0.8f);
        ASSERT_TRUE(child.SetWorldRotation(desired));
        ExpectRotation(child.GetWorldRotation(), desired);
        ExpectRotation(child.GetLocalRotation(), desired * parentRotation.Inversed());
    }

    TEST_F(TransformTests, WorldScaleSetterHandlesRotatedNonUniformAndNegativeScale)
    {
        RecordProperty("target", "Transform::GetWorldScale / SetWorldScale");
        Transform parent = Create();
        Transform child = Create();
        ASSERT_TRUE(parent.SetLocalScale({ -2, 3, 4 }));
        ASSERT_TRUE(parent.SetLocalRotation(Quaternion::FromAxisAngle(Vector3::Up(), 0.2f)));
        ASSERT_TRUE(child.SetLocalRotation(Quaternion::FromAxisAngle(Vector3::Forward(), 0.5f)));
        ASSERT_TRUE(child.SetParent(parent, ParentMode::KeepLocal));
        const Quaternion rotation = child.GetWorldRotation();
        ASSERT_TRUE(child.SetWorldScale({ -5, 6, 7 }));
        ExpectVector(child.GetWorldScale(), { -5, 6, 7 });
        ExpectRotation(child.GetWorldRotation(), rotation);
        ExpectFinite(child.GetWorldMatrix());
    }

    TEST_F(TransformTests, WorldPositionSolvesAParentContainingShear)
    {
        RecordProperty("target", "Transform::SetWorldPosition / parent linear solve");
        Transform root = Create();
        Transform parent = Create();
        Transform child = Create();
        ASSERT_TRUE(root.SetLocalScale({ 2, 3, 4 }));
        ASSERT_TRUE(root.SetLocalRotation(Quaternion::FromAxisAngle(Vector3::Forward(), 0.4f)));
        ASSERT_TRUE(parent.SetLocalRotation(Quaternion::FromAxisAngle(Vector3::Up(), 0.6f)));
        ASSERT_TRUE(parent.SetLocalScale({ 1, 4, 2 }));
        ASSERT_TRUE(parent.SetParent(root, ParentMode::KeepLocal));
        ASSERT_TRUE(child.SetParent(parent, ParentMode::KeepLocal));
        ASSERT_TRUE(child.SetWorldPosition({ 10, -2, 7 }));
        ExpectVector(child.GetWorldPosition(), { 10, -2, 7 });
        ExpectRotation(child.GetLocalRotation(), Quaternion::Identity());
        ExpectVector(child.GetLocalScale(), Vector3::One());
    }

    TEST_F(TransformTests, ZeroScaleRetainsUnsolvableLocalPositionAndScaleUntilRestored)
    {
        RecordProperty("target", "Transform World setters under zero scale");
        Transform parent = Create();
        Transform child = Create();
        ASSERT_TRUE(parent.SetLocalPosition({ 10, 2, 3 }));
        ASSERT_TRUE(parent.SetLocalScale({ 0, 2, 3 }));
        ASSERT_TRUE(child.SetLocalPosition({ 7, 8, 9 }));
        ASSERT_TRUE(child.SetLocalScale({ 4, 5, 6 }));
        ASSERT_TRUE(child.SetParent(parent, ParentMode::KeepLocal));
        ASSERT_TRUE(child.SetWorldPosition({ 123, 20, 30 }));
        ASSERT_TRUE(child.SetWorldScale({ 999, 8, 12 }));
        ExpectVector(child.GetLocalPosition(), { 7, 9, 9 });
        ExpectVector(child.GetLocalScale(), { 4, 4, 4 });
        ExpectVector(child.GetWorldPosition(), { 10, 20, 30 });
        ExpectVector(child.GetWorldScale(), { 0, 8, 12 });
        ExpectFinite(child.GetWorldMatrix());

        ASSERT_TRUE(parent.SetLocalScale({ 1, 2, 3 }));
        ExpectVector(child.GetWorldPosition(), { 17, 20, 30 });
        ExpectVector(child.GetWorldScale(), { 4, 8, 12 });
    }

    TEST_F(TransformTests, SingularRotatedParentPreservesTheUnobservableLocalDirection)
    {
        RecordProperty("target", "Transform singular least squares position solve");
        Transform root = Create();
        Transform parent = Create();
        Transform child = Create();
        constexpr float ANGLE = 0.7f;
        ASSERT_TRUE(root.SetLocalScale({ 0, 2, 3 }));
        ASSERT_TRUE(parent.SetLocalRotation(Quaternion::FromAxisAngle(Vector3::Forward(), ANGLE)));
        ASSERT_TRUE(parent.SetParent(root, ParentMode::KeepLocal));
        ASSERT_TRUE(child.SetLocalPosition({ 4, 5, 6 }));
        ASSERT_TRUE(child.SetParent(parent, ParentMode::KeepLocal));
        const Vector3 before = child.GetWorldPosition();
        const Vector3 oldLocal = child.GetLocalPosition();
        ASSERT_TRUE(child.SetWorldPosition({ 99, before.y + 6, before.z + 9 }));
        ExpectVector(child.GetWorldPosition(), { 0, before.y + 6, before.z + 9 });
        const Vector3 nullDirection(std::cos(ANGLE), -std::sin(ANGLE), 0);
        EXPECT_NEAR(Vector3::Dot(child.GetLocalPosition() - oldLocal, nullDirection), 0.0f, TOLERANCE);
        ExpectFinite(child.GetWorldMatrix());
    }

    TEST_F(TransformTests, KeepWorldAndUnparentPreserveAnOrdinaryWorldMatrix)
    {
        RecordProperty("target", "Transform::SetParent KeepWorld / root");
        Transform parent = Create();
        Transform child = Create();
        ASSERT_TRUE(parent.SetLocalPosition({ 10, 20, 30 }));
        ASSERT_TRUE(parent.SetLocalScale({ 2, 2, 2 }));
        ASSERT_TRUE(parent.SetLocalRotation(Quaternion::FromAxisAngle(Vector3::Up(), 0.6f)));
        ASSERT_TRUE(child.SetLocalPosition({ 1, 2, 3 }));
        ASSERT_TRUE(child.SetLocalRotation(Quaternion::FromAxisAngle(Vector3::Forward(), 0.2f)));
        ASSERT_TRUE(child.SetLocalScale({ 2, 3, 4 }));
        const Matrix4x4_SIMD before = child.GetWorldMatrix();
        ASSERT_TRUE(child.SetParent(parent));
        ExpectMatrix(child.GetWorldMatrix(), before);
        ASSERT_TRUE(child.SetParent(Transform{}));
        EXPECT_FALSE(child.GetParent().IsValid());
        ExpectMatrix(child.GetWorldMatrix(), before);
    }

    TEST_F(TransformTests, KeepWorldMaintainsItsTrsContractUnderNonUniformParents)
    {
        RecordProperty("target", "Transform::SetParent non-uniform KeepWorld");
        Transform first = Create();
        Transform second = Create();
        Transform child = Create();
        ASSERT_TRUE(first.SetLocalScale({ 2, 3, 4 }));
        ASSERT_TRUE(first.SetLocalRotation(Quaternion::FromAxisAngle(Vector3::Up(), 0.3f)));
        ASSERT_TRUE(second.SetLocalScale({ 4, 1, 2 }));
        ASSERT_TRUE(second.SetLocalRotation(Quaternion::FromAxisAngle(Vector3::Right(), 0.6f)));
        ASSERT_TRUE(child.SetLocalRotation(Quaternion::FromAxisAngle(Vector3::Forward(), 0.5f)));
        ASSERT_TRUE(child.SetLocalPosition({ 1, 2, 3 }));
        ASSERT_TRUE(child.SetParent(first, ParentMode::KeepLocal));
        const Vector3 position = child.GetWorldPosition();
        const Quaternion rotation = child.GetWorldRotation();
        const Vector3 scale = child.GetWorldScale();
        ASSERT_TRUE(child.SetParent(second));
        ExpectVector(child.GetWorldPosition(), position);
        ExpectRotation(child.GetWorldRotation(), rotation);
        ExpectVector(child.GetWorldScale(), scale);
        ExpectFinite(child.GetWorldMatrix());
    }

    TEST_F(TransformTests, KeepWorldAcceptsAZeroScaleParentWithoutClearingLocal)
    {
        RecordProperty("target", "Transform::SetParent KeepWorld under zero scale");
        Transform parent = Create();
        Transform child = Create();
        ASSERT_TRUE(parent.SetLocalScale({ 0, 2, 3 }));
        ASSERT_TRUE(child.SetLocalPosition({ 7, 8, 9 }));
        ASSERT_TRUE(child.SetLocalScale({ 4, 5, 6 }));
        ASSERT_TRUE(child.SetParent(parent));
        ExpectVector(child.GetLocalPosition(), { 7, 4, 3 });
        ExpectVector(child.GetLocalScale(), { 4, 2.5f, 2 });
        ExpectVector(child.GetWorldPosition(), { 0, 8, 9 });
        ExpectVector(child.GetWorldScale(), { 0, 5, 6 });
        ExpectFinite(child.GetWorldMatrix());
    }

    TEST_F(TransformTests, SetWorldMatrixPublishesCanonicalLocalTrsWithoutHiddenShear)
    {
        RecordProperty("target", "Transform::SetWorldMatrix / canonical TRS cache");
        Transform transform = Create();
        const Matrix4x4_SIMD desired = Compose({ 1, 2, 3 },
            Quaternion::FromAxisAngle(Vector3::Up(), 0.5f), { 2, 3, -4 });
        ASSERT_TRUE(transform.SetWorldMatrix(desired));
        ExpectMatrix(transform.GetWorldMatrix(), desired);

        Matrix4x4_SIMD sheared = Matrix4x4_SIMD::Identity();
        sheared[0][1] = 0.6f;
        ASSERT_TRUE(transform.SetWorldMatrix(sheared));
        const Matrix4x4_SIMD canonical = transform.GetWorldMatrix();
        ExpectFinite(canonical);
        ExpectMatrix(canonical, Compose(transform.GetLocalPosition(), transform.GetLocalRotation(),
            transform.GetLocalScale()));
        ASSERT_TRUE(transform.SetLocalPosition(transform.GetLocalPosition()));
        ExpectMatrix(transform.GetWorldMatrix(), canonical);
    }

    TEST_F(TransformTests, WorldMatrixSetterConvertsItsTrsThroughANonUniformParent)
    {
        RecordProperty("target", "Transform::SetWorldMatrix under non-uniform parent");
        Transform parent = Create();
        Transform child = Create();
        ASSERT_TRUE(parent.SetLocalPosition({ 10, -3, 4 }));
        ASSERT_TRUE(parent.SetLocalRotation(Quaternion::FromAxisAngle(Vector3::Up(), 0.5f)));
        ASSERT_TRUE(parent.SetLocalScale({ 2, 3, 4 }));
        ASSERT_TRUE(child.SetParent(parent, ParentMode::KeepLocal));
        const Vector3 position(1, 2, 3);
        const Quaternion rotation = Quaternion::FromAxisAngle(Vector3::Forward(), 0.2f);
        const Vector3 scale(2, 3, 4);
        ASSERT_TRUE(child.SetWorldMatrix(Compose(position, rotation, scale)));
        ExpectVector(child.GetWorldPosition(), position);
        ExpectRotation(child.GetWorldRotation(), rotation);
        ExpectVector(child.GetWorldScale(), scale);
        ExpectFinite(child.GetWorldMatrix());
    }

    TEST_F(TransformTests, CollapsedWorldMatrixKeepsLocalThatCannotBeRecovered)
    {
        RecordProperty("target", "Transform::SetWorldMatrix zero scale fallback");
        Transform parent = Create();
        Transform child = Create();
        ASSERT_TRUE(parent.SetLocalScale(Vector3::Zero()));
        ASSERT_TRUE(child.SetLocalPosition({ 3, 4, 5 }));
        ASSERT_TRUE(child.SetLocalScale({ 6, 7, 8 }));
        ASSERT_TRUE(child.SetParent(parent, ParentMode::KeepLocal));
        Matrix4x4_SIMD collapsed;
        collapsed[3][0] = 10;
        collapsed[3][3] = 1;
        ASSERT_TRUE(child.SetWorldMatrix(collapsed));
        ExpectVector(child.GetLocalPosition(), { 3, 4, 5 });
        ExpectVector(child.GetLocalScale(), { 6, 7, 8 });
        ExpectFinite(child.GetWorldMatrix());
        ASSERT_TRUE(parent.SetLocalScale(Vector3::One()));
        ExpectVector(child.GetWorldPosition(), { 3, 4, 5 });
        ExpectVector(child.GetWorldScale(), { 6, 7, 8 });
    }

    TEST_F(TransformTests, InvalidInputsAndCyclesLeaveLocalAndParentUnchanged)
    {
        RecordProperty("target", "Transform setters input validation / cycles");
        Transform parent = Create();
        Transform child = Create();
        ASSERT_TRUE(child.SetLocalPosition({ 1, 2, 3 }));
        ASSERT_TRUE(child.SetParent(parent, ParentMode::KeepLocal));
        EXPECT_FALSE(parent.SetParent(child));
        EXPECT_FALSE(child.SetParent(child));
        EXPECT_FALSE(parent.GetParent().IsValid());
        EXPECT_TRUE(child.GetParent().IsValid());
        EXPECT_FALSE(child.SetParent(parent, static_cast<ParentMode>(99)));

        const float invalid = std::numeric_limits<float>::infinity();
        ASSERT_TRUE(parent.SetLocalScale(Vector3::Zero()));
        EXPECT_FALSE(child.SetLocalPosition({ invalid, 0, 0 }));
        EXPECT_FALSE(child.SetWorldPosition({ invalid, 0, 0 }));
        EXPECT_FALSE(child.SetLocalRotation({ 0, invalid, 0, 1 }));
        EXPECT_FALSE(child.SetWorldRotation({ 0, 0, invalid, 1 }));
        EXPECT_FALSE(child.SetLocalScale({ invalid, 1, 1 }));
        EXPECT_FALSE(child.SetWorldScale({ invalid, 1, 1 }));
        Matrix4x4_SIMD projection = Matrix4x4_SIMD::Identity();
        projection[0][3] = 1;
        EXPECT_FALSE(child.SetWorldMatrix(projection));
        ExpectVector(child.GetLocalPosition(), { 1, 2, 3 });
        ExpectVector(child.GetLocalScale(), Vector3::One());
        ExpectRotation(child.GetLocalRotation(), Quaternion::Identity());
    }

    TEST_F(TransformTests, DestroyReservesTheWholeSubtreeAndUnlinksSurvivingParent)
    {
        RecordProperty("target", "ECSWorld::DestroyEntity / FlushDestroy hierarchy");
        const auto anchorHandle = ECSSystem::CreateEntity();
        const auto rootHandle = ECSSystem::CreateEntity();
        const auto childHandle = ECSSystem::CreateEntity();
        const auto leafHandle = ECSSystem::CreateEntity();
        Transform anchor(anchorHandle), root(rootHandle), child(childHandle), leaf(leafHandle);
        ASSERT_TRUE(root.SetParent(anchor));
        ASSERT_TRUE(child.SetParent(root));
        ASSERT_TRUE(leaf.SetParent(child));
        ECSSystem::GetEntity(childHandle)->SetActive(false);
        ECSSystem::DestroyEntity(rootHandle);
        EXPECT_FALSE(root.IsValid());
        EXPECT_FALSE(child.IsValid());
        EXPECT_FALSE(leaf.IsValid());
        EXPECT_TRUE(anchor.IsValid());
        EXPECT_TRUE(ECSSystem::GetEntity(leafHandle)->IsDestroy());
        EXPECT_FALSE(child.SetLocalPosition({ 1, 0, 0 }));
        EXPECT_FALSE(anchor.SetParent(root));

        ECSSystem::EndFrame();
        EXPECT_EQ(ECSSystem::GetEntity(rootHandle), nullptr);
        EXPECT_EQ(ECSSystem::GetEntity(childHandle), nullptr);
        EXPECT_EQ(ECSSystem::GetEntity(leafHandle), nullptr);
        EXPECT_TRUE(ECSSystem::GetComponent<Hestia::TransformHierarchy>(anchorHandle)->m_children.empty());
        Transform replacement = Create();
        EXPECT_TRUE(replacement.IsValid());
        EXPECT_FALSE(root.IsValid());
    }

    TEST_F(TransformTests, DetachedChildSurvivesFormerParentDestruction)
    {
        RecordProperty("target", "Transform::SetParent root / destruction ownership");
        const auto parentHandle = ECSSystem::CreateEntity();
        Transform parent(parentHandle);
        Transform child = Create();
        ASSERT_TRUE(parent.SetLocalPosition({ 2, 0, 0 }));
        ASSERT_TRUE(child.SetLocalPosition({ 3, 0, 0 }));
        ASSERT_TRUE(child.SetParent(parent, ParentMode::KeepLocal));
        ASSERT_TRUE(child.SetParent(Transform{}));
        ECSSystem::DestroyEntity(parentHandle);
        ECSSystem::EndFrame();
        EXPECT_TRUE(child.IsValid());
        ExpectVector(child.GetWorldPosition(), { 5, 0, 0 });
    }

    TEST_F(TransformTests, ProcessCanWriteBorrowedColumnsWithTheSameDirtyPropagation)
    {
        RecordProperty("target", "TransformProcess::SetLocalTRS column operation");
        const auto handle = ECSSystem::CreateEntity();
        Transform parent(handle);
        Transform child = Create();
        ASSERT_TRUE(child.SetParent(parent));
        ASSERT_TRUE(Hestia::TransformProcess::SetLocalTRS(
            *ECSSystem::GetComponent<Hestia::Transform>(handle),
            *ECSSystem::GetComponent<Hestia::TransformHierarchy>(handle),
            { 9, 8, 7 }, Quaternion::Identity(), Vector3::One()));
        ExpectVector(child.GetWorldPosition(), { 9, 8, 7 });
    }

    TEST_F(TransformTests, InvalidGetHasADebugAssertion)
    {
        RecordProperty("target", "Transform invalid getter assertion");
#ifndef NDEBUG
        EXPECT_DEATH({
            _set_error_mode(_OUT_TO_STDERR);
            _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
            _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
            Transform{}.GetWorldMatrix();
        }, "Transform Get requires a live Entity");
#endif
    }

    TEST(TransformCacheProcess, QueriesOnlyWorldMatricesAcrossChunks)
    {
        RecordProperty("target", "ECSWorld Cache-only Process registration / SoA column");
        Hestia::ComponentRegistry registry;
        ASSERT_TRUE(registry.Register<Hestia::Name>("Hestia::Name"));
        ASSERT_TRUE(registry.Register<Hestia::Tag>("Hestia::Tag"));
        ASSERT_TRUE(registry.Register<Hestia::Layer>("Hestia::Layer"));
        ASSERT_TRUE(registry.Register<Hestia::Transform>("Hestia::Transform"));
        ASSERT_TRUE(registry.Register<Hestia::TransformCache>("Hestia::TransformCache"));
        ASSERT_TRUE(registry.Register<Hestia::TransformHierarchy>("Hestia::TransformHierarchy"));
        Hestia::ECSWorld world(registry, 1024);
        ASSERT_TRUE(world.Initialize());
        ASSERT_TRUE(world.RegisterProcess<&CollectWorldMatrices>(Hestia::ECSProcessPhase::PreRender));
        for (int index = 0; index < 12; ++index)
        {
            const auto handle = world.CreateEntity();
            const float position = static_cast<float>(index);
            auto& transform = *world.GetComponent<Hestia::Transform>(handle);
            transform.m_localPosition.x = position;
            transform.m_localMatrix.Translate(position, 0, 0);
            world.GetComponent<Hestia::TransformCache>(handle)->m_worldMatrix.Translate(position, 0, 0);
        }
        g_renderPositions.clear();
        world.Run(Hestia::ECSProcessPhase::PreRender);
        ASSERT_EQ(g_renderPositions.size(), 12u);
        std::sort(g_renderPositions.begin(), g_renderPositions.end());
        for (size_t index = 0; index < g_renderPositions.size(); ++index)
            EXPECT_FLOAT_EQ(g_renderPositions[index], static_cast<float>(index));
    }

    TEST(TransformLifetime, FacadeDoesNotKeepAnEcsAlive)
    {
        RecordProperty("target", "Transform::IsValid after ECS finalization");
        Transform facade;
        EXPECT_FALSE(facade.IsValid());
        {
            Hestia::ECSSystem system;
            ASSERT_TRUE(system.Initialize());
            facade = Transform(ECSSystem::CreateEntity());
            EXPECT_TRUE(facade.IsValid());
        }
        EXPECT_FALSE(facade.IsValid());
        EXPECT_FALSE(facade.SetLocalPosition(Vector3::Zero()));
    }
}
