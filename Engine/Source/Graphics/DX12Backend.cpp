/*=============================================================================

 File   : DX12Backend.cpp
 Desc   : DirectX12を直接叩くレイヤーの実装

 ------------------------------------------------------------------------------

 Date   : 2026/09/30
 Author : Yokoyama Haruki

===============================================================================*/

#include "pch.h"

#include "Graphics/DX12Backend.h"
#include "D3DX12/d3dx12.h"

#include "Time/TimeSystem.h"
#include "Log/LogSystem.h"

#include <dxgidebug.h>
#include <cstddef>
#include <limits>

using namespace Hestia;

constexpr UINT Align256(UINT size)
{
	// 256の倍数に切り上げる
	// ~255 は 0xFFFFFF00 で、下位8ビット(256)を0にするマスク
	return (size + 255) & ~255;
}

bool DX12Backend::CreateShaderResourceView(
	ID3D12Resource * resource,
	uint32_t& srvHeapIndex,
	D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc)
{
	if (!m_Device || !resource || !m_SRVMasterHeap)
		return false;

	if (m_SRVMasterHeapIndex >= MAX_MASTER_SRV_COUNT)
	{
		MessageBox(nullptr, L"Exceeded maximum SRV count.", L"Error", MB_OK);
		return false;
	}

	// SRVの作成
	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_SRVMasterHeapStart;
	handle.ptr += m_SRVIncrementSize * m_SRVMasterHeapIndex;

	m_Device->CreateShaderResourceView(
		resource,
		srvDesc,
		handle);

	// SRVヒープのインデックスを返す
	srvHeapIndex = static_cast<uint32_t>(m_SRVMasterHeapIndex);

	// SRVヒープのインデックスを更新
	m_SRVMasterHeapIndex++;

	return true;
}

bool DX12Backend::CreateRenderTargetView(
	ID3D12Resource* resource,
	int& rtvHeapIndex,
	D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc)
{
	if(m_RTVHeapIndex >= MAX_RTV_COUNT)
	{
		MessageBox(nullptr, L"Exceeded maximum RTV count.", L"Error", MB_OK);
		return false;
	}

	// RTVの作成
	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_RTVHeapStart;
	handle.ptr += m_RTVIncrementSize * m_RTVHeapIndex;
	m_Device->CreateRenderTargetView(
		resource,
		rtvDesc,
		handle);

	// RTVヒープのインデックスを返す
	rtvHeapIndex = m_RTVHeapIndex;

	// RTVヒープのインデックスを更新
	m_RTVHeapIndex++;
	return true;
}

bool DX12Backend::CreateDepthStencilView(
	ID3D12Resource* resource,
	int& dsvHeapIndex,
	D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc)
{
	if(m_DSVHeapIndex >= MAX_DSV_COUNT)
	{
		MessageBox(nullptr, L"Exceeded maximum DSV count.", L"Error", MB_OK);
		return false;
	}

	// DSVの作成
	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_DSVHeapStart;
	handle.ptr += m_DSVIncrementSize * m_DSVHeapIndex;
	m_Device->CreateDepthStencilView(
		resource,
		dsvDesc,
		handle);

	// DSVヒープのインデックスを返す
	dsvHeapIndex = m_DSVHeapIndex;

	// DSVヒープのインデックスを更新
	m_DSVHeapIndex++;
	return true;
}

bool DX12Backend::LoadShaderFromCSO(const wchar_t* path, ComPtr<ID3DBlob>& shaderBlob)
{
	HRESULT hr = D3DReadFileToBlob(path, shaderBlob.GetAddressOf());
	if (!CheckResult(hr, L"Failed to load shader from CSO.")) return false;
	return true;
}

bool DX12Backend::Initialize(HWND hWnd, int width, int height)
{
    if (m_Device || !hWnd || width <= 0 || height <= 0)
        return false;

	HRESULT hr = S_OK;

	// デバッグレイヤーの有効化
#ifdef _DEBUG

	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(
		D3D12GetDebugInterface(
			IID_PPV_ARGS(debugController.GetAddressOf()))))
	{
		debugController->EnableDebugLayer();
	}

