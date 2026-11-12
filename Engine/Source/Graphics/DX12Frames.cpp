/*=============================================================================

 File   : DX12Frames.cpp
 Desc   : フレーム記録の開始、送信、定数領域を管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Graphics/DX12Frames.h"

#include "Graphics/DX12Device.h"
#include "Graphics/DX12Descriptors.h"
#include "Graphics/DX12Presentation.h"
#include "Graphics/DX12Pipelines.h"
#include "Graphics/DX12Resources.h"
#include "Asset/Material.h"
#include "Asset/MaterialShader.h"
#include "Time/TimeSystem.h"
#include "Log/Logger.h"
#include "D3DX12/d3dx12.h"
#include <cstring>

using namespace Hestia;

namespace
{
    constexpr UINT Align256(UINT size) { return (size + 255) & ~255u; }
}

bool DX12Frames::CreateVisibleHeaps(ID3D12Device* device, const DX12Descriptors& descriptors)
{
    for (auto& frame : m_FrameContexts)
    {
        if (!descriptors.CreateVisibleHeap(device, frame.srvVisibleHeap))
            return false;
    }
    return true;
}

bool DX12Frames::CreateCommandLists(ID3D12Device* device)
{
    HRESULT hr;
    // CommandAllocatorとCommandListの作成
    for (int i = 0; i < DX12_FRAME_COUNT; ++i)
    {
        hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(m_FrameContexts[i].commandAllocator.GetAddressOf()));

        if (!CheckDX12Result(hr, L"Failed to create command allocator.")) return false;

        // CommandListの作成
        hr = device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_FrameContexts[i].commandAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(m_FrameContexts[i].commandList.GetAddressOf()));

        if (!CheckDX12Result(hr, L"Failed to create command list.")) return false;

        // CommandListを閉じる
        m_FrameContexts[i].commandList->Close();
    }

    return true;
}

bool DX12Frames::CreateConstants(ID3D12Device* device)
{
    HRESULT hr;
    // ConstantBufferの作成
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;	// GPUからの読み取りは遅いため最適化の場合Defaultにする(Defaultの場合はUploadHeapからCopyする必要がある)

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

    // PerFrame ConstantBufferの作成
    for( int i = 0; i < DX12_FRAME_COUNT; ++i)
    {
        cbDesc.Width = Align256(sizeof(PerFrameConstants));
        hr = device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &cbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_FrameContexts[i].perFrameCB.GetAddressOf()));

        if (!CheckDX12Result(hr, L"Failed to create PerFrame ConstantBuffer.")) return false;
    }

    return true;
}

bool DX12Frames::BeginDraw(DX12Device& device, DX12Presentation& presentation, const DX12Descriptors& descriptors,
    const DX12Pipelines& pipelines, const Matrix4x4_SIMD& view, const Matrix4x4_SIMD& projection)
{
    m_frameIndex = (m_frameIndex + 1) % DX12_FRAME_COUNT;
    FrameContext& frame = m_FrameContexts[m_frameIndex];
    ID3D12GraphicsCommandList* cmd = frame.commandList.Get();

    // この Frame の前回の利用が終わるまで、Allocator や定数を書き換えない。
    if (frame.fenceValue != 0 && device.GetFence()->GetCompletedValue() < frame.fenceValue)
    {
        if (FAILED(device.GetFence()->SetEventOnCompletion(frame.fenceValue, device.GetFenceEvent())) ||
            WaitForSingleObject(device.GetFenceEvent(), INFINITE) != WAIT_OBJECT_0)
            return false;
    }

    // PSO は描画する Pass ごとに設定するため、描画開始時は指定しない。
    if (FAILED(frame.commandAllocator->Reset()) ||
        FAILED(frame.commandList->Reset(frame.commandAllocator.Get(), nullptr)))
        return false;

    presentation.BeginDraw(cmd, descriptors);

    // VisibleSRVHeapの設定
    ID3D12DescriptorHeap* heaps[] = { frame.srvVisibleHeap.Get() };
    cmd->SetDescriptorHeaps(1, heaps);

    // RootSignatureの設定
    cmd->SetGraphicsRootSignature(pipelines.GetRootSignature());

    // PerFrame ConstantBufferの設定
    void* pData;
    if (FAILED(frame.perFrameCB->Map(0, nullptr, &pData)))
    {
        frame.commandList->Close();
        return false;
    }

    PerFrameConstants* perFrameConstants = static_cast<PerFrameConstants*>(pData);
    perFrameConstants->viewMatrix = view;
    perFrameConstants->projectionMatrix = projection;
    perFrameConstants->cameraPosition = Vector3(0.0f, 0.0f, 0.0f);
    perFrameConstants->time = TimeSystem::TotalTime();
    frame.perFrameCB->Unmap(0, nullptr);

    cmd->SetGraphicsRootConstantBufferView(0, frame.perFrameCB->GetGPUVirtualAddress());

    // Fence完了済みのHeapとUpload領域を再利用し、今フレームのMaterialを再取得する
    frame.m_materialBindings.clear();
    frame.visibleSRVCount = 0;
    frame.drawDataOffset = 0;
    return true;
}
bool DX12Frames::EndDraw(DX12Device& device, DX12Presentation& presentation)
{
    ID3D12GraphicsCommandList* commandList = m_FrameContexts[m_frameIndex].commandList.Get();

    presentation.RecordBackBufferCopy(commandList);

    // コマンドリストのクローズ
    commandList->Close();

    // BackBuffer へのコピーを含めて送信し、同じ Queue へ完了を表す Signal を積む。
    ID3D12CommandList* commandLists[] = { commandList };
    device.GetQueue()->ExecuteCommandLists(_countof(commandLists), commandLists);

    // シグナルの送信
    const UINT64 fenceValue = device.NextFenceValue();
    if (!CheckDX12Result(device.GetQueue()->Signal(device.GetFence(), fenceValue), L"Failed to signal frame fence."))
        return false;
    m_FrameContexts[m_frameIndex].fenceValue = device.LastFenceValue();

    presentation.Present();

    return true;
}

D3D12_GPU_VIRTUAL_ADDRESS DX12Frames::UploadDrawData(ID3D12Device* device, std::span<const std::byte> bytes)
{
    // CBVとインスタンスSRVで同じフレーム領域を使い、各開始アドレスを256バイトに揃える
    constexpr std::size_t pageSize = 1u << 20;
    if (bytes.size() > pageSize)
        return 0;

    const auto size = Align256(static_cast<UINT>(std::max<std::size_t>(bytes.size(), 1)));
    auto& frame = m_FrameContexts[m_frameIndex];
    auto offset = frame.drawDataOffset;
    if (offset % pageSize + size > pageSize)
        offset += pageSize - offset % pageSize;

    // 記録済みの Draw が参照するページを保持し、足りないときだけ次のページを追加
    if (offset / pageSize == frame.drawDataPages.size())
    {
        FrameContext::DrawDataPage page;
        const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
        const auto desc = CD3DX12_RESOURCE_DESC::Buffer(pageSize);
        const D3D12_RANGE readRange{ 0, 0 };
        void* mappedData = nullptr;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(page.resource.GetAddressOf()))) ||
            FAILED(page.resource->Map(0, &readRange, &mappedData)))
        {
            Logger::Error("Could not allocate the frame draw data buffer");
            return 0;
        }
        page.mappedData = static_cast<std::byte*>(mappedData);
        frame.drawDataPages.push_back(std::move(page));
    }

    // CPU 側のパック済みデータを転送し、次の Draw が上書きしないように進める
    const auto& page = frame.drawDataPages[offset / pageSize];
    auto* destination = page.mappedData + offset % pageSize;
    if (!bytes.empty())
        std::memcpy(destination, bytes.data(), bytes.size());
    std::memset(destination + bytes.size(), 0, size - bytes.size());
    frame.drawDataOffset = offset + size;
    return page.resource->GetGPUVirtualAddress() + offset % pageSize;
}

void DX12Frames::Finalize()
{
    // Frame が使った Upload ページも GPU 完了後にマップ解除する。
    for (auto& frame : m_FrameContexts)
    {
        frame.commandList.Reset();
        frame.commandAllocator.Reset();
        frame.perFrameCB.Reset();
        frame.srvVisibleHeap.Reset();
        frame.fenceValue = 0;
        frame.visibleSRVCount = 0;
        frame.m_materialBindings.clear();
        for (auto& page : frame.drawDataPages)
            page.resource->Unmap(0, nullptr);
        frame.drawDataPages.clear();
        frame.drawDataOffset = 0;
    }
    m_frameIndex = 0;
}

bool DX12Frames::BindMaterial(ID3D12Device* device, const DX12Resources& resources,
    const DX12Descriptors& descriptors, const Material& material)
{
    // 既に記録したMaterialはHeapが満杯になった後も同じ領域を使える
    auto& frame = Current();
    auto binding = frame.m_materialBindings.find(&material);
    if (binding == frame.m_materialBindings.end())
    {
        if (frame.visibleSRVCount >= DX12_MAX_MATERIAL_COUNT)
            return false;

        // このフレームでの初回利用時にMaterial定数を確保する
        const auto bytes = material.GetConstantBuffer();
        if (bytes.size() > D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16u)
        {
            Logger::Error("Material constant buffer exceeds the DX12 limit");
            return false;
        }
        const auto address = UploadDrawData(device, bytes);
        if (address == 0)
            return false;

        // ReflectionのスロットにTextureを割り当て、空きは白Textureで埋める
        std::array<int, MAX_MATERIAL_TEXTURE> slots;
        slots.fill(static_cast<int>(resources.WhiteTexture().m_masterSRVIndex));
        for (const auto& property : material.GetShader()->GetProperties())
        {
            if (property.m_resourceSlot >= MAX_MATERIAL_TEXTURE)
                continue;
            if (const auto* texture = resources.GetTexture(material.GetTexture(property.m_name)))
                slots[property.m_resourceSlot] = static_cast<int>(texture->m_masterSRVIndex);
        }
        const auto textures = CopyMaterialTextures(device, descriptors, slots);
        binding = frame.m_materialBindings.emplace(&material, MaterialBinding{ address, textures }).first;
    }

    // System Textureは専用領域を用意するまで同じテーブルを参照する
    const auto& data = binding->second;
    frame.commandList->SetGraphicsRootConstantBufferView(
        static_cast<UINT>(DX12RootParameterIndex::PerMaterialCBV), data.m_constants);
    frame.commandList->SetGraphicsRootDescriptorTable(
        static_cast<UINT>(DX12RootParameterIndex::PerMaterialSRV), data.m_textures);
    frame.commandList->SetGraphicsRootDescriptorTable(
        static_cast<UINT>(DX12RootParameterIndex::PerSystemSRV), data.m_textures);
    return true;
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12Frames::CopyMaterialTextures(ID3D12Device* device, const DX12Descriptors& descriptors,
    const std::array<int, MAX_MATERIAL_TEXTURE>& slots)
{
    // 前の Draw が読むテーブルを残し、現在の Frame の空き範囲へコピーする。
    auto& frame = Current();
    const UINT increment = descriptors.GetSRVIncrementSize();
    const UINT offset = increment * frame.visibleSRVCount * MAX_MATERIAL_TEXTURE;
    auto destination = frame.srvVisibleHeap->GetCPUDescriptorHandleForHeapStart();
    destination.ptr += offset;

    for (const auto slot : slots)
    {
        device->CopyDescriptorsSimple(1, destination, descriptors.GetSRVHandle(slot),
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        destination.ptr += increment;
    }

    // 後続のDrawからも参照できるようテーブルの先頭を返す
    auto gpuHandle = frame.srvVisibleHeap->GetGPUDescriptorHandleForHeapStart();
    gpuHandle.ptr += offset;
    ++frame.visibleSRVCount;
    return gpuHandle;
}
