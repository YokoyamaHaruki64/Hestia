/*=============================================================================

 File   : DX12Pipelines.h
 Desc   : Root Signature、Input Layout と PSO を管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _DX12_PIPELINES_H_
#define _DX12_PIPELINES_H_

#include "Graphics/DX12Object.h"
#include <unordered_map>

namespace Hestia
{
    class MaterialShader;

    /// @brief 共通 Root Signature と MaterialShader の Pass ごとの PSO を保持する。
    class DX12Pipelines
    {
    public:
        bool Initialize(ID3D12Device* device);
        void Finalize();
        ID3D12PipelineState* GetMaterialPipeline(ID3D12Device* device, const MaterialShader& shader, std::size_t passIndex);
        ID3D12RootSignature* GetRootSignature() const { return m_RootSignature.Get(); }

    private:
        ComPtr<ID3D12RootSignature> m_RootSignature;
        std::unordered_map<uint64_t, std::vector<ComPtr<ID3D12PipelineState>>> m_materialPipelines;
        D3D12_INPUT_LAYOUT_DESC m_InputLayoutDesc{};
    };
}

#endif // _DX12_PIPELINES_H_
