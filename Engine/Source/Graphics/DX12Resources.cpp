/*=============================================================================

 File   : DX12Resources.cpp
 Desc   : GPU Asset とアップロードを管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Graphics/DX12Resources.h"

#include "Graphics/DX12Device.h"
#include "Graphics/DX12Descriptors.h"
#include "Graphics/DX12Support.h"
#include "D3DX12/d3dx12.h"

#include <cstring>
#include <limits>

using namespace Hestia;

bool DX12Resources::Initialize(ID3D12Device* device)
{
    return m_UploadContextPool.Initialize(device);
}

void DX12Resources::Update(const DX12Device& device)
{
    auto* fence = device.GetFence();
    if (!fence)
        return;
    const auto completedValue = fence->GetCompletedValue();

    // 同じ Queue で送った転送の完了順に Upload Resource を解放する。
    while (!m_WaitingResources.empty() && completedValue >= m_WaitingResources.front().fenceValue)
        m_WaitingResources.pop();

    m_UploadContextPool.ReleaseContext(completedValue);
}

void DX12Resources::Finalize()
{
    // GPU の完了後に登録済み Asset と代替資源をまとめて解放する。
    m_gpuMeshes.Clear();
    m_gpuTextures.Clear();
    m_defaultStreamBuffers = {};
    m_defaultStreamCapacities.fill(0);
    m_whiteTexture = {};
    m_SystemTextures.clear();
    while (!m_WaitingResources.empty())
        m_WaitingResources.pop();
    m_UploadContextPool.Finalize();
}

const DXVertexBuffer& DX12Resources::GetVertexBuffer(const DXMesh& mesh, VertexStream stream) const
{
    // Mesh にない任意属性は、同じスロットに共用の既定値を供給する。
    const auto index = static_cast<std::size_t>(stream);
    const auto& buffer = mesh.m_streams[index];
    return buffer.m_resource ? buffer : m_defaultStreamBuffers[index];
}

bool DX12Resources::UploadBufferToDefault(
    DX12Device& device, 
    const void* data,
    size_t byteSize,
    D3D12_RESOURCE_STATES finalState,
    ComPtr<ID3D12Resource>& resource)
{
    if (!device.GetDevice() || !device.GetQueue() || !device.GetFence() || !data || byteSize == 0)
        return false;

    // Default Heap と転送用 Upload Heap を作成
    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(byteSize));
    resource.Reset();

    HRESULT hr = device.GetDevice()->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(resource.GetAddressOf()));

    if (!CheckDX12Result(hr, L"Failed to create default buffer."))
        return false;

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    ComPtr<ID3D12Resource> uploadResource;
    hr = device.GetDevice()->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadResource.GetAddressOf()));
    if (!CheckDX12Result(hr, L"Failed to create buffer upload resource."))
        return false;

    void* mappedData = nullptr;
    hr = uploadResource->Map(0, nullptr, &mappedData);
    if (!CheckDX12Result(hr, L"Failed to map buffer upload resource."))
        return false;

    // 呼び出し元のデータを Upload Resource へコピー
    memcpy(mappedData, data, byteSize);
    uploadResource->Unmap(0, nullptr);

    // 同じ Queue へ転送を記録する。Allocator の使用期間をフェンス番号に対応付ける。
    int contextIndex = -1;
    if (!m_UploadContextPool.GetFreeIndex(contextIndex))
        return false;

    const UINT64 fenceValue = device.NextFenceValue();
    auto commandList = m_UploadContextPool.GetResetCommandList(contextIndex, fenceValue);
    if (!commandList)
        return false;

    // CopyBufferRegionで Default Heap へコピー
    commandList->CopyBufferRegion(resource.Get(), 0, uploadResource.Get(), 0, byteSize);
    if (finalState != D3D12_RESOURCE_STATE_COPY_DEST)
    {
        const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);
        commandList->ResourceBarrier(1, &barrier);
    }

    hr = commandList->Close();
    if (!CheckDX12Result(hr, L"Failed to close buffer upload command list."))
        return false;

    ID3D12CommandList* commandLists[] = { commandList.Get() };
    device.GetQueue()->ExecuteCommandLists(1, commandLists);
    // Copy 完了 Fence まで両 Resource を保持
    m_WaitingResources.push({ uploadResource, fenceValue });
    m_WaitingResources.push({ resource, fenceValue });

    if (!CheckDX12Result(device.GetQueue()->Signal(device.GetFence(), fenceValue), L"Failed to signal buffer upload."))
        return false;

    return true;
}

bool DX12Resources::CreateVertexStreamBuffer(
    DX12Device& device, 
    VertexStream stream,
    const void* data,
    size_t vertexCount,
    DXVertexBuffer& result)
{
    if (!data || vertexCount == 0)
        return false;

    size_t stride = 0;
    switch (stream)
    {
    case VertexStream::Position:
        stride = sizeof(PositionVertex);
        break;
    case VertexStream::NormalTangent:
        stride = sizeof(NormalTangentVertex);
        break;
    case VertexStream::PrimaryTexCoord:
        stride = sizeof(PrimaryTexCoordVertex);
        break;
    case VertexStream::Color:
        stride = sizeof(ColorVertex);
        break;
    case VertexStream::Count:
        return false;
    }

    if (stride == 0 || vertexCount > std::numeric_limits<UINT>::max() / stride)
        return false;

    const size_t byteSize = stride * vertexCount;
    if (!UploadBufferToDefault(device, 
        data,
        byteSize,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        result.m_resource))
        return false;

    result.m_view.BufferLocation = result.m_resource->GetGPUVirtualAddress();
    result.m_view.SizeInBytes = static_cast<UINT>(byteSize);
    result.m_view.StrideInBytes = static_cast<UINT>(stride);
    return true;
}

bool DX12Resources::CreateIndexBuffer(
    DX12Device& device, 
    const uint32_t* indices,
    size_t indexCount,
    ComPtr<ID3D12Resource>& indexBuffer,
    D3D12_INDEX_BUFFER_VIEW& indexBufferView)
{
    if (!indices || indexCount == 0 ||
        indexCount > std::numeric_limits<UINT>::max() / sizeof(uint32_t))
        return false;

    const size_t byteSize = sizeof(uint32_t) * indexCount;
    if (!UploadBufferToDefault(device, 
        indices,
        byteSize,
        D3D12_RESOURCE_STATE_INDEX_BUFFER,
        indexBuffer))
        return false;

    indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    indexBufferView.SizeInBytes = static_cast<UINT>(byteSize);
    indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    return true;
}

bool DX12Resources::EnsureDefaultStreamBuffers(DX12Device& device, size_t vertexCount)
{
    if (vertexCount == 0 ||
        vertexCount > std::numeric_limits<UINT>::max() / sizeof(NormalTangentVertex) ||
        vertexCount > std::numeric_limits<UINT>::max() / sizeof(PrimaryTexCoordVertex) ||
        vertexCount > std::numeric_limits<UINT>::max() / sizeof(ColorVertex))
        return false;

    // 法線と接線の既定値
    const size_t normalIndex = static_cast<size_t>(VertexStream::NormalTangent);
    if (m_defaultStreamCapacities[normalIndex] < vertexCount ||
        !m_defaultStreamBuffers[normalIndex].m_resource)
    {
        const std::vector<NormalTangentVertex> vertices(vertexCount);

        DXVertexBuffer buffer;
        if (!CreateVertexStreamBuffer(device, VertexStream::NormalTangent, vertices.data(), vertexCount, buffer))
            return false;
        RetireResource(device, std::move(m_defaultStreamBuffers[normalIndex].m_resource));
        m_defaultStreamBuffers[normalIndex] = std::move(buffer);
        m_defaultStreamCapacities[normalIndex] = vertexCount;
    }

    // UV の既定値
    const size_t uvIndex = static_cast<size_t>(VertexStream::PrimaryTexCoord);
    if (m_defaultStreamCapacities[uvIndex] < vertexCount ||
        !m_defaultStreamBuffers[uvIndex].m_resource)
    {
        std::vector<PrimaryTexCoordVertex> vertices(vertexCount);
        DXVertexBuffer buffer;
        if (!CreateVertexStreamBuffer(device, VertexStream::PrimaryTexCoord, vertices.data(), vertexCount, buffer))
            return false;
        RetireResource(device, std::move(m_defaultStreamBuffers[uvIndex].m_resource));
        m_defaultStreamBuffers[uvIndex] = std::move(buffer);
        m_defaultStreamCapacities[uvIndex] = vertexCount;
    }

    // 頂点カラーの既定値
    const size_t colorIndex = static_cast<size_t>(VertexStream::Color);
    if (m_defaultStreamCapacities[colorIndex] < vertexCount ||
        !m_defaultStreamBuffers[colorIndex].m_resource)
    {
        std::vector<ColorVertex> vertices(vertexCount);
        for (auto& vertex : vertices)
            vertex.m_color = { 1.0f, 1.0f, 1.0f, 1.0f };

        DXVertexBuffer buffer;
        if (!CreateVertexStreamBuffer(device, VertexStream::Color, vertices.data(), vertexCount, buffer))
            return false;
        RetireResource(device, std::move(m_defaultStreamBuffers[colorIndex].m_resource));
        m_defaultStreamBuffers[colorIndex] = std::move(buffer);
        m_defaultStreamCapacities[colorIndex] = vertexCount;
    }

    return true;
}

void DX12Resources::RetireResource(const DX12Device& device, ComPtr<ID3D12Resource> resource)
{
    if (!resource || !device.GetQueue() || !device.GetFence() || device.LastFenceValue() == 0 ||
        device.GetFence()->GetCompletedValue() >= device.LastFenceValue())
        return;

    m_WaitingResources.push({ std::move(resource), device.LastFenceValue() });
}

bool DX12Resources::UploadTextureResource(
    DX12Device& device, DX12Descriptors& descriptors, 
    DXTexture& texture,
    const TextureDesc& desc,
    const void* pixels,
    size_t rowPitch,
    size_t slicePitch)
{
    if (!device.GetDevice() || !device.GetQueue() || !device.GetFence() || !pixels)
        return false;

    // Format と Pitch を確認
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    switch (desc.m_format)
    {
    case TextureFormat::RGBA8Unorm:
        format = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    case TextureFormat::RGBA8Srgb:
        format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        break;
    default:
        return false;
    }

    if (desc.m_width > std::numeric_limits<size_t>::max() / 4)
        return false;

    const size_t minimumRowPitch = static_cast<size_t>(desc.m_width) * 4;
    if (rowPitch < minimumRowPitch || rowPitch > static_cast<size_t>(std::numeric_limits<LONG_PTR>::max()) ||
        desc.m_height > std::numeric_limits<size_t>::max() / rowPitch)
        return false;

    const size_t minimumSlicePitch = rowPitch * static_cast<size_t>(desc.m_height);
    if (slicePitch < minimumSlicePitch ||
        slicePitch > static_cast<size_t>(std::numeric_limits<LONG_PTR>::max()))
        return false;

    // Default Heap Texture を作成
    const auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        format,
        desc.m_width,
        desc.m_height,
        1,
        static_cast<UINT16>(desc.m_mipLevels));
    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    texture.m_resource.Reset();

    HRESULT hr = device.GetDevice()->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(texture.m_resource.GetAddressOf()));
    if (!CheckDX12Result(hr, L"Failed to create texture resource."))
        return false;

    // CPU 画素を Upload Resource 経由で Default Texture へ転送
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.m_resource.Get(), 0, 1);
    if (uploadBufferSize == 0)
        return false;

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    const auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    ComPtr<ID3D12Resource> uploadResource;
    hr = device.GetDevice()->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadResource.GetAddressOf()));
    if (!CheckDX12Result(hr, L"Failed to create texture upload resource."))
        return false;

    int contextIndex = -1;
    if (!m_UploadContextPool.GetFreeIndex(contextIndex))
        return false;

    const UINT64 fenceValue = device.NextFenceValue();
    auto commandList = m_UploadContextPool.GetResetCommandList(contextIndex, fenceValue);
    if (!commandList)
        return false;

    D3D12_SUBRESOURCE_DATA subresource{};
    subresource.pData = pixels;
    subresource.RowPitch = static_cast<LONG_PTR>(rowPitch);
    subresource.SlicePitch = static_cast<LONG_PTR>(slicePitch);
    const UINT64 copiedBytes = UpdateSubresources(
        commandList.Get(),
        texture.m_resource.Get(),
        uploadResource.Get(),
        0,
        0,
        1,
        &subresource);
    if (copiedBytes == 0)
    {
        commandList->Close();
        return false;
    }

    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        texture.m_resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &barrier);

    hr = commandList->Close();
    if (!CheckDX12Result(hr, L"Failed to close texture upload command list."))
        return false;

    ID3D12CommandList* commandLists[] = { commandList.Get() };
    device.GetQueue()->ExecuteCommandLists(1, commandLists);
    m_WaitingResources.push({ uploadResource, fenceValue });
    m_WaitingResources.push({ texture.m_resource, fenceValue });

    if (!CheckDX12Result(device.GetQueue()->Signal(device.GetFence(), fenceValue), L"Failed to signal texture upload."))
        return false;

    // 成功した Texture に SRV を割り当てる
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = desc.m_mipLevels;
    return descriptors.CreateShaderResourceView(device.GetDevice(), texture.m_resource.Get(), texture.m_masterSRVIndex, &srvDesc);
}

bool DX12Resources::CreateWhiteTexture(DX12Device& device, DX12Descriptors& descriptors)
{
    TextureDesc desc;
    desc.m_width = 1;
    desc.m_height = 1;
    desc.m_mipLevels = 1;
    desc.m_format = TextureFormat::RGBA8Unorm;
    const std::array<uint8_t, 4> pixels{ 255, 255, 255, 255 };
    return UploadTextureResource(device, descriptors, m_whiteTexture, desc, pixels.data(), pixels.size(), pixels.size());
}

bool DX12Resources::UploadMesh(DX12Device& device, AssetHandle<Mesh> handle, const Mesh& mesh)
{
    if (!handle.IsValid() ||
        handle.m_index != mesh.m_handle.m_index ||
        handle.m_generation != mesh.m_handle.m_generation ||
        !mesh.IsValid() || !m_gpuMeshes.CanRegister(handle))
        return false;

    DXMesh gpuMesh;
    if (!CreateMesh(device, mesh, gpuMesh))
        return false;

    return m_gpuMeshes.Register(handle, std::move(gpuMesh));
}

bool DX12Resources::UploadTexture(
    DX12Device& device, DX12Descriptors& descriptors, 
    AssetHandle<Texture> handle,
    const TextureDesc& desc,
    const void* pixels,
    size_t rowPitch,
    size_t slicePitch)
{
    if (!handle.IsValid() || !desc.IsValid() ||
        !pixels || !m_gpuTextures.CanRegister(handle))
        return false;

    DXTexture gpuTexture;
    if (!UploadTextureResource(device, descriptors, gpuTexture, desc, pixels, rowPitch, slicePitch))
        return false;

    return m_gpuTextures.Register(handle, std::move(gpuTexture));
}

bool DX12Resources::HasMesh(AssetHandle<Mesh> handle) const
{
    return m_gpuMeshes.Get(handle) != nullptr;
}
bool DX12Resources::CreateMesh(DX12Device& device, const Mesh& mesh, DXMesh& result)
{
    // 必須 Position と存在する Optional Stream を作成
    const size_t vertexCount = mesh.m_positions.size();
    result = {};
    if (!EnsureDefaultStreamBuffers(device, vertexCount))
        return false;

    const size_t positionIndex = static_cast<size_t>(VertexStream::Position);
    if (!CreateVertexStreamBuffer(device, 
        VertexStream::Position,
        mesh.m_positions.data(),
        vertexCount,
        result.m_streams[positionIndex]))
        return false;

    const size_t normalIndex = static_cast<size_t>(VertexStream::NormalTangent);
    if (!mesh.m_normalTangents.empty() && !CreateVertexStreamBuffer(device, 
        VertexStream::NormalTangent,
        mesh.m_normalTangents.data(),
        mesh.m_normalTangents.size(),
        result.m_streams[normalIndex]))
        return false;

    const size_t uvIndex = static_cast<size_t>(VertexStream::PrimaryTexCoord);
    if (!mesh.m_primaryTexCoords.empty() && !CreateVertexStreamBuffer(device, 
        VertexStream::PrimaryTexCoord,
        mesh.m_primaryTexCoords.data(),
        mesh.m_primaryTexCoords.size(),
        result.m_streams[uvIndex]))
        return false;

    const size_t colorIndex = static_cast<size_t>(VertexStream::Color);
    if (!mesh.m_colors.empty() && !CreateVertexStreamBuffer(device, 
        VertexStream::Color,
        mesh.m_colors.data(),
        mesh.m_colors.size(),
        result.m_streams[colorIndex]))
        return false;

    // Index Buffer を作成
    if (!CreateIndexBuffer(device, 
        mesh.m_indices.data(),
        mesh.m_indices.size(),
        result.m_indexBuffer,
        result.m_indexView))
        return false;

    result.m_vertexCount = static_cast<uint32_t>(vertexCount);
    // CPU Mesh と同じ描画範囲を GPU Mesh に保持。SubMesh 未指定時は Slot 0 にまとめる
    result.m_subMeshes.resize(mesh.GetSubMeshCount());
    for (std::size_t index = 0; index < result.m_subMeshes.size(); ++index)
        mesh.GetSubMesh(index, result.m_subMeshes[index]);
    return true;
}

bool DX12Resources::UploadContextPool::Initialize(ComPtr<ID3D12Device> device)
{
    m_Device = device;
    contexts.reserve(INITIAL_CONTEXT_COUNT);

    for (int i = 0; i < INITIAL_CONTEXT_COUNT; ++i)
    {
        UploadContext context;
        if (!CreateUploadContext(context))
            return false;
        contexts.push_back(std::move(context));
    }
    return true;
}

void DX12Resources::UploadContextPool::Finalize()
{
    contexts.clear();
    m_Device.Reset();
}

void DX12Resources::UploadContextPool::ReleaseContext(UINT64 completedFenceValue)
{
    for (auto& context : contexts)
    {
        if (!context.isFree && context.fenceValue <= completedFenceValue)
        {
            context.isFree = true;
        }
    }
}

bool DX12Resources::UploadContextPool::GetFreeIndex(int& outIndex)
{
    for (int i = 0; i < contexts.size(); ++i)
    {
        if (contexts[i].isFree)
        {
            outIndex = i;
            return true;
        }
    }
    UploadContext newContext;
    if (!CreateUploadContext(newContext))
        return false;

    contexts.push_back(std::move(newContext));
    outIndex = contexts.size() - 1;
    return true;
}

ComPtr<ID3D12GraphicsCommandList> DX12Resources::UploadContextPool::GetResetCommandList(int index, UINT64 fenceValue)
{
    if (index < 0 || index >= contexts.size())
        return nullptr;

    if (FAILED(contexts[index].allocator->Reset()) ||
        FAILED(contexts[index].commandList->Reset(contexts[index].allocator.Get(), nullptr)))
        return nullptr;
    contexts[index].fenceValue = fenceValue;
    contexts[index].isFree = false;
    return contexts[index].commandList;
}

bool DX12Resources::UploadContextPool::CreateUploadContext(UploadContext& context)
{
    // CommandAllocatorの作成
    HRESULT hr = m_Device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(context.allocator.GetAddressOf()));

    if(FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to UploadContext command allocator.", L"Error", MB_OK);
        return false;
    }

    // CommandListの作成
    hr = m_Device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        context.allocator.Get(),
        nullptr,
        IID_PPV_ARGS(context.commandList.GetAddressOf()));

    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to UploadContext command list.", L"Error", MB_OK);
        return false;
    }

    // CommandListを閉じる
    context.commandList->Close();

    return true;
}
