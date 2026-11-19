/*=============================================================================

 File   : TransformAPI.h
 Desc   : Transform の共有 API と Engine 内の個別操作を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/16
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _TRANSFORM_API_H_
#define _TRANSFORM_API_H_

#include "Common/Include/EntityHandle.h"
#include "Common/Include/HMath.h"

namespace Hestia
{
    /// @brief 親変更時に維持する座標系
    enum class ParentMode
    {
        KeepWorld,
        KeepLocal,
    };

    /// @brief Engineが提供するTransform操作の関数テーブル
    /// @note Handleと数学型を使い、ECS内の配置はEngine側で解決する
    struct TransformAPI
    {
        bool (*IsValid)(EntityHandle entity) noexcept;
        Vector3 (*GetLocalPosition)(EntityHandle entity);
        Quaternion (*GetLocalRotation)(EntityHandle entity);
        Vector3 (*GetLocalScale)(EntityHandle entity);
        bool (*SetLocalPosition)(EntityHandle entity, const Vector3& position);
        bool (*SetLocalRotation)(EntityHandle entity, const Quaternion& rotation);
        bool (*SetLocalScale)(EntityHandle entity, const Vector3& scale);
        Vector3 (*GetWorldPosition)(EntityHandle entity);
        Quaternion (*GetWorldRotation)(EntityHandle entity);
        Vector3 (*GetWorldScale)(EntityHandle entity);
        Matrix4x4_SIMD (*GetWorldMatrix)(EntityHandle entity);
        bool (*SetWorldPosition)(EntityHandle entity, const Vector3& position);
        bool (*SetWorldRotation)(EntityHandle entity, const Quaternion& rotation);
        bool (*SetWorldScale)(EntityHandle entity, const Vector3& scale);
        bool (*SetWorldMatrix)(EntityHandle entity, const Matrix4x4_SIMD& matrix);
        EntityHandle (*GetParent)(EntityHandle entity);
        bool (*SetParent)(EntityHandle entity, EntityHandle parent, ParentMode mode);
    };

#if defined(HESTIA_ENGINE_INTERNAL)
    class ECSWorld;

    /// @brief EntityHandle から対象を解決し、Transform の個別操作を実行する。
    class TransformAPIImpl
    {
    public:
        /// @brief Game に渡す操作を Engine 側の実装へ接続する。
        static TransformAPI CreateAPI() noexcept;

    private:
        static bool IsValid(EntityHandle entity) noexcept;
        static ECSWorld* ResolveForGet(EntityHandle entity);
        static ECSWorld* ResolveForSet(EntityHandle entity);
        static Vector3 GetLocalPosition(EntityHandle entity);
        static Quaternion GetLocalRotation(EntityHandle entity);
        static Vector3 GetLocalScale(EntityHandle entity);
        static bool SetLocalPosition(EntityHandle entity, const Vector3& position);
        static bool SetLocalRotation(EntityHandle entity, const Quaternion& rotation);
        static bool SetLocalScale(EntityHandle entity, const Vector3& scale);
        static Matrix4x4_SIMD GetWorldMatrix(EntityHandle entity);
        static Vector3 GetWorldPosition(EntityHandle entity);
        static Quaternion GetWorldRotation(EntityHandle entity);
        static Vector3 GetWorldScale(EntityHandle entity);
        static bool SetWorldPosition(EntityHandle entity, const Vector3& position);
        static bool SetWorldRotation(EntityHandle entity, const Quaternion& rotation);
        static bool SetWorldScale(EntityHandle entity, const Vector3& scale);
        static bool SetWorldMatrix(EntityHandle entity, const Matrix4x4_SIMD& matrix);
        static EntityHandle GetParent(EntityHandle entity);
        static bool SetParent(EntityHandle entity, EntityHandle parent, Hestia::ParentMode mode);
    };
#endif
}

#endif // _TRANSFORM_API_H_
