/*=============================================================================

 File   : TransformAPI.cpp
 Desc   : Transform の個別操作を Engine.dll 内の ECS へ接続する。

------------------------------------------------------------------------------

 Date   : 2026/11/16
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "API/ECSProcess/TransformAPI.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "ECS/ECSWorld.h"
#include "Log/Logger.h"
#include "Transform/TransformProcess.h"

namespace Hestia
{
    // Facadeの呼び出し元に関わらず、Entityの解決と状態変更はEngineに集約
    TransformAPI TransformAPIImpl::CreateAPI() noexcept
    {
        return {
            .IsValid = &IsValid,
            .GetLocalPosition = &GetLocalPosition,
            .GetLocalRotation = &GetLocalRotation,
            .GetLocalScale = &GetLocalScale,
            .SetLocalPosition = &SetLocalPosition,
            .SetLocalRotation = &SetLocalRotation,
            .SetLocalScale = &SetLocalScale,
            .GetWorldPosition = &GetWorldPosition,
            .GetWorldRotation = &GetWorldRotation,
            .GetWorldScale = &GetWorldScale,
            .GetWorldMatrix = &GetWorldMatrix,
            .SetWorldPosition = &SetWorldPosition,
            .SetWorldRotation = &SetWorldRotation,
            .SetWorldScale = &SetWorldScale,
            .SetWorldMatrix = &SetWorldMatrix,
            .GetParent = &GetParent,
            .SetParent = &SetParent,
        };
    }

    bool TransformAPIImpl::IsValid(EntityHandle entity) noexcept
    {
        return TransformProcess::Resolve(entity) != nullptr;
    }

    ECSWorld* TransformAPIImpl::ResolveForGet(EntityHandle entity)
    {
        ECSWorld* world = TransformProcess::Resolve(entity);
        if (world == nullptr)
        {
            // Debugで停止する前に理由を記録し、assert無効時はGetterの既定値に委ねる
            Logger::Error("Transform Get requires a live Entity");
            assert(world != nullptr && "Transform Get requires a live Entity");
        }
        return world;
    }

    ECSWorld* TransformAPIImpl::ResolveForSet(EntityHandle entity)
    {
        ECSWorld* world = TransformProcess::Resolve(entity);
        if (world == nullptr)
            Logger::Error("Transform Set requires a live Entity");
        return world;
    }

    Vector3 TransformAPIImpl::GetLocalPosition(EntityHandle entity)
    {
        ECSWorld* world = ResolveForGet(entity);
        return world != nullptr ? world->GetComponent<Transform>(entity)->m_localPosition :
            Vector3::Zero();
    }

    Quaternion TransformAPIImpl::GetLocalRotation(EntityHandle entity)
    {
        ECSWorld* world = ResolveForGet(entity);
        return world != nullptr ? world->GetComponent<Transform>(entity)->m_localRotation :
            Quaternion::Identity();
    }

    Vector3 TransformAPIImpl::GetLocalScale(EntityHandle entity)
    {
        ECSWorld* world = ResolveForGet(entity);
        return world != nullptr ? world->GetComponent<Transform>(entity)->m_localScale :
            Vector3::One();
    }

    bool TransformAPIImpl::SetLocalPosition(EntityHandle entity, const Vector3& position)
    {
        ECSWorld* world = ResolveForSet(entity);
        if (world == nullptr)
            return false;

        auto& transform = *world->GetComponent<Transform>(entity);
        return TransformProcess::SetLocalTRS(transform, *world->GetComponent<TransformHierarchy>(entity),
            position, transform.m_localRotation, transform.m_localScale);
    }

    bool TransformAPIImpl::SetLocalRotation(EntityHandle entity, const Quaternion& rotation)
    {
        ECSWorld* world = ResolveForSet(entity);
        if (world == nullptr)
            return false;

        auto& transform = *world->GetComponent<Transform>(entity);
        return TransformProcess::SetLocalTRS(transform, *world->GetComponent<TransformHierarchy>(entity),
            transform.m_localPosition, rotation, transform.m_localScale);
    }

    bool TransformAPIImpl::SetLocalScale(EntityHandle entity, const Vector3& scale)
    {
        ECSWorld* world = ResolveForSet(entity);
        if (world == nullptr)
            return false;

        auto& transform = *world->GetComponent<Transform>(entity);
        return TransformProcess::SetLocalTRS(transform, *world->GetComponent<TransformHierarchy>(entity),
            transform.m_localPosition, transform.m_localRotation, scale);
    }

    Matrix4x4_SIMD TransformAPIImpl::GetWorldMatrix(EntityHandle entity)
    {
        ECSWorld* world = ResolveForGet(entity);
        return world != nullptr ? TransformProcess::EnsureWorld(*world, entity) :
            Matrix4x4_SIMD::Identity();
    }

    Vector3 TransformAPIImpl::GetWorldPosition(EntityHandle entity)
    {
        const Matrix4x4_SIMD matrix = GetWorldMatrix(entity);
        return { matrix[3][0], matrix[3][1], matrix[3][2] };
    }

    Quaternion TransformAPIImpl::GetWorldRotation(EntityHandle entity)
    {
        ECSWorld* world = ResolveForGet(entity);
        if (world == nullptr)
            return Quaternion::Identity();

        // 行列Cacheを更新し、回転は拡縮の影響を受けないQuaternionの合成値から取る
        TransformProcess::EnsureWorld(*world, entity);
        return TransformProcess::WorldRotation(*world, entity);
    }

    Vector3 TransformAPIImpl::GetWorldScale(EntityHandle entity)
    {
        ECSWorld* world = ResolveForGet(entity);
        if (world == nullptr)
            return Vector3::One();

        // 行列Cacheを更新し、親の変換を反映した基底から拡縮を求める
        TransformProcess::EnsureWorld(*world, entity);
        return TransformProcess::WorldScale(*world, entity);
    }

    bool TransformAPIImpl::SetWorldPosition(EntityHandle entity, const Vector3& position)
    {
        // 有効な対象と有限な入力だけを座標変換へ渡す
        ECSWorld* world = ResolveForSet(entity);
        if (world == nullptr)
            return false;

        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
        {
            Logger::Error("Transform received a non-finite World position");
            return false;
        }

        // 親の最新Worldから位置を逆算し、回転と拡縮はそのまま残す
        auto& transform = *world->GetComponent<Transform>(entity);
        const auto& hierarchy = *world->GetComponent<TransformHierarchy>(entity);
        const Matrix4x4_SIMD parentWorld = hierarchy.m_parent.IsValid() ?
            TransformProcess::EnsureWorld(*world, hierarchy.m_parent) : Matrix4x4_SIMD::Identity();
        const Vector3 local = TransformProcess::LocalPosition(parentWorld, transform.m_localPosition, position);
        return TransformProcess::WriteLocal(*world, transform, hierarchy, local,
            transform.m_localRotation, transform.m_localScale);
    }

    bool TransformAPIImpl::SetWorldRotation(EntityHandle entity, const Quaternion& rotation)
    {
        // 対象と入力を検証してから、Quaternionを正規化する
        ECSWorld* world = ResolveForSet(entity);
        if (world == nullptr)
            return false;

        if (!std::isfinite(rotation.x) || !std::isfinite(rotation.y) ||
            !std::isfinite(rotation.z) || !std::isfinite(rotation.w))
        {
            Logger::Error("Transform received a non-finite World rotation");
            return false;
        }

        // 親の回転を取り除き、Localの位置と拡縮を保持する
        auto& transform = *world->GetComponent<Transform>(entity);
        const auto& hierarchy = *world->GetComponent<TransformHierarchy>(entity);
        const Quaternion parent = hierarchy.m_parent.IsValid() ?
            TransformProcess::WorldRotation(*world, hierarchy.m_parent) : Quaternion::Identity();
        return TransformProcess::WriteLocal(*world, transform, hierarchy, transform.m_localPosition,
            rotation.Normalized() * parent.Inversed(), transform.m_localScale);
    }

    bool TransformAPIImpl::SetWorldScale(EntityHandle entity, const Vector3& scale)
    {
        // 非有限値がLocalへ伝わらないよう、変換前に入力を検証する
        ECSWorld* world = ResolveForSet(entity);
        if (world == nullptr)
            return false;

        if (!std::isfinite(scale.x) || !std::isfinite(scale.y) || !std::isfinite(scale.z))
        {
            Logger::Error("Transform received a non-finite World scale");
            return false;
        }

        // 現在の回転を基準に、親の変換が各軸に与える倍率を求める
        auto& transform = *world->GetComponent< Transform>(entity);
        const auto& hierarchy = *world->GetComponent<TransformHierarchy>(entity);
        const Vector3 factors = TransformProcess::ScaleFactors(*world, hierarchy.m_parent,
            transform.m_localRotation, TransformProcess::WorldRotation(*world, entity));

        // 親の拡縮が戻ったときに使えるよう、逆算できない軸のLocal値を残す
        const Vector3 local = {
            factors.x == 0.0f ? transform.m_localScale.x : scale.x / factors.x,
            factors.y == 0.0f ? transform.m_localScale.y : scale.y / factors.y,
            factors.z == 0.0f ? transform.m_localScale.z : scale.z / factors.z };
        return TransformProcess::WriteLocal(*world, transform, hierarchy, transform.m_localPosition,
            transform.m_localRotation, local);
    }

    bool TransformAPIImpl::SetWorldMatrix(EntityHandle entity, const Matrix4x4_SIMD& matrix)
    {
        ECSWorld* world = ResolveForSet(entity);
        if (world == nullptr)
            return false;

        // 入力をWorld TRSへ近似し、座標変換に使えない行列は書き込み前に拒否する
        Vector3 position;
        Quaternion rotation;
        Vector3 scale;
        if (!TransformProcess::DecomposeWorld(matrix, TransformProcess::WorldRotation(*world, entity),
            position, rotation, scale))
        {
            Logger::Error("Transform requires a finite affine World matrix");
            return false;
        }

        // 現在の親を基準にLocalへ変換して、TRSの正本を確定する
        return TransformProcess::WriteWorld(*world,
            world->GetComponent<TransformHierarchy>(entity)->m_parent,
            *world->GetComponent<Transform>(entity),
            *world->GetComponent<TransformHierarchy>(entity), position, rotation, scale);
    }

    EntityHandle TransformAPIImpl::GetParent(EntityHandle entity)
    {
        ECSWorld* world = ResolveForGet(entity);
        return world != nullptr ? world->GetComponent<TransformHierarchy>(entity)->m_parent :
            EntityHandle{};
    }

    bool TransformAPIImpl::SetParent(EntityHandle entity, EntityHandle parent, ParentMode mode)
    {
        // 自身と新しい親の生存状態を確認する。空の親は解除として扱う
        ECSWorld* world = ResolveForSet(entity);
        if (world == nullptr)
            return false;

        if ((mode != ParentMode::KeepWorld && mode != ParentMode::KeepLocal) ||
            (parent != EntityHandle{} && TransformProcess::Resolve(parent) != world))
        {
            Logger::Error("Transform received an invalid parent or ParentMode");
            return false;
        }

        // 新しい親から祖先を辿り、自身が見つかれば循環になるため拒否する
        for (EntityHandle ancestor = parent; ancestor.IsValid();
            ancestor = world->GetComponent<TransformHierarchy>(ancestor)->m_parent)
        {
            if (ancestor == entity)
            {
                Logger::Error("Transform parent would form a cycle");
                return false;
            }
        }

        auto& hierarchy = *world->GetComponent<TransformHierarchy>(entity);
        if (hierarchy.m_parent == parent)
            return true;

        // KeepWorldでは、付け替える前の階層で維持するTRSを取得する
        auto& transform = *world->GetComponent<Transform>(entity);
        Vector3 position;
        Quaternion rotation;
        Vector3 scale;
        if (mode == ParentMode::KeepWorld)
        {
            position = GetWorldPosition(entity);
            rotation = TransformProcess::WorldRotation(*world, entity);
            scale = TransformProcess::WorldScale(*world, entity);
        }

        // 確保の失敗で変更が途中に残らないよう、新しい親の子一覧に空きを作る
        TransformHierarchy* newParent = parent.IsValid() ?
            world->GetComponent<TransformHierarchy>(parent) : nullptr;
        if (newParent != nullptr)
            newParent->m_children.reserve(newParent->m_children.size() + 1);

        // 逆算に失敗したら旧階層を維持できるよう、リンクの変更前にLocalを確定する
        if (mode == ParentMode::KeepWorld &&
            !TransformProcess::WriteWorld(*world, parent, transform, hierarchy, position, rotation, scale))
            return false;

        // 旧親と新親の子一覧を更新し、自身の親Handleも付け替える
        if (hierarchy.m_parent.IsValid())
        {
            auto& children = world->GetComponent<TransformHierarchy>(hierarchy.m_parent)->m_children;
            children.erase(std::remove(children.begin(), children.end(), entity), children.end());
        }
        if (newParent != nullptr)
            newParent->m_children.push_back(entity);
        hierarchy.m_parent = parent;

        // 新しい親でWorldを再計算するため、配下のCacheまで無効化する
        TransformProcess::MarkWorldDirty(*world, transform, hierarchy);
        return true;
    }
}
