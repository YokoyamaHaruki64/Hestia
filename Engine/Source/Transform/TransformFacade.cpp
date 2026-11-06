/*=============================================================================

 File   : TransformFacade.cpp
 Desc   : Game向けTransform操作をEngine.dll内のECSへ接続する

------------------------------------------------------------------------------

 Date   : 2026/11/05
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Game/Transform.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "ECS/ECSWorld.h"
#include "Log/Logger.h"
#include "Transform/TransformProcess.h"

namespace HestiaGame
{
    using Hestia::ECSWorld;
    using Hestia::TransformCache;
    using Hestia::TransformHierarchy;
    using Hestia::TransformProcess;

    bool Transform::IsValid() const noexcept
    {
        return TransformProcess::Resolve(m_entity) != nullptr;
    }

    ECSWorld* Transform::ResolveForGet() const
    {
        ECSWorld* world = TransformProcess::Resolve(m_entity);
        if (world == nullptr)
        {
            // Debugで停止する前に理由を記録し、assert無効時はGetterの既定値に委ねる
            Hestia::Logger::Error("Transform Get requires a live Entity");
            assert(world != nullptr && "Transform Get requires a live Entity");
        }
        return world;
    }

    ECSWorld* Transform::ResolveForSet() const
    {
        ECSWorld* world = TransformProcess::Resolve(m_entity);
        if (world == nullptr)
            Hestia::Logger::Error("Transform Set requires a live Entity");
        return world;
    }

    Vector3 Transform::GetLocalPosition() const
    {
        ECSWorld* world = ResolveForGet();
        return world != nullptr ? world->GetComponent<Hestia::Transform>(m_entity)->m_localPosition :
            Vector3::Zero();
    }

    Quaternion Transform::GetLocalRotation() const
    {
        ECSWorld* world = ResolveForGet();
        return world != nullptr ? world->GetComponent<Hestia::Transform>(m_entity)->m_localRotation :
            Quaternion::Identity();
    }

    Vector3 Transform::GetLocalScale() const
    {
        ECSWorld* world = ResolveForGet();
        return world != nullptr ? world->GetComponent<Hestia::Transform>(m_entity)->m_localScale :
            Vector3::One();
    }

    bool Transform::SetLocalPosition(const Vector3& position)
    {
        ECSWorld* world = ResolveForSet();
        if (world == nullptr)
            return false;

        auto& transform = *world->GetComponent<Hestia::Transform>(m_entity);
        return TransformProcess::SetLocalTRS(transform, *world->GetComponent<TransformHierarchy>(m_entity),
            position, transform.m_localRotation, transform.m_localScale);
    }

    bool Transform::SetLocalRotation(const Quaternion& rotation)
    {
        ECSWorld* world = ResolveForSet();
        if (world == nullptr)
            return false;

        auto& transform = *world->GetComponent<Hestia::Transform>(m_entity);
        return TransformProcess::SetLocalTRS(transform, *world->GetComponent<TransformHierarchy>(m_entity),
            transform.m_localPosition, rotation, transform.m_localScale);
    }

    bool Transform::SetLocalScale(const Vector3& scale)
    {
        ECSWorld* world = ResolveForSet();
        if (world == nullptr)
            return false;

        auto& transform = *world->GetComponent<Hestia::Transform>(m_entity);
        return TransformProcess::SetLocalTRS(transform, *world->GetComponent<TransformHierarchy>(m_entity),
            transform.m_localPosition, transform.m_localRotation, scale);
    }

    Matrix4x4_SIMD Transform::GetWorldMatrix() const
    {
        ECSWorld* world = ResolveForGet();
        return world != nullptr ? TransformProcess::EnsureWorld(*world, m_entity) :
            Matrix4x4_SIMD::Identity();
    }

    Vector3 Transform::GetWorldPosition() const
    {
        const Matrix4x4_SIMD matrix = GetWorldMatrix();
        return { matrix[3][0], matrix[3][1], matrix[3][2] };
    }

    Quaternion Transform::GetWorldRotation() const
    {
        ECSWorld* world = ResolveForGet();
        if (world == nullptr)
            return Quaternion::Identity();

        // 行列Cacheを更新し、回転は拡縮の影響を受けないQuaternionの合成値から取る
        TransformProcess::EnsureWorld(*world, m_entity);
        return TransformProcess::WorldRotation(*world, m_entity);
    }

    Vector3 Transform::GetWorldScale() const
    {
        ECSWorld* world = ResolveForGet();
        if (world == nullptr)
            return Vector3::One();

        // 行列Cacheを更新し、親の変換を反映した基底から拡縮を求める
        TransformProcess::EnsureWorld(*world, m_entity);
        return TransformProcess::WorldScale(*world, m_entity);
    }

    bool Transform::SetWorldPosition(const Vector3& position)
    {
        // 有効な対象と有限な入力だけを座標変換へ渡す
        ECSWorld* world = ResolveForSet();
        if (world == nullptr)
            return false;

        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
        {
            Hestia::Logger::Error("Transform received a non-finite World position");
            return false;
        }

        // 親の最新Worldから位置を逆算し、回転と拡縮はそのまま残す
        auto& transform = *world->GetComponent<Hestia::Transform>(m_entity);
        const auto& hierarchy = *world->GetComponent<TransformHierarchy>(m_entity);
        const Matrix4x4_SIMD parentWorld = hierarchy.m_parent.IsValid() ?
            TransformProcess::EnsureWorld(*world, hierarchy.m_parent) : Matrix4x4_SIMD::Identity();
        const Vector3 local = TransformProcess::LocalPosition(parentWorld, transform.m_localPosition, position);
        return TransformProcess::WriteLocal(*world, transform, hierarchy, local,
            transform.m_localRotation, transform.m_localScale);
    }

    bool Transform::SetWorldRotation(const Quaternion& rotation)
    {
        // 対象と入力を検証してから、Quaternionを正規化する
        ECSWorld* world = ResolveForSet();
        if (world == nullptr)
            return false;

        if (!std::isfinite(rotation.x) || !std::isfinite(rotation.y) ||
            !std::isfinite(rotation.z) || !std::isfinite(rotation.w))
        {
            Hestia::Logger::Error("Transform received a non-finite World rotation");
            return false;
        }

        // 親の回転を取り除き、Localの位置と拡縮を保持する
        auto& transform = *world->GetComponent<Hestia::Transform>(m_entity);
        const auto& hierarchy = *world->GetComponent<TransformHierarchy>(m_entity);
        const Quaternion parent = hierarchy.m_parent.IsValid() ?
            TransformProcess::WorldRotation(*world, hierarchy.m_parent) : Quaternion::Identity();
        return TransformProcess::WriteLocal(*world, transform, hierarchy, transform.m_localPosition,
            rotation.Normalized() * parent.Inversed(), transform.m_localScale);
    }

    bool Transform::SetWorldScale(const Vector3& scale)
    {
        // 非有限値がLocalへ伝わらないよう、変換前に入力を検証する
        ECSWorld* world = ResolveForSet();
        if (world == nullptr)
            return false;

        if (!std::isfinite(scale.x) || !std::isfinite(scale.y) || !std::isfinite(scale.z))
        {
            Hestia::Logger::Error("Transform received a non-finite World scale");
            return false;
        }

        // 現在の回転を基準に、親の変換が各軸に与える倍率を求める
        auto& transform = *world->GetComponent<Hestia::Transform>(m_entity);
        const auto& hierarchy = *world->GetComponent<TransformHierarchy>(m_entity);
        const Vector3 factors = TransformProcess::ScaleFactors(*world, hierarchy.m_parent,
            transform.m_localRotation, TransformProcess::WorldRotation(*world, m_entity));

        // 親の拡縮が戻ったときに使えるよう、逆算できない軸のLocal値を残す
        const Vector3 local = {
            factors.x == 0.0f ? transform.m_localScale.x : scale.x / factors.x,
            factors.y == 0.0f ? transform.m_localScale.y : scale.y / factors.y,
            factors.z == 0.0f ? transform.m_localScale.z : scale.z / factors.z };
        return TransformProcess::WriteLocal(*world, transform, hierarchy, transform.m_localPosition,
            transform.m_localRotation, local);
    }

    bool Transform::SetWorldMatrix(const Matrix4x4_SIMD& matrix)
    {
        ECSWorld* world = ResolveForSet();
        if (world == nullptr)
            return false;

        // 入力をWorld TRSへ近似し、座標変換に使えない行列は書き込み前に拒否する
        Vector3 position;
        Quaternion rotation;
        Vector3 scale;
        if (!TransformProcess::DecomposeWorld(matrix, TransformProcess::WorldRotation(*world, m_entity),
            position, rotation, scale))
        {
            Hestia::Logger::Error("Transform requires a finite affine World matrix");
            return false;
        }

        // 現在の親を基準にLocalへ変換して、TRSの正本を確定する
        return TransformProcess::WriteWorld(*world,
            world->GetComponent<TransformHierarchy>(m_entity)->m_parent,
            *world->GetComponent<Hestia::Transform>(m_entity),
            *world->GetComponent<TransformHierarchy>(m_entity), position, rotation, scale);
    }

    Transform Transform::GetParent() const
    {
        ECSWorld* world = ResolveForGet();
        return world != nullptr ? Transform(world->GetComponent<TransformHierarchy>(m_entity)->m_parent) :
            Transform{};
    }

    bool Transform::SetParent(const Transform& parent, Hestia::ParentMode mode)
    {
        // 自身と新しい親の生存状態を確認する。空の親は解除として扱う
        ECSWorld* world = ResolveForSet();
        if (world == nullptr)
            return false;

        if ((mode != Hestia::ParentMode::KeepWorld && mode != Hestia::ParentMode::KeepLocal) ||
            (parent.m_entity != Hestia::EntityHandle{} && TransformProcess::Resolve(parent.m_entity) != world))
        {
            Hestia::Logger::Error("Transform received an invalid parent or ParentMode");
            return false;
        }

        // 新しい親から祖先を辿り、自身が見つかれば循環になるため拒否する
        for (Hestia::EntityHandle ancestor = parent.m_entity; ancestor.IsValid();
            ancestor = world->GetComponent<TransformHierarchy>(ancestor)->m_parent)
        {
            if (ancestor == m_entity)
            {
                Hestia::Logger::Error("Transform parent would form a cycle");
                return false;
            }
        }

        auto& hierarchy = *world->GetComponent<TransformHierarchy>(m_entity);
        if (hierarchy.m_parent == parent.m_entity)
            return true;

        // KeepWorldでは、付け替える前の階層で維持するTRSを取得する
        auto& transform = *world->GetComponent<Hestia::Transform>(m_entity);
        Vector3 position;
        Quaternion rotation;
        Vector3 scale;
        if (mode == Hestia::ParentMode::KeepWorld)
        {
            position = GetWorldPosition();
            rotation = TransformProcess::WorldRotation(*world, m_entity);
            scale = TransformProcess::WorldScale(*world, m_entity);
        }

        // 確保の失敗で変更が途中に残らないよう、新しい親の子一覧に空きを作る
        TransformHierarchy* newParent = parent.m_entity.IsValid() ?
            world->GetComponent<TransformHierarchy>(parent.m_entity) : nullptr;
        if (newParent != nullptr)
            newParent->m_children.reserve(newParent->m_children.size() + 1);

        // 逆算に失敗したら旧階層を維持できるよう、リンクの変更前にLocalを確定する
        if (mode == Hestia::ParentMode::KeepWorld &&
            !TransformProcess::WriteWorld(*world, parent.m_entity, transform, hierarchy, position, rotation, scale))
            return false;

        // 旧親と新親の子一覧を更新し、自身の親Handleも付け替える
        if (hierarchy.m_parent.IsValid())
        {
            auto& children = world->GetComponent<TransformHierarchy>(hierarchy.m_parent)->m_children;
            children.erase(std::remove(children.begin(), children.end(), m_entity), children.end());
        }
        if (newParent != nullptr)
            newParent->m_children.push_back(m_entity);
        hierarchy.m_parent = parent.m_entity;

        // 新しい親でWorldを再計算するため、配下のCacheまで無効化する
        TransformProcess::MarkWorldDirty(*world, transform, hierarchy);
        return true;
    }
}