#endif

	// Deviceの作成
	ComPtr<IDXGIFactory7> dxgiFactory;
	hr = CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
	if (!CheckResult(hr, L"Failed to create DXGI factory.")) return false;

	ComPtr<IDXGIAdapter4> adapter;
	bool faundSoftwareAdapter = false;

	// GPUの探索
	for( UINT i = 0; 
		dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(adapter.GetAddressOf())) != DXGI_ERROR_NOT_FOUND; i++)
	{
		DXGI_ADAPTER_DESC3 desc{};
		adapter->GetDesc3(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)
		{
			faundSoftwareAdapter = true;
			continue;
		}

		// デバイスの作成
		hr = D3D12CreateDevice(
			adapter.Get(), 
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(m_Device.GetAddressOf()));

		if (!CheckResult(hr, L"Failed to create D3D12 device.")) return false;

		break;
	}

	if(!m_Device)
	{
		if(faundSoftwareAdapter)
		{
			// ソフトウェアアダプタを使用する
			hr = D3D12CreateDevice(
				nullptr, 
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(m_Device.GetAddressOf()));

			if (!CheckResult(hr, L"Failed to create D3D12 device.")) return false;
		}
		else
		{
			MessageBox(nullptr, L"Failed to find a suitable GPU.", L"Error", MB_OK);
			return false;
		}
	}

	// 各DescriptorHeapのインクリメントサイズを取得
	m_RTVIncrementSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_DSVIncrementSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_SRVIncrementSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// CommandQueueの作成
	D3D12_COMMAND_QUEUE_DESC queueDesc{};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask = 0;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

	hr = m_Device->CreateCommandQueue(
		&queueDesc, IID_PPV_ARGS(m_CommandQueue.GetAddressOf()));

	if (!CheckResult(hr, L"Failed to create command queue.")) return false;

	// SwapChainの作成
	BOOL allowTearing = FALSE;

	dxgiFactory->CheckFeatureSupport(
		DXGI_FEATURE_PRESENT_ALLOW_TEARING,
		&allowTearing,
		sizeof(allowTearing));

	m_AllowTearing = allowTearing == TRUE;

	DXGI_SWAP_CHAIN_DESC1 desc{};
	desc.Width = width;
	desc.Height = height;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.Stereo = FALSE;

	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = MAX_BACKBUFFER_COUNT;

	desc.Scaling = DXGI_SCALING_STRETCH;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	desc.Flags = m_AllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> swapChain;
	hr = dxgiFactory->CreateSwapChainForHwnd(
		m_CommandQueue.Get(),
		hWnd,
		&desc,
		nullptr,
		nullptr,
        swapChain.GetAddressOf());
    if (!CheckResult(hr, L"Failed to create swap chain.")) return false;
    if (!CheckResult(swapChain.As(&m_SwapChain), L"Failed to query swap chain.")) return false;

	// BackBufferの取得
	for (int i = 0; i < MAX_BACKBUFFER_COUNT; ++i)
	{
		hr = m_SwapChain->GetBuffer(i, IID_PPV_ARGS(m_BackBuffers[i].GetAddressOf()));
		if (!CheckResult(hr, L"Failed to get back buffer.")) return false;
	}


	// RTVHeapの作成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.NumDescriptors = MAX_RTV_COUNT;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	hr = m_Device->CreateDescriptorHeap(
		&heapDesc, IID_PPV_ARGS(m_RTVHeap.GetAddressOf()));
	if (!CheckResult(hr, L"Failed to create RTV descriptor heap.")) return false;

	// RTVHeapの先頭ハンドルを取得
	m_RTVHeapStart = m_RTVHeap->GetCPUDescriptorHandleForHeapStart();



	// DSVHeapの作成
	heapDesc.NumDescriptors = 64;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	hr = m_Device->CreateDescriptorHeap(
		&heapDesc, IID_PPV_ARGS(m_DSVHeap.GetAddressOf()));
	if (!CheckResult(hr, L"Failed to create DSV descriptor heap.")) return false;

	// DSVHeapの先頭ハンドルを取得
	m_DSVHeapStart = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();



	// SRVMasterHeapの作成
	heapDesc.NumDescriptors = MAX_MASTER_SRV_COUNT;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	hr = m_Device->CreateDescriptorHeap(
		&heapDesc, IID_PPV_ARGS(m_SRVMasterHeap.GetAddressOf()));
	if (!CheckResult(hr, L"Failed to create SRV master descriptor heap.")) return false;
	
	// SRVMasterHeapの先頭ハンドルを取得
	m_SRVMasterHeapStart = m_SRVMasterHeap->GetCPUDescriptorHandleForHeapStart();



	// SRVVisibleHeapの作成
	heapDesc.NumDescriptors = MAX_SYSTEM_TEXTURE + MAX_MATERIAL_TEXTURE * MAX_MATERIAL_COUNT;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	for( int i = 0;i < FRAME_COUNT; ++i)
	{
		hr = m_Device->CreateDescriptorHeap(
			&heapDesc, IID_PPV_ARGS(m_FrameContexts[i].srvVisibleHeap.GetAddressOf()));
		if (!CheckResult(hr, L"Failed to create SRV visible descriptor heap.")) return false;

	}


	// RenderTargetの作成
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	for( int i = 0; i < 4; ++i)
		clearValue.Color[i] = CLEAR_COLOR[i];

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	for (int i = 0; i < 2; ++i)
	{

		hr = m_Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			&clearValue,
			IID_PPV_ARGS(m_sceneColors[i].resource.GetAddressOf()));
		if (!CheckResult(hr, L"Failed to create render target.")) return false;

		// RTVの作成
		if (!CreateRenderTargetView(m_sceneColors[i].resource.Get(), m_sceneColors[i].rtvHeapIndex))
		{
			MessageBox(nullptr, L"Failed to create RTV for render target.", L"Error", MB_OK);
			return false;
		}
		
		// SRVの作成
		if (!CreateShaderResourceView(m_sceneColors[i].resource.Get(), m_sceneColors[i].srvHeapIndex))
		{
			MessageBox(nullptr, L"Failed to create SRV for render target.", L"Error", MB_OK);
			return false;
		}

	}

	// DepthStancilResourceの作成
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	hr = m_Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(m_DepthStencil.resource.GetAddressOf()));
	if (!CheckResult(hr, L"Failed to create depth stencil.")) return false;

	// DSVの作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	if (!CreateDepthStencilView(m_DepthStencil.resource.Get(), m_DepthStencil.dsvHeapIndex, &dsvDesc))
	{
		MessageBox(nullptr, L"Failed to create DSV for depth stencil.", L"Error", MB_OK);
		return false;
	}

	// SRV(Depth)の作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping =
		D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;

	if (!CreateShaderResourceView(m_DepthStencil.resource.Get(), m_DepthStencil.depthSrvHeapIndex, &srvDesc))
	{
		MessageBox(nullptr, L"Failed to create SRV for depth stencil.", L"Error", MB_OK);
		return false;
	}

	// SRV(Stencil)の作成
	srvDesc.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
	srvDesc.Texture2D.PlaneSlice = 1;

	if(!CreateShaderResourceView(m_DepthStencil.resource.Get(), m_DepthStencil.stencilSrvHeapIndex, &srvDesc))
	{
		MessageBox(nullptr, L"Failed to create SRV for stencil.", L"Error", MB_OK);
		return false;
	}

	// CommandAllocatorとCommandListの作成
	for (int i = 0; i < FRAME_COUNT; ++i)
	{
		hr = m_Device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(m_FrameContexts[i].commandAllocator.GetAddressOf()));

		if (!CheckResult(hr, L"Failed to create command allocator.")) return false;

		// CommandListの作成
		hr = m_Device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_FrameContexts[i].commandAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(m_FrameContexts[i].commandList.GetAddressOf()));

		if (!CheckResult(hr, L"Failed to create command list.")) return false;

		// CommandListを閉じる
		m_FrameContexts[i].commandList->Close();
	}

	// UploadContextPoolの初期化
    if (!m_UploadContextPool.Initialize(m_Device)) return false;

	m_frameIndex = 0;

	// Fenceの作成
	hr = m_Device->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(m_Fence.GetAddressOf()));
	if (!CheckResult(hr, L"Failed to create fence.")) return false;

	// FenceEventの作成
	m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_FenceEvent) return false;


	if (!CheckResult(m_Fence->Signal(0), L"Failed to initialize fence.")) return false;

	// 白 Texture は公開 Upload API の有効化前に内部作成
	if (!CreateWhiteTexture()) return false;
	m_BoundMaterialSRVs.fill(static_cast<int>(m_whiteTexture.m_masterSRVIndex));

	auto rtvHandle = GetRTVHandle(m_sceneColors[m_sceneColorIndex].rtvHeapIndex);
	auto dsvHandle = GetDSVHandle(m_DepthStencil.dsvHeapIndex);


	// Viewportの設定

	m_Viewport.TopLeftX = 0.0f;
	m_Viewport.TopLeftY = 0.0f;
	m_Viewport.Width = static_cast<float>(width);
	m_Viewport.Height = static_cast<float>(height);
	m_Viewport.MinDepth = 0.0f;
	m_Viewport.MaxDepth = 1.0f;

	m_ScissorRect.left = 0;
	m_ScissorRect.top = 0;
	m_ScissorRect.right = width;
	m_ScissorRect.bottom = height;



	// RootSignatureの作成
	const int rootParamCount = 5;
	std::array<D3D12_ROOT_PARAMETER, rootParamCount> rootParams{};
	std::array<D3D12_ROOT_PARAMETER_TYPE, rootParamCount> rootParamTypes
	{
		D3D12_ROOT_PARAMETER_TYPE_CBV,	// PerFrame Constant Buffer
		D3D12_ROOT_PARAMETER_TYPE_CBV,	// PerObject Constant Buffer
		D3D12_ROOT_PARAMETER_TYPE_CBV,	// PerMaterial Constant Buffer
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,	// System Texture SRV
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE	// Material Texture SRV
	};

	std::array<D3D12_DESCRIPTOR_RANGE, 2> descriptorRanges
	{
		// System Texture SRV
		D3D12_DESCRIPTOR_RANGE
		{
			D3D12_DESCRIPTOR_RANGE_TYPE_SRV,		// Range Type
			16,										// Descriptor数
			0,										// Registerの開始位置
			0,										// Register Space
			D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND	// Offset (APPEND:自動で割り当て = m_SRVIncementSizeと同一サイズ？)
		},
		// Material Texture SRV
		D3D12_DESCRIPTOR_RANGE
		{
			D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			MAX_MATERIAL_TEXTURE,
			0,
			1,
			D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
		}
	};

	int tableCount = 0;
	for(int i = 0; i < rootParamCount; ++i)
	{
		rootParams[i].ParameterType = rootParamTypes[i];
		rootParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		switch (rootParamTypes[i])
		{
		case D3D12_ROOT_PARAMETER_TYPE_CBV:
			rootParams[i].Descriptor.ShaderRegister = i;
			rootParams[i].Descriptor.RegisterSpace = 0;
			break;
		
		case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
			rootParams[i].DescriptorTable.NumDescriptorRanges = 1;
			rootParams[i].DescriptorTable.pDescriptorRanges = &descriptorRanges[tableCount];
			tableCount++;
			break;

		case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
			rootParams[i].Constants.ShaderRegister = i;
			rootParams[i].Constants.RegisterSpace = 0;
			rootParams[i].Constants.Num32BitValues = 4;
			break;

		default:
			break;
		}
	}

	// Static Samplerの設定
	std::array<D3D12_STATIC_SAMPLER_DESC, 4> staticSamplers{};
	auto CreateSampler = [](D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressMode, UINT shaderRegister)->D3D12_STATIC_SAMPLER_DESC
	{
		D3D12_STATIC_SAMPLER_DESC sampler{};
		sampler.Filter = filter;
		sampler.AddressU = addressMode;
		sampler.AddressV = addressMode;
		sampler.AddressW = addressMode;
		sampler.MipLODBias = 0.0f;
		sampler.MaxAnisotropy = 1;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = shaderRegister;
		return sampler;
	};

	staticSamplers[0] = CreateSampler(
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0);
	staticSamplers[1] = CreateSampler(
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 1);
	staticSamplers[2] = CreateSampler(
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, 2);
	staticSamplers[3] = CreateSampler(
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 3);

	// RootSignatureの作成
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.NumParameters = rootParamCount;
	rootSignatureDesc.pParameters = rootParams.data();
	rootSignatureDesc.NumStaticSamplers = static_cast<UINT>(staticSamplers.size());
	rootSignatureDesc.pStaticSamplers = staticSamplers.data();
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		signatureBlob.GetAddressOf(),
		errorBlob.GetAddressOf());

	if(!CheckResult(hr, L"Failed to serialize root signature."))
	{
		if (errorBlob)
		{
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
		}
		return false;
	}

	hr = m_Device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(m_RootSignature.GetAddressOf()));
	if (!CheckResult(hr, L"Failed to create root signature.")) return false;



	// InputLayoutの設定
	static constexpr D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, static_cast<UINT>(offsetof(NormalTangentVertex, m_tangent)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 3, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	m_InputLayoutDesc.NumElements = _countof(inputElementDescs);
	m_InputLayoutDesc.pInputElementDescs = inputElementDescs;

	// ConstantBufferの作成
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
	for( int i = 0; i < FRAME_COUNT; ++i)
	{
		cbDesc.Width = Align256(sizeof(PerFrameConstants));
		hr = m_Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&cbDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_FrameContexts[i].perFrameCB.GetAddressOf()));

		if (!CheckResult(hr, L"Failed to create PerFrame ConstantBuffer.")) return false;
	}

	// PerObject ConstantBufferの作成
	cbDesc.Width = m_PerObjectCB.SIZE * MAX_OBJECT_COUNT * FRAME_COUNT;
	hr = m_Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&cbDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_PerObjectCB.constantBuffer.GetAddressOf()));
	if (!CheckResult(hr, L"Failed to create PerObject ConstantBuffer.")) return false;

	// PerObject ConstantBufferのマッピングを保持
    if (!CheckResult(m_PerObjectCB.constantBuffer->Map(0, nullptr, &m_PerObjectCB.mappedData), L"Failed to map PerObject CB.")) return false;

	// PerMaterial ConstantBufferの作成
	cbDesc.Width = m_PerMaterialCB.SIZE * MAX_MATERIAL_COUNT * FRAME_COUNT;
	hr = m_Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&cbDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_PerMaterialCB.constantBuffer.GetAddressOf()));
	if (!CheckResult(hr, L"Failed to create PerMaterial ConstantBuffer.")) return false;

	// PerMaterial ConstantBufferのマッピングを保持
    if (!CheckResult(m_PerMaterialCB.constantBuffer->Map(0, nullptr, &m_PerMaterialCB.mappedData), L"Failed to map PerMaterial CB.")) return false;

    m_IsInitialized = true;
	return true;

};

