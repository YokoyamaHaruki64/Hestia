/*=============================================================================

 File   : IRenderPass.h
 Desc   : 描画Passの処理段階と共通コマンドを定義する

------------------------------------------------------------------------------

 Date   : 2026/11/09
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _I_RENDER_PASS_H_
#define _I_RENDER_PASS_H_

#include "Graphics/DX12Object.h"

#include <span>
#include <tuple>

namespace Hestia
{
    class DX12Frames;
    class DX12Resources;
    class DX12Descriptors;
    class DX12Pipelines;
    class MaterialShader;

    using SortKey = std::tuple<std::uintptr_t, std::size_t, std::uintptr_t, std::uintptr_t, std::size_t>;

    /// @brief Buildで解決済みのSubMeshとShader Passの組
    /// @note 参照先と元の提出列はRecord完了まで有効
    struct DrawCommand
    {
        SortKey m_sortKey;
        const DXMesh* m_mesh;
        const Material* m_material;
        const MaterialShader* m_shader;
        std::size_t m_subMeshIndex;
        std::size_t m_shaderPassIndex;
        std::size_t m_submissionIndex;
    };

    /// @brief 1 Drawで記録するソート済みコマンド列の範囲
    struct DrawBatch
    {
        std::size_t m_first;
        std::size_t m_count;
    };

    /// @brief Recordが現在のフレームで借用するDX12の記録先と資源
    struct RenderPassRecordContext
    {
        ID3D12Device* m_device;
        DX12Frames& m_frames;
        const DX12Resources& m_resources;
        const DX12Descriptors& m_descriptors;
        DX12Pipelines& m_pipelines;
    };

    /// @brief BackendがBuild、Sort、Recordの順で実行する論理的な描画段階
    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;

        /// @brief 対象を選別し、解決済みコマンドとSort Keyを生成する
        virtual void Build(std::span<const RenderData> submissions, const DX12Resources& resources) = 0;

        /// @brief Build結果の順序とバッチ範囲を確定する
        virtual void Sort() = 0;

        /// @brief Buildに渡した提出と確定済みバッチからCommandListへ記録する
        virtual void Record(std::span<const RenderData> submissions, const RenderPassRecordContext& context) = 0;
    };
}

#endif // _I_RENDER_PASS_H_
