/*=============================================================================

 File   : DX12MaterialPipeline.cpp
 Desc   : MaterialShader の Pass に対応する DX12 Pipeline State を管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Graphics/DX12Backend.h"

#include "Asset/MaterialShader.h"
#include "D3DX12/d3dx12.h"
#include "Log/Logger.h"

namespace Hestia
{
    ID3D12PipelineState* DX12Backend::GetMaterialPipeline(const MaterialShader& shader, std::size_t passIndex)
    {
        // 登録済み Shader の Forward Pass に対応するキャッシュを検索
        const auto handle = shader.GetHandle();
        const auto passes = shader.GetPasses();
        if (!m_Device || !m_RootSignature || !handle.IsValid() || passIndex >= passes.size())
            return nullptr;

        const auto& pass = passes[passIndex];
        if (pass.m_info.m_renderType != RenderType::Forward)
            return nullptr;

        const uint64_t cacheKey = (static_cast<uint64_t>(handle.m_generation) << 32) | handle.m_index;
        const auto cached = m_materialPipelines.find(cacheKey);
        if (cached != m_materialPipelines.end() && cached->second[passIndex])
            return cached->second[passIndex].Get();

        const auto& pipeline = pass.m_pipeline;
        if (pipeline.m_vertexShader.empty() || pipeline.m_pixelShader.empty())
            return nullptr;

        // 要求 Stream の既存レイアウトを抽出。NormalTangent の場合は同一 Slot の両要素を追加
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
        inputElements.reserve(m_InputLayoutDesc.NumElements);
        for (UINT index = 0; index < m_InputLayoutDesc.NumElements; ++index)
        {
            const auto& element = m_InputLayoutDesc.pInputElementDescs[index];
            if (std::ranges::find(pipeline.m_requiredVertexStreams, static_cast<VertexStream>(element.InputSlot)) !=
                pipeline.m_requiredVertexStreams.end())
                inputElements.push_back(element);
        }

        // 現行の Root Signature と Forward 描画ターゲットに Shader を接続
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = m_RootSignature.Get();
        desc.VS = { pipeline.m_vertexShader.data(), pipeline.m_vertexShader.size() };
        desc.PS = { pipeline.m_pixelShader.data(), pipeline.m_pixelShader.size() };
        desc.InputLayout = { inputElements.data(), static_cast<UINT>(inputElements.size()) };
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.SampleMask = UINT_MAX;
        desc.SampleDesc.Count = 1;

        // Blend の指定を色と Alpha の合成係数へ反映
        desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        auto& blend = desc.BlendState.RenderTarget[0];
        switch (pipeline.m_blend)
        {
        case BlendMode::None:
            break;
        case BlendMode::Alpha:
            blend.BlendEnable = TRUE;
            blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
            blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            break;
        case BlendMode::Additive:
            blend.BlendEnable = TRUE;
            blend.DestBlend = D3D12_BLEND_ONE;
            blend.DestBlendAlpha = D3D12_BLEND_ONE;
            break;
        }

        // Rasterizer の描画方式、カリング面、深度クリップを設定
        desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        desc.RasterizerState.FillMode = pipeline.m_rasterizer.m_fill == FillMode::Wireframe
            ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.DepthClipEnable = pipeline.m_rasterizer.m_depthClip;
        switch (pipeline.m_rasterizer.m_cull)
        {
        case CullMode::None:  desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; break;
        case CullMode::Front: desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT; break;
        case CullMode::Back:  desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK; break;
        }

        // ZTest が Off でも ZWrite が On なら比較を常に通して深度書き込みを維持
        const auto& depth = pipeline.m_depthStencil;
        desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        desc.DepthStencilState.DepthEnable = depth.m_depthTest || depth.m_depthWrite;
        desc.DepthStencilState.DepthWriteMask = depth.m_depthWrite
            ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        if (depth.m_depthTest)
        {
            switch (depth.m_depthFunc)
            {
            case CompareFunc::Less:         desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS; break;
            case CompareFunc::LessEqual:    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; break;
            case CompareFunc::Greater:      desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER; break;
            case CompareFunc::GreaterEqual: desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL; break;
            case CompareFunc::Equal:        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL; break;
            case CompareFunc::NotEqual:     desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL; break;
            case CompareFunc::Always:       desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS; break;
            }
        }

        // IA の描画命令が使用する PrimitiveTopology の種別を設定
        switch (pipeline.m_topology)
        {
        case PrimitiveTopologyType::Triangle: desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
        case PrimitiveTopologyType::Line:     desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; break;
        case PrimitiveTopologyType::Point:    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; break;
        }

        // 生成が成功した PSO を Shader の Pass 番号に対応付ける
        ComPtr<ID3D12PipelineState> state;
        const HRESULT result = m_Device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(state.GetAddressOf()));
        if (FAILED(result))
        {
            Logger::Error("Failed to create MaterialShader pipeline state for " + std::string(shader.GetName()) +
                " pass " + std::to_string(passIndex));
            return nullptr;
        }
        auto& states = m_materialPipelines.try_emplace(cacheKey, passes.size()).first->second;
        states[passIndex] = std::move(state);
        return states[passIndex].Get();
    }
}
