/*=============================================================================

 File   : DX12Draw.cpp
 Desc   : 描画提出から GPU の Draw を記録する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Graphics/DX12Draw.h"

#include "Graphics/DX12Frames.h"
#include "Graphics/DX12Resources.h"
#include "Graphics/DX12Descriptors.h"
#include "Graphics/DX12Pipelines.h"
#include "Asset/AssetSystem.h"
#include "Asset/Material.h"
#include "Asset/MaterialShader.h"
#include <array>
#include <cstring>

using namespace Hestia;

bool DX12Draw::CreateConstants(ID3D12Device* device)
{
    HRESULT hr;
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC cbDesc{};
    cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDesc.Alignment = 0;
    cbDesc.Height = 1;
    cbDesc.DepthOrArraySize = 1;
    cbDesc.MipLevels = 1;
    cbDesc.Format = DXGI_FORMAT_UNKNOWN;
    cbDesc.SampleDesc.Count = 1;
    cbDesc.SampleDesc.Quality = 0;
    cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // PerObject ConstantBufferの作成
    cbDesc.Width = m_PerObjectCB.SIZE * DX12_MAX_OBJECT_COUNT * DX12_FRAME_COUNT;
    hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &cbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_PerObjectCB.constantBuffer.GetAddressOf()));
    if (!CheckDX12Result(hr, L"Failed to create PerObject ConstantBuffer.")) return false;

    // PerObject ConstantBufferのマッピングを保持
    if (!CheckDX12Result(m_PerObjectCB.constantBuffer->Map(0, nullptr, &m_PerObjectCB.mappedData), L"Failed to map PerObject CB.")) return false;

    return true;
}

void DX12Draw::Finalize()
{
    // Backend が GPU 待機を済ませてから永続マップを解除する。
    if (m_PerObjectCB.constantBuffer && m_PerObjectCB.mappedData)
        m_PerObjectCB.constantBuffer->Unmap(0, nullptr);
    m_PerObjectCB.mappedData = nullptr;
    m_PerObjectCB.constantBuffer.Reset();
    m_currentFrameData.clear();
}

void DX12Draw::Draw(ID3D12Device* device, DX12Frames& frames, const DX12Resources& resources,
    const DX12Descriptors& descriptors, DX12Pipelines& pipelines)
{
    // Forward Pass を描画 Phase 順に実行し、同じ Phase 内では提出順を維持
    for (const RenderPhase phase : { RenderPhase::Opaque, RenderPhase::Masked, RenderPhase::Transparent })
    {
        for (std::size_t index = 0; index < m_currentFrameData.size(); ++index)
        {
            const auto& data = m_currentFrameData[index];
            if (data.materials.empty())
                continue;
            if (const auto* mesh = resources.GetMesh(data.mesh))
                DrawMaterialMesh(device, frames, resources, descriptors, pipelines, data, *mesh, index, phase);
        }
    }
}

void DX12Draw::DrawMaterialMesh(ID3D12Device* device, DX12Frames& frames, const DX12Resources& resources,
    const DX12Descriptors& descriptors, DX12Pipelines& pipelines,
    const RenderData& data, const DXMesh& mesh, std::size_t objectIndex,
    RenderPhase phase)
{
    for (const auto& subMesh : mesh.m_subMeshes)
    {
        // SubMesh の Slot を登録済み Material に解決
        if (subMesh.m_materialSlot >= data.materials.size())
            continue;
        const auto* material = AssetSystem::Get(data.materials[subMesh.m_materialSlot]);
        const auto* shader = material ? material->GetShader() : nullptr;
        if (!shader)
            continue;

        // 現在の Phase に属する Pass だけを描画し、各 Draw に独立した Material 領域を使う
        const auto passes = shader->GetPasses();
        for (std::size_t index = 0; index < passes.size(); ++index)
        {
            const auto& pass = passes[index];
            if (pass.m_info.m_renderType != RenderType::Forward || pass.m_info.m_renderPhase != phase)
                continue;
            if (!frames.CanCommitMaterialTextures())
                return;

            auto* pipeline = pipelines.GetMaterialPipeline(device, *shader, index);
            if (!pipeline || !BindMaterial(device, frames, resources, descriptors, *material, *shader))
                continue;

            BindObject(frames, data.worldMatrix, objectIndex);
            frames.GetCommandList()->SetPipelineState(pipeline);
            auto topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            if (pass.m_pipeline.m_topology == PrimitiveTopologyType::Line)
                topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            else if (pass.m_pipeline.m_topology == PrimitiveTopologyType::Point)
                topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;

            DrawMesh(frames, resources, mesh, subMesh, topology, pass.m_pipeline.m_requiredVertexStreams);
        }
    }
}

void DX12Draw::BindObject(DX12Frames& frames, const Matrix4x4_SIMD& worldMatrix, size_t objectIndex)
{
    // 各 Frame が独立した Object 定数の領域を持ち、他の Frame の Draw を上書きしない。
    const int index = frames.Index() * DX12_MAX_OBJECT_COUNT + objectIndex;

    size_t offset = m_PerObjectCB.SIZE * index;

    // DataをPerObject ConstantBufferに書き込む
    memcpy(
        reinterpret_cast<uint8_t*>(m_PerObjectCB.mappedData) + offset,
        &worldMatrix,
        sizeof(Matrix4x4_SIMD));

    // GPUAddressを設定
    frames.GetCommandList()
        ->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(DX12RootParameterIndex::PerObjectCBV),
            m_PerObjectCB.constantBuffer->GetGPUVirtualAddress() + offset);
}

bool DX12Draw::BindMaterial(ID3D12Device* device, DX12Frames& frames, const DX12Resources& resources,
    const DX12Descriptors& descriptors, const Material& material, const MaterialShader& shader)
{
    // Reflection のスロットに Texture を割り当て、空きは白 Texture で埋める。
    const auto address = frames.UploadMaterialConstants(device, material.GetConstantBuffer());
    if (address == 0)
        return false;
    frames.GetCommandList()->SetGraphicsRootConstantBufferView(
        static_cast<UINT>(DX12RootParameterIndex::PerMaterialCBV), address);

    std::array<int, MAX_MATERIAL_TEXTURE> slots;
    slots.fill(static_cast<int>(resources.WhiteTexture().m_masterSRVIndex));
    for (const auto& property : shader.GetProperties())
    {
        if (property.m_resourceSlot >= MAX_MATERIAL_TEXTURE)
            continue;
        if (const auto* texture = resources.GetTexture(material.GetTexture(property.m_name)))
            slots[property.m_resourceSlot] = static_cast<int>(texture->m_masterSRVIndex);
    }

    frames.CommitMaterialTextures(device, descriptors, slots);
    return true;
}

void DX12Draw::DrawMesh(DX12Frames& frames, const DX12Resources& resources, const DXMesh& mesh, const SubMesh& range, D3D_PRIMITIVE_TOPOLOGY topology,
    std::span<const VertexStream> streams)
{
    std::array<D3D12_VERTEX_BUFFER_VIEW, static_cast<size_t>(VertexStream::Count)> views{};

    // 要求された Stream のスロットだけを埋め、Mesh にないものは共通バッファで補う
    for (const VertexStream vertexStream : streams)
    {
        const auto i = static_cast<std::size_t>(vertexStream);
        views[i] = resources.GetVertexBuffer(mesh, vertexStream).m_view;

    }

    auto* commandList = frames.GetCommandList();
    commandList->IASetPrimitiveTopology(topology);
    commandList->IASetVertexBuffers(0, static_cast<UINT>(views.size()), views.data());
    commandList->IASetIndexBuffer(&mesh.m_indexView);
    commandList->DrawIndexedInstanced(range.m_indexCount, 1, range.m_firstIndex, 0, 0);
}
