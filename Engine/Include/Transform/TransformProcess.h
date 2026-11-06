/*=============================================================================

 File   : TransformProcess.h
 Desc   : Transformのdirty伝播、World更新、座標変換を共有する

------------------------------------------------------------------------------

 Date   : 2026/11/05
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _TRANSFORM_PROCESS_H_
#define _TRANSFORM_PROCESS_H_

#include <cstddef>

#include "Transform/TransformData.h"

namespace HestiaGame { class Transform; }

namespace Hestia
{
    class ECSWorld;

    /// @brief 単一ECSSystemのTransformを処理する非所有の操作群
    class TransformProcess
    {
        friend class HestiaGame::Transform;

    public:
        /// @brief PreRenderでdirtyな行を更新する。inactiveも対象
        static void Run(size_t count, const Entity* entities, Transform* transforms,
            TransformCache* caches, const TransformHierarchy* hierarchies);

        /// @brief Processが借用した列へTRSを書き込み、子孫へdirtyを伝播する
        /// @param transform 単一ECSSystemから現在借用中のTransform
        /// @param hierarchy 同じEntityの現在の親子関係
        /// @param position 有限なLocal位置
        /// @param rotation 有限なLocal Quaternion。正規化して保存する
        /// @param scale 有限なLocal拡縮
        /// @return 成功: true / ECS未接続・入力が無効: false
        /// @pre 参照は同じEntityに属し、構造変更をまたいで保持していない
        static bool SetLocalTRS(Transform& transform, const TransformHierarchy& hierarchy,
            const Vector3& position, const Quaternion& rotation, const Vector3& scale);

    private:
        static ECSWorld* Resolve(EntityHandle handle) noexcept;
        static void MarkWorldDirty(ECSWorld& world, Transform& transform,
            const TransformHierarchy& hierarchy);
        static void UpdateWorld(ECSWorld& world, Transform& transform,
            TransformCache& cache, const TransformHierarchy& hierarchy);
        static const Matrix4x4_SIMD& EnsureWorld(ECSWorld& world, EntityHandle handle);
        static Quaternion WorldRotation(ECSWorld& world, EntityHandle handle);
        static Vector3 WorldScale(ECSWorld& world, EntityHandle handle);
        static Vector3 ScaleFactors(ECSWorld& world, EntityHandle parent,
            const Quaternion& localRotation, const Quaternion& worldRotation);
        static Vector3 LocalPosition(const Matrix4x4_SIMD& parentWorld,
            const Vector3& oldLocal, const Vector3& worldPosition);
        static bool WriteLocal(ECSWorld& world, Transform& transform,
            const TransformHierarchy& hierarchy, const Vector3& position,
            const Quaternion& rotation, const Vector3& scale);
        static bool WriteWorld(ECSWorld& world, EntityHandle parent, Transform& transform,
            const TransformHierarchy& hierarchy, const Vector3& position,
            const Quaternion& rotation, const Vector3& scale);
        static bool DecomposeWorld(const Matrix4x4_SIMD& matrix,
            const Quaternion& fallback, Vector3& position, Quaternion& rotation, Vector3& scale);
    };
}

#endif // _TRANSFORM_PROCESS_H_
