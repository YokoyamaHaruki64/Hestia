/*=============================================================================
 File   : [ShaderInc.h]
 Desc   : シェーダー情報の管理を行う構造体の宣言

 ------------------------------------------------------------------------------

 Date   : 2026/03/12
 Author : Yokoyama Haruki

===============================================================================*/

#ifndef _SHADER_INFO_H_
#define _SHADER_INFO_H_

#include <string>
#include <vector>
#include <d3d11.h>
#include <d3d11shader.h>

#include "Graphics/GraphicInc.h"
#include "Common/ValueType.h"


struct ShaderParameterDesc
{
    std::string name{};
    UINT size{ 0 };
    UINT offset{ 0 };
};

enum class ShaderStage
{
    Unknown,
    Vertex,
    Pixel,
    Geometry,
    Hull,
    Domain
};

struct InputSlotBinding
{
    VertexStreamType streamType;
    uint32_t inputSlot;
};

struct ShaderTextureInfo
{
    std::string name{};
    int vsSlot{ -1 };
    int psSlot{ -1 };
};

struct ConstantBufferInfo
{
    std::string name{};
    int vsSlot{ -1 };
    int psSlot{ -1 };
    UINT size{ 0 };
    std::vector<ShaderParameterDesc> parameters{};
};

using PropertyType = ValueType;

enum class MaterialPropertyStorage
{
    ConstantBuffer,
    Resource,
};

inline MaterialPropertyStorage GetMaterialPropertyStorage(PropertyType type)
{
    return type == PropertyType::Texture2D
        ? MaterialPropertyStorage::Resource
        : MaterialPropertyStorage::ConstantBuffer;
}

inline const char* GetPropertyTypeStringForHLSL(PropertyType type)
{
    switch (type)
    {
    case PropertyType::Unknown:
        return "Unknown";
        break;

    case PropertyType::Float:
        return "float";
        break;

    case PropertyType::Int:
        return "int";
        break;

    case PropertyType::Bool:
        return "bool";
        break;

    case PropertyType::Vector2:
        return "float2";
        break;

    case PropertyType::Vector3:
        return "float3";
        break;

    case PropertyType::Vector4:
        return "float4";
        break;

    case PropertyType::Matrix3x3:
        return "float3x3";
        break;

    case PropertyType::Matrix4x4:
        return "float4x4";
        break;

    case PropertyType::Color:
        return "float4";
        break;

    case PropertyType::Texture2D:
        return "Texture2D";
        break;

    default:
        return "Unknown";
    }
}

struct ShaderPropertyDesc
{
    ShaderPropertyID id;
    std::string name;
    std::string displayName;
    PropertyType type{ PropertyType::Unknown };
    MaterialPropertyStorage storage{ MaterialPropertyStorage::ConstantBuffer };
    PropertyValue defaultValue;
    UINT offset{ INVALID_PROPERTY_OFFSET };
    UINT size{ 0 };

    ShaderPropertyDesc(
        ShaderPropertyID id,
        const std::string& name,
        const std::string& displayName,
        PropertyType type,
        PropertyValue defaultValue)
        :
        id(id),
        name(name),
        displayName(displayName),
        type(type),
        storage(GetMaterialPropertyStorage(type)),
        defaultValue(defaultValue)
    {
    }
};

#endif // _SHADER_INFO_H_
