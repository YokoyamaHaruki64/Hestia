/*=============================================================================

 File   : ForwardRenderPass.cpp
 Desc   : 描画提出から GPU の Draw を記録する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Graphics/ForwardRenderPass.h"

#include "Graphics/DX12Frames.h"
#include "Graphics/DX12Resources.h"
#include "Graphics/DX12Descriptors.h"
#include "Graphics/DX12Pipelines.h"
#include "Graphics/DX12Support.h"
#include "Asset/Material.h"
#include "Asset/MaterialShader.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <tuple>

using namespace Hestia;

bool ForwardRenderPass::CreateConstants(ID3D12Device* device)
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

void ForwardRenderPass::Finalize()
{
    // Backend が GPU 待機を済ませてから永続マップを解除する。
    if (m_PerObjectCB.constantBuffer && m_PerObjectCB.mappedData)
        m_PerObjectCB.constantBuffer->Unmap(0, nullptr);
    m_PerObjectCB.mappedData = nullptr;
    m_PerObjectCB.constantBuffer.Reset();
    ClearFrame();
}

void ForwardRenderPass::ClearFrame()
{
    for (auto& phase : m_phaseDraws)
    {
        phase.m_commands.clear();
        phase.m_batches.clear();
    }
}

void ForwardRenderPass::Build(std::span<const RenderData> submissions, const DX12Resources& resources)
{
    // Phase の値をそのまま配列添字に使い、提出順を保ったまま描画順へ振り分ける
    static_assert(static_cast<std::size_t>(RenderPhase::Opaque) == 0 &&
        static_cast<std::size_t>(RenderPhase::Masked) == 1 &&
        static_cast<std::size_t>(RenderPhase::Transparent) == 2);
    ClearFrame();

    for (std::size_t submissionIndex = 0; submissionIndex < submissions.size(); ++submissionIndex)
    {
        // 提出ごとに一度だけ GPU Mesh を解決し、全 SubMesh と Pass で共有
        const auto& data = submissions[submissionIndex];
        if (!data.mesh || data.materials.empty())
            continue;
        const auto* mesh = resources.GetMesh(data.mesh->m_handle);
        if (!mesh)
            continue;

        for (std::size_t subMeshIndex = 0; subMeshIndex < mesh->m_subMeshes.size(); ++subMeshIndex)
        {
            // Slot の Material から使える Forward Pass を展開
            const auto& subMesh = mesh->m_subMeshes[subMeshIndex];
            if (subMesh.m_materialSlot >= data.materials.size())
                continue;
            const auto* material = data.materials[subMesh.m_materialSlot];
            const auto* shader = material ? material->GetShader() : nullptr;
            if (!shader)
                continue;

            const auto passes = shader->GetPasses();
            for (std::size_t passIndex = 0; passIndex < passes.size(); ++passIndex)
            {
                const auto& info = passes[passIndex].m_info;
                const auto phaseIndex = static_cast<std::size_t>(info.m_renderPhase);
                if (info.m_renderType != RenderType::Forward || phaseIndex >= m_phaseDraws.size())
                    continue;

                // フレーム内の参照から比較用のキーを一度だけ生成
                const SortKey sortKey{
                    reinterpret_cast<std::uintptr_t>(shader), passIndex,
                    reinterpret_cast<std::uintptr_t>(material),
                    reinterpret_cast<std::uintptr_t>(mesh), subMeshIndex,
                };
                m_phaseDraws[phaseIndex].m_commands.push_back({
                    sortKey, mesh, material, shader, subMeshIndex, passIndex, submissionIndex,
                });
            }
        }
    }
}

void ForwardRenderPass::Sort()
{
    constexpr std::size_t maxInstancesPerDraw = 1024; // 連続Upload領域に収まる件数で分割
    for (std::size_t phaseIndex = 0; phaseIndex < m_phaseDraws.size(); ++phaseIndex)
    {
        // 透明物はブレンド順を維持し、OpaqueとMaskedだけをまとめる
        auto& phase = m_phaseDraws[phaseIndex];
        auto& commands = phase.m_commands;
        const bool canReorder = phaseIndex != static_cast<std::size_t>(RenderPhase::Transparent);
        if (canReorder)
            std::ranges::sort(commands, [](const DrawCommand& left, const DrawCommand& right)
            {
                return std::tie(left.m_sortKey, left.m_submissionIndex) <
                    std::tie(right.m_sortKey, right.m_submissionIndex);
            });

        // Sort Keyの表現にかかわらず、実際の描画状態が一致する連続範囲を確定
        for (std::size_t next = 0; next < commands.size();)
        {
            const auto first = next++;
            const auto& draw = commands[first];
            if (canReorder && draw.m_shader->IsInstancing())
            {
                while (next < commands.size() && next - first < maxInstancesPerDraw)
                {
                    const auto& candidate = commands[next];
                    if (candidate.m_mesh != draw.m_mesh || candidate.m_subMeshIndex != draw.m_subMeshIndex ||
                        candidate.m_material != draw.m_material || candidate.m_shader != draw.m_shader ||
                        candidate.m_shaderPassIndex != draw.m_shaderPassIndex)
                        break;
                    ++next;
                }
            }
            phase.m_batches.push_back({ first, next - first });
        }
    }
}

void ForwardRenderPass::Record(std::span<const RenderData> submissions, const RenderPassRecordContext& context)
{
    auto& frames = context.m_frames;
    const auto& resources = context.m_resources;

    // 確定済みのバッチをPhase順に記録
    for (const auto& phase : m_phaseDraws)
    {
        for (const auto& batch : phase.m_batches)
        {
            if (!frames.CanCommitMaterialTextures())
                return;

            // バッチ共通のPSOとMaterialを設定
            const auto& draw = phase.m_commands[batch.m_first];
            const auto& pass = draw.m_shader->GetPasses()[draw.m_shaderPassIndex];
            auto* pipeline = context.m_pipelines.GetMaterialPipeline(context.m_device, *draw.m_shader, draw.m_shaderPassIndex);
            if (!pipeline || !BindMaterial(context.m_device, frames, resources,
                context.m_descriptors, *draw.m_material, *draw.m_shader))
                continue;

            // 各InstanceのWorld行列と色を、SV_InstanceIDで参照する順に配置
            if (draw.m_shader->IsInstancing())
            {
                m_instances.clear();
                m_instances.reserve(batch.m_count);
                const auto commands = std::span(phase.m_commands).subspan(batch.m_first, batch.m_count);
                for (const auto& command : commands)
                {
                    const auto& data = submissions[command.m_submissionIndex];
                    m_instances.push_back({ data.worldMatrix, data.instanceColor });
                }
                const auto address = frames.UploadDrawData(context.m_device, std::as_bytes(std::span(m_instances)));
                if (address == 0)
                    continue;
                frames.GetCommandList()->SetGraphicsRootShaderResourceView(
                    static_cast<UINT>(DX12RootParameterIndex::InstancesSRV), address);
            }
            else
                BindObject(frames, submissions[draw.m_submissionIndex].worldMatrix, draw.m_submissionIndex);
            frames.GetCommandList()->SetPipelineState(pipeline);

            // Pass のトポロジと Stream で SubMesh の範囲を描画
            auto topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            if (pass.m_pipeline.m_topology == PrimitiveTopologyType::Line)
                topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            else if (pass.m_pipeline.m_topology == PrimitiveTopologyType::Point)
                topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;

            const auto& subMesh = draw.m_mesh->m_subMeshes[draw.m_subMeshIndex];
            DrawMesh(frames, resources, *draw.m_mesh, subMesh, topology, pass.m_pipeline.m_requiredVertexStreams,
                static_cast<UINT>(batch.m_count));
        }
    }
}

void ForwardRenderPass::BindObject(DX12Frames& frames, const Matrix4x4_SIMD& worldMatrix, size_t objectIndex)
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

bool ForwardRenderPass::BindMaterial(ID3D12Device* device, DX12Frames& frames, const DX12Resources& resources,
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

void ForwardRenderPass::DrawMesh(DX12Frames& frames, const DX12Resources& resources, const DXMesh& mesh, const SubMesh& range, D3D_PRIMITIVE_TOPOLOGY topology,
    std::span<const VertexStream> streams, UINT instanceCount)
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
    commandList->DrawIndexedInstanced(range.m_indexCount, instanceCount, range.m_firstIndex, 0, 0);
}