void DX12Backend::Finalize()
{
    m_IsInitialized = false;
    if (m_CommandQueue && m_Fence && !WaitForGPU())
        return;

    // Upload と Draw が参照しなくなってから GPU Resource を解放
    if (m_PerObjectCB.constantBuffer && m_PerObjectCB.mappedData)
        m_PerObjectCB.constantBuffer->Unmap(0, nullptr);
    if (m_PerMaterialCB.constantBuffer && m_PerMaterialCB.mappedData)
        m_PerMaterialCB.constantBuffer->Unmap(0, nullptr);
    m_PerObjectCB.mappedData = nullptr;
    m_PerMaterialCB.mappedData = nullptr;

    m_currentFrameData.clear();
    m_gpuMeshes.Clear();
    m_gpuTextures.Clear();
    m_defaultStreamBuffers = {};
    m_defaultStreamCapacities.fill(0);
    m_whiteTexture = {};
    m_SystemTextures.clear();
    while (!m_WaitingResources.empty())
        m_WaitingResources.pop();
    m_UploadContextPool.Finalize();

    for (auto& frame : m_FrameContexts)
    {
        frame.commandList.Reset();
        frame.commandAllocator.Reset();
        frame.perFrameCB.Reset();
        frame.srvVisibleHeap.Reset();
        frame.fenceValue = 0;
        frame.visibleSRVCount = 0;
    }

    for (auto& sceneColor : m_sceneColors)
        sceneColor = {};
    m_DepthStencil = {};
    for (auto& backBuffer : m_BackBuffers)
        backBuffer.Reset();
    m_PerObjectCB.constantBuffer.Reset();
    m_PerMaterialCB.constantBuffer.Reset();
    m_PerMaterialCB.materialCache.clear();
    m_PerMaterialCB.maxIndex = 0;
    m_CurrentPipelineState.Reset();
    m_PipelineCache.clear();
    m_RootSignature.Reset();
    m_RTVHeap.Reset();
    m_DSVHeap.Reset();
    m_SRVMasterHeap.Reset();
    m_SwapChain.Reset();
    m_CommandQueue.Reset();
    m_Fence.Reset();
    m_Device.Reset();

    if (m_FenceEvent)
    {
        CloseHandle(m_FenceEvent);
        m_FenceEvent = nullptr;
    }

    m_frameIndex = 0;
    m_sceneColorIndex = 0;
    m_RTVHeapIndex = 0;
    m_DSVHeapIndex = 0;
    m_SRVMasterHeapIndex = 0;
    m_RTVIncrementSize = 0;
    m_DSVIncrementSize = 0;
    m_SRVIncrementSize = 0;
    m_globalFenceValue = 0;
    m_AllowTearing = false;
    m_BoundMaterialSRVs.fill(-1);
}
void DX12Backend::Update()
{
	if (!m_IsInitialized || !m_Fence)
		return;

	const UINT64 completedValue = m_Fence->GetCompletedValue();

	// コピー完了したリソースをキューから削除
	// ComPtrなのでpop()で参照がなくなればリソース解放できる
	while (!m_WaitingResources.empty() &&
		completedValue >= m_WaitingResources.front().fenceValue)
	{
		m_WaitingResources.pop();
	}

	// UploadContextの解放
	m_UploadContextPool.ReleaseContext(completedValue);
}

