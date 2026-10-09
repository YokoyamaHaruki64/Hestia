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
#include "Graphics/TextureManager.h"

#include "Time/TimeSystem.h"
#include "Log/LogSystem.h"

#include <dxgidebug.h>

using namespace Hestia;

constexpr UINT Align256(UINT size)
{
	// 256の倍数に切り上げる
	// ~255 は 0xFFFFFF00 で、下位8ビット(256)を0にするマスク
	return (size + 255) & ~255;
}

bool DX12Backend::CreateShaderResourceView(
	ID3D12Resource * resource,
	int& srvHeapIndex, 
	D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc)
{
	if(m_SRVMasterHeapIndex >= MAX_MASTER_SRV_COUNT)
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
	srvHeapIndex = m_SRVMasterHeapIndex;

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

    // NullTextureの作成
    D3D12_SHADER_RESOURCE_VIEW_DESC nullDesc{};
    nullDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    nullDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    nullDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    nullDesc.Texture2D.MipLevels = 1;
    if (!CreateShaderResourceView(nullptr, m_nullTexture.srvHeapIndex, &nullDesc)) return false;

    ResetMaterialTextureBinding();

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


	m_Fence->Signal(0);

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
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
	// GPUの完了を待機
	if (m_CommandQueue && m_Fence && m_FenceEvent)
	{
		WaitForGPU();
	}

	// FenceEventのクローズ
	if (m_FenceEvent)
	{
		CloseHandle(m_FenceEvent);
		m_FenceEvent = nullptr;
	}

	m_IsInitialized = false;
    m_currentFrameData.clear();
    for (auto& frame : m_FrameContexts)
        frame.drewMesh.clear();

}

void DX12Backend::Update()
{
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
	if( frame.fenceValue != 0 && m_Fence->GetCompletedValue() < frame.fenceValue)
	{
		m_Fence->SetEventOnCompletion(frame.fenceValue, m_FenceEvent);
		WaitForSingleObject(m_FenceEvent, INFINITE);
	}

	// コマンドアロケータとコマンドリストのリセット
    frame.drewMesh.clear();
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

    for(int i = 0; i < m_currentFrameData.size(); ++i)
    {
        RenderData& data = m_currentFrameData[i];
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
        SetupPipeline(data.vsShaderPath, data.psShaderPath);
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        setupPipelineTime += duration.count();
        m_FrameContexts[m_frameIndex].drewMesh.push_back(data.mesh);
        start = std::chrono::high_resolution_clock::now();
        DrawMesh(data.mesh);
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
	m_CommandQueue->Signal(m_Fence.Get(), GetNextFenceValue());
	m_FrameContexts[m_frameIndex].fenceValue = m_globalFenceValue;

	// Present

	UINT presentFlags = 0;
	if(m_AllowTearing)
		presentFlags |= DXGI_PRESENT_ALLOW_TEARING;

	m_SwapChain->Present(0, presentFlags);

    m_currentFrameData.clear();
}

bool DX12Backend::CreateVertexBuffer(
    const Vertex* vertices,
	size_t vertexCount, 
	ComPtr<ID3D12Resource>& vertexBuffer,
	D3D12_VERTEX_BUFFER_VIEW& vertexBufferView)
{
	// 頂点バッファの作成
	D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;

	resourceDesc.Width = sizeof(Vertex) * vertexCount;
	resourceDesc.Height = 1;

	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;

	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;

	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT hr = m_Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(vertexBuffer.GetAddressOf()));

	if (!CheckResult(hr, L"Failed to create vertex buffer.")) return false;

	// VertexBufferViewの設定
	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(Vertex) * vertexCount);
	vertexBufferView.StrideInBytes = sizeof(Vertex);

	// 頂点バッファのマッピング
	void* pVertexDataBegin = nullptr;
	hr = vertexBuffer->Map(0, nullptr, &pVertexDataBegin);
	if (!CheckResult(hr, L"Failed to map vertex buffer.")) return false;

	memcpy(pVertexDataBegin, vertices, sizeof(Vertex) * vertexCount);
	vertexBuffer->Unmap(0, nullptr);

	return true;
}

bool DX12Backend::CreateIndexBuffer(
    const uint32_t* indices,
	size_t indexCount,
	ComPtr<ID3D12Resource>& indexBuffer,
	D3D12_INDEX_BUFFER_VIEW& indexBufferView)
{
	// インデックスバッファの作成
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;

    resourceDesc.Width = sizeof(uint32_t) * indexCount;
	resourceDesc.Height = 1;

	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;

	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;

	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT hr = m_Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(indexBuffer.GetAddressOf()));

	if (!CheckResult(hr, L"Failed to create index buffer.")) return false;

	// IndexBufferViewの設定
	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    indexBufferView.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indexCount);
    indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	// インデックスバッファのマッピング
	void* pIndexDataBegin = nullptr;
	hr = indexBuffer->Map(0, nullptr, &pIndexDataBegin);
	if (!CheckResult(hr, L"Failed to map index buffer.")) return false;

    memcpy(pIndexDataBegin, indices, sizeof(uint32_t) * indexCount);
	indexBuffer->Unmap(0, nullptr);

	return true;
}

