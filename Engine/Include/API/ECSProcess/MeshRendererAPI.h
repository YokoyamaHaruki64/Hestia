/*=============================================================================

 File   : MeshRendererAPI.h
 Desc   : MeshRenderer の設定を Game へ公開する。

------------------------------------------------------------------------------

 Date   : 2026/11/16
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _MESH_RENDERER_API_H_
#define _MESH_RENDERER_API_H_

#include <cassert>
#include <cstddef>

#include "Common/Include/EntityHandle.h"
#include "Common/Include/Math/Vector4.h"
#include "Engine/Include/Asset/AssetHandle.h"

namespace Hestia
{
    struct Mesh;
    class Material;

    /// @brief EntityHandle で MeshRenderer を操作する関数テーブル。
    /// @note 対象不在・破棄予約済みの Setter は false、Asset の Getter は無効 Handle を返す。
    struct MeshRendererAPI
    {
        bool (*IsValid)(EntityHandle entity) noexcept;
        bool (*IsActive)(EntityHandle entity) noexcept;
        bool (*SetActive)(EntityHandle entity, bool active) noexcept;
        AssetHandle<Mesh> (*GetMesh)(EntityHandle entity) noexcept;
        bool (*SetMesh)(EntityHandle entity, AssetHandle<Mesh> mesh);
        std::size_t (*GetMaterialCount)(EntityHandle entity) noexcept;
        AssetHandle<Material> (*GetMaterial)(EntityHandle entity, std::size_t index) noexcept;
        bool (*SetMaterial)(EntityHandle entity, std::size_t index, AssetHandle<Material> material);
        Vector4 (*GetInstanceColor)(EntityHandle entity) noexcept;
        bool (*SetInstanceColor)(EntityHandle entity, const Vector4& color) noexcept;
    };

#if defined(HESTIA_ENGINE_INTERNAL)
    /// @brief MeshRenderer の公開 API を Engine 側の操作に接続する。
    class MeshRendererAPIImpl
    {
    public:
        static MeshRendererAPI CreateAPI() noexcept;
    };
#endif
}

#endif // _MESH_RENDERER_API_H_