bool DX12Backend::BeginDraw(std::vector<RenderData>& renderData, const Matrix4x4& view, const Matrix4x4& projection)
{
    if (!m_IsInitialized || renderData.size() > MAX_DRAW_COUNT)
        return false;

	m_frameIndex = (m_frameIndex + 1) % FRAME_COUNT;
	FrameContext& frame = m_FrameContexts[m_frameIndex];
	ID3D12GraphicsCommandList* cmd = frame.commandList.Get();

	// フェンスの完了を待機
	// 該当フレームのフェンスが完了していない場合完了するまで待機する
	if (frame.fenceValue != 0 && m_Fence->GetCompletedValue() < frame.fenceValue)
	{
		if (FAILED(m_Fence->SetEventOnCompletion(frame.fenceValue, m_FenceEvent)) ||
			WaitForSingleObject(m_FenceEvent, INFINITE) != WAIT_OBJECT_0)
			return false;
	}

	// コマンドアロケータとコマンドリストのリセット
    if (FAILED(frame.commandAllocator->Reset()) ||
        FAILED(frame.commandList->Reset(frame.commandAllocator.Get(), m_CurrentPipelineState.Get())))
        return false;

	// ViewportとScissorRectの設定
	cmd->RSSetViewports(1, &m_Viewport);
	cmd->RSSetScissorRects(1, &m_ScissorRect);

	// RenderTargetとDepthStencilの設定
	auto rtvHandle = GetRTVHandle(m_sceneColors[m_sceneColorIndex].rtvHeapIndex);
	auto dsvHandle = GetDSVHandle(m_DepthStencil.dsvHeapIndex);
	cmd->OMSetRenderTargets(
		1, 
		&rtvHandle,
		FALSE, 
		&dsvHandle);

	// RenderTargetとDepthStencilのクリア

	cmd->ClearRenderTargetView(
		rtvHandle,
		CLEAR_COLOR, 0, nullptr);

	cmd->ClearDepthStencilView(
		dsvHandle,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f, 0, 0, nullptr);

	// VisibleSRVHeapの設定
	ID3D12DescriptorHeap* heaps[] = { frame.srvVisibleHeap.Get() };
	cmd->SetDescriptorHeaps(1, heaps);

	// RootSignatureの設定
	cmd->SetGraphicsRootSignature(m_RootSignature.Get());

	// PerFrame ConstantBufferの設定
	void* pData;
    if (FAILED(frame.perFrameCB->Map(0, nullptr, &pData)))
    {
        frame.commandList->Close();
        return false;
    }

	PerFrameConstants* perFrameConstants = static_cast<PerFrameConstants*>(pData);
    perFrameConstants->viewMatrix = Matrix4x4_SIMD::Load(view);
    perFrameConstants->projectionMatrix = Matrix4x4_SIMD::Load(projection);
	perFrameConstants->cameraPosition = Vector3(0.0f, 0.0f, 0.0f);
	perFrameConstants->time = TimeSystem::TotalTime();
	frame.perFrameCB->Unmap(0, nullptr);

	cmd->SetGraphicsRootConstantBufferView(0, frame.perFrameCB->GetGPUVirtualAddress());

	frame.visibleSRVCount = 0;
    m_currentFrameData.swap(renderData);
    return true;
}

