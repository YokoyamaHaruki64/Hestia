/*=============================================================================

 File   : MaterialProperty.h
 Desc   : Material と MaterialShader で共有する Property を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/06
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _MATERIAL_PROPERTY_H_
#define _MATERIAL_PROPERTY_H_

#include <cstdint>
#include <string>
#include <variant>

#include "Asset/AssetHandle.h"
#include "Common/Include/Color.h"
#include "Common/Include/Math/Matrix3x3.h"
#include "Common/Include/Math/Matrix4x4.h"
#include "Common/Include/Math/Vector2.h"
#include "Common/Include/Math/Vector3.h"
#include "Common/Include/Math/Vector4.h"

namespace Hestia
{
    struct Texture;

    using MaterialPropertyID = uint64_t;
    using MaterialPropertyValue = std::variant<
        bool,
        int32_t,
        float,
        Vector2,
        Vector3,
        Vector4,
        Color,
        Matrix3x3,
        Matrix4x4,
        AssetHandle<Texture>
    >;

    /// @brief Shaderの全Passで共有するProperty定義とGPU配置。
    struct MaterialPropertyDesc
    {
        std::string m_name;
        MaterialPropertyValue m_defaultValue;
        uint32_t m_offset = UINT32_MAX;         // 定数バッファ内のオフセット。定数バッファ未使用時は無効。
        uint32_t m_size = 0;                    // Reflection が報告したバイト数。
        uint32_t m_resourceSlot = UINT32_MAX;   // SRV 未使用時は無効。
        bool m_rowMajor = true;                 // 行列の Reflection が row_major を示す場合に設定。-ZprをDXCで指定するため、基本はtrueになる
    };
}

#endif // _MATERIAL_PROPERTY_H_