bool DX12Backend::UploadTexture(DXTexture& texture, const std::vector<D3D12_SUBRESOURCE_DATA>& subresources)
{
	UINT64 uploadBufferSize =
		GetRequiredIntermediateSize(texture.resource.Get(), 0, subresources.size());

	// UploadResourceの作成
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	auto uploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

	ComPtr<ID3D12Resource> uploadResource;

	HRESULT hr = m_Device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&uploadResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadResource.GetAddressOf()));

	if (!CheckResult(hr, L"Failed to create upload resource.")) return false;

	// CommandListの取得
	int freeIndex;
	if (!m_UploadContextPool.GetFreeIndex(freeIndex)) return false;

	const UINT64 fenceValue = GetNextFenceValue();
	auto commandList = m_UploadContextPool.GetResetCommandList(freeIndex, fenceValue);
	if (!commandList) return false;

	// データのコピー:UpdateSubresourcesは以下2つを行う
	// uploadResourceにsubresourcesのデータをコピー
	// texture.resourceにuploadResourceのデータをコピーするコマンド発行(CopyTextureRegion)
    const UINT64 copiedBytes = UpdateSubresources(
		commandList.Get(),
		texture.resource.Get(),
		uploadResource.Get(),
		0, 0,
		subresources.size(),
		subresources.data());
    if (copiedBytes == 0)
    {
        commandList->Close();
        return false;
    }

	// CopyTextureRegionがUploadResourceを参照するため、CopyTextureRegionの完了までUploadResourceを解放しないようにする
	m_WaitingResources.push({ uploadResource, fenceValue });
    m_WaitingResources.push({ texture.resource, fenceValue });

	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		texture.resource.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	commandList->ResourceBarrier(1,&barrier);

	// コマンドリストのクローズ
	hr = commandList->Close();
	if (!CheckResult(hr, L"Failed to close command list.")) return false;

	// コマンドリストの実行
	ID3D12CommandList* commandLists[] = { commandList.Get() };
	m_CommandQueue->ExecuteCommandLists(1,commandLists);

	// フェンスのシグナル送信
    if (!CheckResult(m_CommandQueue->Signal(m_Fence.Get(), fenceValue), L"Failed to signal texture upload.")) return false;
	
	// SRVの作成
	if(!CreateShaderResourceView(texture.resource.Get(), texture.srvHeapIndex))
	{
		MessageBox(nullptr, L"Failed to create SRV for texture.", L"Error", MB_OK);
		return false;
	}

	return true;
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
    auto& frame = m_FrameContexts[m_frameIndex];
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

    int index = m_frameIndex * MAX_MATERIAL_COUNT + materialIndex;
	MaterialConstants materialConstants{
		material.diffuseColor,
		material.specularColor,
		material.emissionColor,
		material.roughness,
		material.metallic
	};

    size_t offset = m_PerMaterialCB.SIZE * index;

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


    // Texture設定
    for(int i = 0; i < MAX_MATERIAL_TEXTURE; ++i)
    {
        if(i < material.textures.size())
            BindMaterialTexture(i, TextureManager::GetTexture(material.textures[i]));
        else
            BindMaterialTexture(i, m_nullTexture);
    }

    CommitMaterialTexture();
}

void DX12Backend::BindMaterialTexture(int slot, const DXTexture& texture)
{
	if(slot < 0 || slot >= MAX_MATERIAL_TEXTURE)
	{
		MessageBox(nullptr, L"Invalid material texture slot.", L"Error", MB_OK);
		return;
	}

	m_BoundMaterialSRVs[slot] = texture.srvHeapIndex;
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
		if (m_BoundMaterialSRVs[i] >= 0)
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

void DX12Backend::ResetMaterialTextureBinding()
{
	m_BoundMaterialSRVs.fill(m_nullTexture.srvHeapIndex);
}

bool DX12Backend::CreateMesh(const Mesh& mesh, DXMesh& result)
{
    if(!CreateVertexBuffer(
        mesh.vertices.data(),mesh.vertices.size(),
        result.vertexBuffer,result.vertexBufferView))
        return false;

    if (!CreateIndexBuffer(
        mesh.indices.data(), mesh.indices.size(),
        result.indexBuffer, result.indexBufferView))
        return false;

    result.indexCount = static_cast<UINT>(mesh.indices.size());

    return true;
}

void DX12Backend::DrawMesh(const DXMesh& mesh)
{
    auto* cmd = m_FrameContexts[m_frameIndex].commandList.Get();
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
    cmd->IASetIndexBuffer(&mesh.indexBufferView);
    cmd->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
}

void DX12Backend::WaitForGPU()
{
	const UINT64 fenceValue = GetNextFenceValue();

    if (FAILED(m_CommandQueue->Signal(m_Fence.Get(), fenceValue)))
        return;

	if (m_Fence->GetCompletedValue() < fenceValue)
	{
        if (FAILED(m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent)))
            return;

		WaitForSingleObject(
			m_FenceEvent,
			INFINITE);
	}
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