void Hestia::DX12Backend::Draw()
{
    long long bindObjectTime = 0;
    long long bindMaterialTime = 0;
    long long setupPipelineTime = 0;
    long long drawMeshTime = 0;

    for (size_t i = 0; i < m_currentFrameData.size(); ++i)
    {
        RenderData& data = m_currentFrameData[i];
        const DXMesh* mesh = m_gpuMeshes.Get(data.mesh);
        if (!mesh)
            continue;

        auto start = std::chrono::high_resolution_clock::now();
        BindObject(data.worldMatrix, i);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        bindObjectTime += duration.count();

        start = std::chrono::high_resolution_clock::now();
        BindMaterial(data.material);
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        bindMaterialTime += duration.count();

        start = std::chrono::high_resolution_clock::now();

        const bool pipelineReady = SetupPipeline(data.vsShaderPath, data.psShaderPath);
        if (pipelineReady && m_CurrentPipelineState)
            m_FrameContexts[m_frameIndex].commandList->SetPipelineState(m_CurrentPipelineState.Get());

        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        setupPipelineTime += duration.count();

        if (!pipelineReady || !m_CurrentPipelineState)
            continue;

        start = std::chrono::high_resolution_clock::now();
        DrawMesh(*mesh);
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        drawMeshTime += duration.count();
    }

    Logger::Log("BindObject Time: " + std::to_string(bindObjectTime) + " microseconds");
    Logger::Log("BindMaterial Time: " + std::to_string(bindMaterialTime) + " microseconds");
    Logger::Log("SetupPipeline Time: " + std::to_string(setupPipelineTime) + " microseconds");
    Logger::Log("DrawMesh Time: " + std::to_string(drawMeshTime) + " microseconds");
}
void DX12Backend::EndDraw()
{
	ID3D12GraphicsCommandList* commandList = m_FrameContexts[m_frameIndex].commandList.Get();

	// BackBufferへのコピー
	size_t backBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

	// RTVの状態をCOPY_SOURCEに変更
	std::array<D3D12_RESOURCE_BARRIER, 2> barriers{};
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
		m_sceneColors[m_sceneColorIndex].resource.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_COPY_SOURCE);
	// BackBufferの状態をCOPY_DESTに変更
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
		m_BackBuffers[backBufferIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_COPY_DEST);

	commandList->ResourceBarrier(barriers.size(), barriers.data());

	commandList->CopyResource(
		m_BackBuffers[backBufferIndex].Get(),
		m_sceneColors[m_sceneColorIndex].resource.Get());

	// RTVの状態をRENDER_TARGETに戻す
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
		m_sceneColors[m_sceneColorIndex].resource.Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	// BackBufferの状態をPRESENTに戻す
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
		m_BackBuffers[backBufferIndex].Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PRESENT);

	commandList->ResourceBarrier(barriers.size(), barriers.data());

	// コマンドリストのクローズ
	commandList->Close();

	// コマンドリストの実行
	ID3D12CommandList* commandLists[] = { commandList };
	m_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

	// シグナルの送信
	const UINT64 fenceValue = GetNextFenceValue();
	if (!CheckResult(m_CommandQueue->Signal(m_Fence.Get(), fenceValue), L"Failed to signal frame fence."))
	{
		m_IsInitialized = false;
		return;
	}
	m_FrameContexts[m_frameIndex].fenceValue = m_globalFenceValue;

	// Present

	UINT presentFlags = 0;
	if(m_AllowTearing)
		presentFlags |= DXGI_PRESENT_ALLOW_TEARING;

	m_SwapChain->Present(0, presentFlags);

    m_currentFrameData.clear();
}

