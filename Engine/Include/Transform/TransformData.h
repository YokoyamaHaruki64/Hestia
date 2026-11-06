/*=============================================================================

 File   : TransformData.h
 Desc   : 必須Transformの正本、描画用Cache、親子関係を保持する

------------------------------------------------------------------------------

 Date   : 2026/11/05
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _TRANSFORM_DATA_H_
#define _TRANSFORM_DATA_H_

#include <vector>

#include "Common/Include/HMath.h"
#include "ECS/Entity.h"

namespace Hestia
{
    /// @brief Local TRSの正本と行列更新状態
    /// @note 書き込みはTransformProcessの操作を通し、子孫へのdirty伝播も行う
    struct Transform
    {
        Vector3 m_localPosition = Vector3::Zero();
        Quaternion m_localRotation = Quaternion::Identity();
        Vector3 m_localScale = Vector3::One();
        Matrix4x4_SIMD m_localMatrix = Matrix4x4_SIMD::Identity();
        bool m_localDirty = false;
        bool m_worldDirty = false;
    };

    /// @brief 描画収集が連続して読む計算済みWorld行列
    struct TransformCache
    {
        Matrix4x4_SIMD m_worldMatrix = Matrix4x4_SIMD::Identity();
    };

    /// @brief Entityを所有しない親子関係。空の親Handleはrootを表す
    /// @note 関係の変更はFacadeのSetParentを通す
    struct TransformHierarchy
    {
        EntityHandle m_parent;
        std::vector<EntityHandle> m_children;
    };
}

#endif // _TRANSFORM_DATA_H_