bool DX12Backend::UploadBufferToDefault(
    const void* data,
    size_t byteSize,
    D3D12_RESOURCE_STATES finalState,
    ComPtr<ID3D12Resource>& resource)
{
    if (!m_Device || !m_CommandQueue || !m_Fence || !data || byteSize == 0)
        return false;

    // Default Heap と転送用 Upload Heap を作成
    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(byteSize));
    resource.Reset();

    HRESULT hr = m_Device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(resource.GetAddressOf()));

    if (!CheckResult(hr, L"Failed to create default buffer."))
        return false;

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    ComPtr<ID3D12Resource> uploadResource;
    hr = m_Device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadResource.GetAddressOf()));
    if (!CheckResult(hr, L"Failed to create buffer upload resource."))
        return false;

    void* mappedData = nullptr;
    hr = uploadResource->Map(0, nullptr, &mappedData);
    if (!CheckResult(hr, L"Failed to map buffer upload resource."))
        return false;

    // 呼び出し元のデータを Upload Resource へコピー
    memcpy(mappedData, data, byteSize);
    const D3D12_RANGE writtenRange{ 0, byteSize };
    uploadResource->Unmap(0, nullptr);

    int contextIndex = -1;
    if (!m_UploadContextPool.GetFreeIndex(contextIndex))
        return false;

    const UINT64 fenceValue = GetNextFenceValue();
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
    if (!CheckResult(hr, L"Failed to close buffer upload command list."))
        return false;

    ID3D12CommandList* commandLists[] = { commandList.Get() };
    m_CommandQueue->ExecuteCommandLists(1, commandLists);
    // Copy 完了 Fence まで両 Resource を保持
    m_WaitingResources.push({ uploadResource, fenceValue });
    m_WaitingResources.push({ resource, fenceValue });

    if (!CheckResult(m_CommandQueue->Signal(m_Fence.Get(), fenceValue), L"Failed to signal buffer upload."))
        return false;

    return true;
}

bool DX12Backend::CreateVertexStreamBuffer(
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
    if (!UploadBufferToDefault(
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

bool DX12Backend::CreateIndexBuffer(
    const uint32_t* indices,
    size_t indexCount,
    ComPtr<ID3D12Resource>& indexBuffer,
    D3D12_INDEX_BUFFER_VIEW& indexBufferView)
{
    if (!indices || indexCount == 0 ||
        indexCount > std::numeric_limits<UINT>::max() / sizeof(uint32_t))
        return false;

    const size_t byteSize = sizeof(uint32_t) * indexCount;
    if (!UploadBufferToDefault(
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

bool DX12Backend::EnsureDefaultStreamBuffers(size_t vertexCount)
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
        std::vector<NormalTangentVertex> vertices(vertexCount);
        for (auto& vertex : vertices)
        {
            vertex.m_normal = { 0.0f, 0.0f, -1.0f };
            vertex.m_tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
        }

        DXVertexBuffer buffer;
        if (!CreateVertexStreamBuffer(VertexStream::NormalTangent, vertices.data(), vertexCount, buffer))
            return false;
        RetireResource(std::move(m_defaultStreamBuffers[normalIndex].m_resource));
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
        if (!CreateVertexStreamBuffer(VertexStream::PrimaryTexCoord, vertices.data(), vertexCount, buffer))
            return false;
        RetireResource(std::move(m_defaultStreamBuffers[uvIndex].m_resource));
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
        if (!CreateVertexStreamBuffer(VertexStream::Color, vertices.data(), vertexCount, buffer))
            return false;
        RetireResource(std::move(m_defaultStreamBuffers[colorIndex].m_resource));
        m_defaultStreamBuffers[colorIndex] = std::move(buffer);
        m_defaultStreamCapacities[colorIndex] = vertexCount;
    }

    return true;
}

void DX12Backend::RetireResource(ComPtr<ID3D12Resource> resource)
{
    if (!resource || !m_CommandQueue || !m_Fence || m_globalFenceValue == 0 ||
        m_Fence->GetCompletedValue() >= m_globalFenceValue)
        return;

    m_WaitingResources.push({ std::move(resource), m_globalFenceValue });
}

bool DX12Backend::UploadTextureResource(
    DXTexture& texture,
    const TextureDesc& desc,
    const void* pixels,
    size_t rowPitch,
    size_t slicePitch)
{
    if (!m_Device || !m_CommandQueue || !m_Fence || !pixels)
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

    HRESULT hr = m_Device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(texture.m_resource.GetAddressOf()));
    if (!CheckResult(hr, L"Failed to create texture resource."))
        return false;

    // CPU 画素を Upload Resource 経由で Default Texture へ転送
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.m_resource.Get(), 0, 1);
    if (uploadBufferSize == 0)
        return false;

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    const auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    ComPtr<ID3D12Resource> uploadResource;
    hr = m_Device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadResource.GetAddressOf()));
    if (!CheckResult(hr, L"Failed to create texture upload resource."))
        return false;

    int contextIndex = -1;
    if (!m_UploadContextPool.GetFreeIndex(contextIndex))
        return false;

    const UINT64 fenceValue = GetNextFenceValue();
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
    if (!CheckResult(hr, L"Failed to close texture upload command list."))
        return false;

    ID3D12CommandList* commandLists[] = { commandList.Get() };
    m_CommandQueue->ExecuteCommandLists(1, commandLists);
    m_WaitingResources.push({ uploadResource, fenceValue });
    m_WaitingResources.push({ texture.m_resource, fenceValue });

    if (!CheckResult(m_CommandQueue->Signal(m_Fence.Get(), fenceValue), L"Failed to signal texture upload."))
        return false;

    // 成功した Texture に SRV を割り当てる
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = desc.m_mipLevels;
    return CreateShaderResourceView(texture.m_resource.Get(), texture.m_masterSRVIndex, &srvDesc);
}

bool DX12Backend::CreateWhiteTexture()
{
    TextureDesc desc;
    desc.m_width = 1;
    desc.m_height = 1;
    desc.m_mipLevels = 1;
    desc.m_format = TextureFormat::RGBA8Unorm;
    const std::array<uint8_t, 4> pixels{ 255, 255, 255, 255 };
    return UploadTextureResource(m_whiteTexture, desc, pixels.data(), pixels.size(), pixels.size());
}

bool DX12Backend::UploadMesh(AssetHandle<Mesh> handle, const Mesh& mesh)
{
    if (!m_IsInitialized || !handle.IsValid() ||
        handle.m_index != mesh.m_handle.m_index ||
        handle.m_generation != mesh.m_handle.m_generation ||
        !mesh.IsValid() || !m_gpuMeshes.CanRegister(handle))
        return false;

    DXMesh gpuMesh;
    if (!CreateMesh(mesh, gpuMesh))
        return false;

    return m_gpuMeshes.Register(handle, std::move(gpuMesh));
}

bool DX12Backend::UploadTexture(
    AssetHandle<Texture> handle,
    const TextureDesc& desc,
    const void* pixels,
    size_t rowPitch,
    size_t slicePitch)
{
    if (!m_IsInitialized || !handle.IsValid() || !desc.IsValid() ||
        !pixels || !m_gpuTextures.CanRegister(handle))
        return false;

    DXTexture gpuTexture;
    if (!UploadTextureResource(gpuTexture, desc, pixels, rowPitch, slicePitch))
        return false;

    return m_gpuTextures.Register(handle, std::move(gpuTexture));
}

bool DX12Backend::HasMesh(AssetHandle<Mesh> handle) const
{
    return m_gpuMeshes.Get(handle) != nullptr;
}
bool DX12Backend::SetupPipeline(std::filesystem::path vsPath, std::filesystem::path psPath)
{
	// PipelineCacheの確認
	std::string cacheKey = vsPath.string() + "|" + psPath.string();
	auto it = m_PipelineCache.find(cacheKey);
	if( it != m_PipelineCache.end())
	{
		m_CurrentPipelineState = it->second;
        return true;
	}

	// Shaderの読み込み
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;

	// csoファイルの読み込み
    if (!LoadShaderFromCSO(vsPath.c_str(), vertexShader)) return false;

	// csoファイルの読み込み
    if (!LoadShaderFromCSO(psPath.c_str(), pixelShader)) return false;

	// GraphicsPipelineStateの作成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = m_RootSignature.Get();

	psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
	psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };

	D3D12_BLEND_DESC blendDesc{};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	psoDesc.BlendState = blendDesc;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = depthStencilDesc;

	psoDesc.InputLayout = m_InputLayoutDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	psoDesc.SampleDesc.Count = 1;

	ComPtr<ID3D12PipelineState> pipelineState;

	HRESULT hr = m_Device->CreateGraphicsPipelineState(
		&psoDesc,
		IID_PPV_ARGS(pipelineState.GetAddressOf()));

    if (!CheckResult(hr, L"Failed to create graphics pipeline state.")) return false;

	// PipelineCacheに登録
	m_PipelineCache[cacheKey] = pipelineState;
	m_CurrentPipelineState = pipelineState;
    return true;

}

void DX12Backend::BindObject(const Matrix4x4_SIMD& worldMatrix, size_t objectIndex)
{
	auto& frame = m_FrameContexts[m_frameIndex];

	// Frame0領域 / Frame1領域 /... FRAME_COUNT領域 ごとに確保するため、
	// frameIndex * MAX_OBJECT_COUNT + objectCountでオフセットを計算する
    const int index = m_frameIndex * MAX_OBJECT_COUNT + objectIndex;

	size_t offset = m_PerObjectCB.SIZE * index;

	// DataをPerObject ConstantBufferに書き込む
	memcpy(
		reinterpret_cast<uint8_t*>(m_PerObjectCB.mappedData) + offset,
		&worldMatrix,
        sizeof(Matrix4x4_SIMD));

	// GPUAddressを設定
	m_FrameContexts[m_frameIndex].commandList
		->SetGraphicsRootConstantBufferView(
			static_cast<UINT>(RootParameterIndex::PerObjectCBV),
			m_PerObjectCB.constantBuffer->GetGPUVirtualAddress() + offset);
}

void DX12Backend::BindMaterial(const Material& material)
{
    auto it = m_PerMaterialCB.materialCache.find(material.name);
    int materialIndex = 0;

    if (it != m_PerMaterialCB.materialCache.end())
    {
        materialIndex = it->second;
    }
    else
    {
        materialIndex = m_PerMaterialCB.maxIndex++;
        m_PerMaterialCB.materialCache[material.name] = materialIndex;
    }

    const int index = m_frameIndex * MAX_MATERIAL_COUNT + materialIndex;
    MaterialConstants materialConstants{
        material.diffuseColor,
        material.specularColor,
        material.emissionColor,
        material.roughness,
        material.metallic
    };

    const size_t offset = m_PerMaterialCB.SIZE * index;

    // DataをPerMaterial ConstantBufferに書き込む
    memcpy(
        reinterpret_cast<uint8_t*>(m_PerMaterialCB.mappedData) + offset,
        &materialConstants,
        sizeof(MaterialConstants));

    // GPUAddressを設定
    m_FrameContexts[m_frameIndex].commandList
        ->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(RootParameterIndex::PerMaterialCBV),
            m_PerMaterialCB.constantBuffer->GetGPUVirtualAddress() + offset);

    // 欠損・不一致 Handle は白 Texture に置き換えて全 Slot を設定
    for (int i = 0; i < MAX_MATERIAL_TEXTURE; ++i)
    {
        const DXTexture* texture = &m_whiteTexture;
        if (static_cast<size_t>(i) < material.textures.size())
        {
            const DXTexture* registered = m_gpuTextures.Get(material.textures[i]);
            if (registered)
                texture = registered;
        }

        m_BoundMaterialSRVs[i] = static_cast<int>(texture->m_masterSRVIndex);
    }

    CommitMaterialTexture();
}

// 4 microseconds の固定負荷
void DX12Backend::CommitMaterialTexture()
{
	// MasterHeapからVisibleHeapにコピー
	auto& frame = m_FrameContexts[m_frameIndex];
	auto visibleStart = frame.srvVisibleHeap->GetCPUDescriptorHandleForHeapStart();
    UINT offset = m_SRVIncrementSize * frame.visibleSRVCount * MAX_MATERIAL_TEXTURE;

	for(int i = 0; i < MAX_MATERIAL_TEXTURE; ++i)
	{
		auto srcHandle = GetSRVHandle(m_BoundMaterialSRVs[i]);

			auto dstHandle = visibleStart;
			dstHandle.ptr += offset + m_SRVIncrementSize * i;

		m_Device->CopyDescriptorsSimple(
			1,
			dstHandle,
			srcHandle,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}


	auto gpuHandle = frame.srvVisibleHeap->GetGPUDescriptorHandleForHeapStart();
	gpuHandle.ptr += offset;

	frame.commandList
		->SetGraphicsRootDescriptorTable(
			static_cast<UINT>(RootParameterIndex::PerMaterialSRV),
			gpuHandle);

    // 初期 Shader は System Texture を使わない。未参照のテーブルにも有効な範囲を設定
    frame.commandList->SetGraphicsRootDescriptorTable(
        static_cast<UINT>(RootParameterIndex::PerSystemSRV), gpuHandle);

	++frame.visibleSRVCount;

}

bool DX12Backend::CreateMesh(const Mesh& mesh, DXMesh& result)
{
    // 必須 Position と存在する Optional Stream を作成
    const size_t vertexCount = mesh.m_positions.size();
    result = {};
    if (!EnsureDefaultStreamBuffers(vertexCount))
        return false;

    const size_t positionIndex = static_cast<size_t>(VertexStream::Position);
    if (!CreateVertexStreamBuffer(
        VertexStream::Position,
        mesh.m_positions.data(),
        vertexCount,
        result.m_streams[positionIndex]))
        return false;

    const size_t normalIndex = static_cast<size_t>(VertexStream::NormalTangent);
    if (!mesh.m_normalTangents.empty() && !CreateVertexStreamBuffer(
        VertexStream::NormalTangent,
        mesh.m_normalTangents.data(),
        mesh.m_normalTangents.size(),
        result.m_streams[normalIndex]))
        return false;

    const size_t uvIndex = static_cast<size_t>(VertexStream::PrimaryTexCoord);
    if (!mesh.m_primaryTexCoords.empty() && !CreateVertexStreamBuffer(
        VertexStream::PrimaryTexCoord,
        mesh.m_primaryTexCoords.data(),
        mesh.m_primaryTexCoords.size(),
        result.m_streams[uvIndex]))
        return false;

    const size_t colorIndex = static_cast<size_t>(VertexStream::Color);
    if (!mesh.m_colors.empty() && !CreateVertexStreamBuffer(
        VertexStream::Color,
        mesh.m_colors.data(),
        mesh.m_colors.size(),
        result.m_streams[colorIndex]))
        return false;

    // Index Buffer を作成
    if (!CreateIndexBuffer(
        mesh.m_indices.data(),
        mesh.m_indices.size(),
        result.m_indexBuffer,
        result.m_indexView))
        return false;

    result.m_vertexCount = static_cast<uint32_t>(vertexCount);
    result.m_indexCount = static_cast<uint32_t>(mesh.m_indices.size());
    return true;
}

void DX12Backend::DrawMesh(const DXMesh& mesh)
{
    std::array<D3D12_VERTEX_BUFFER_VIEW, static_cast<size_t>(VertexStream::Count)> views{};

    for (size_t i = 0; i < views.size(); ++i)
    {
        const DXVertexBuffer* stream = &mesh.m_streams[i];
        if (!stream->m_resource)
        {
            stream = &m_defaultStreamBuffers[i];
        }

        views[i] = stream->m_view;
    }

    auto* commandList = m_FrameContexts[m_frameIndex].commandList.Get();
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, static_cast<UINT>(views.size()), views.data());
    commandList->IASetIndexBuffer(&mesh.m_indexView);
    commandList->DrawIndexedInstanced(mesh.m_indexCount, 1, 0, 0, 0);
}

bool DX12Backend::WaitForGPU()
{
    if (!m_CommandQueue || !m_Fence)
        return true;

    const UINT64 fenceValue = GetNextFenceValue();
    if (FAILED(m_CommandQueue->Signal(m_Fence.Get(), fenceValue)))
        return false;

    if (m_Fence->GetCompletedValue() >= fenceValue)
        return true;

    HANDLE eventHandle = m_FenceEvent;
    const bool ownsEvent = eventHandle == nullptr;
    if (ownsEvent)
        eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!eventHandle)
        return false;

    const bool eventSet = SUCCEEDED(m_Fence->SetEventOnCompletion(fenceValue, eventHandle));
    const bool completed = eventSet &&
        WaitForSingleObject(eventHandle, INFINITE) == WAIT_OBJECT_0 &&
        m_Fence->GetCompletedValue() >= fenceValue;
    if (ownsEvent)
        CloseHandle(eventHandle);

    return completed;
}
bool DX12Backend::UploadContextPool::CreateUploadContext(UploadContext& context)
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
