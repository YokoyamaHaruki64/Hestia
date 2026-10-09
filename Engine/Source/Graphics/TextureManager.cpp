/*=============================================================================

 File   : TextureManager.cpp
 Desc   : テクスチャ管理の実装
          DX12におけるテクスチャ管理用のスタブであり、後に汎用Asset管理システムへ移行

------------------------------------------------------------------------------

 Date   : 2026/10/02
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Graphics/TextureManager.h"
#include "DirectXTex/DirectXTex.h"
#include "D3DX12/d3dx12.h"

using namespace DirectX;
using namespace Hestia;

std::vector<DXTexture> TextureManager::m_Textures;
std::unordered_map<std::string, TextureHandle> TextureManager::m_TextureCache;
DX12Backend* TextureManager::m_Graphics = nullptr;
DXTexture TextureManager::m_InvalidTexture; // 無効なテクスチャを表すDxTexture

void TextureManager::Finalize()
{
    m_Textures.clear();
    m_TextureCache.clear();
    m_InvalidTexture = DXTexture{};
    m_Graphics = nullptr;
}

TextureHandle TextureManager::LoadTexture(const std::filesystem::path& path)
{
    if (!m_Graphics || !m_Graphics->GetDevice())
        return -1;
	// 既に読み込まれている場合はキャッシュから取得
	auto it = m_TextureCache.find(path.string());
	if (it != m_TextureCache.end())
	{
		return it->second;
	}

	// 読み込み
	ScratchImage image;
	TexMetadata metadata;
	HRESULT hr = LoadFromWICFile(
		path.c_str(),
		WIC_FLAGS_NONE,
		&metadata,
		image
	);

	auto CheckResult = [](HRESULT hr, const std::wstring& message) -> bool
	{
		if (FAILED(hr))
		{
			MessageBox(nullptr, message.c_str(), L"Error", MB_OK);
			return false;
		}
		return true;
	};

	if (!CheckResult(hr, L"Failed to load texture: " + path.wstring()))return -1;

	// DxTextureの作成
	ComPtr<ID3D12Resource> textureResource;

	auto* device = m_Graphics->GetDevice();

	// テクスチャリソースの作成
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		metadata.format,
		static_cast<UINT64>(metadata.width),
		static_cast<UINT>(metadata.height),
		static_cast<UINT16>(metadata.arraySize),
		static_cast<UINT16>(metadata.mipLevels)
	);

	hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(textureResource.GetAddressOf())
	);
	if (!CheckResult(hr, L"Failed to create texture resource: " + path.wstring())) return -1;

	// Upload用のデータを作成
	// ここでsubresourcesに各ミップマップレベルのデータが格納される
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	hr = PrepareUpload(
		device,
		image.GetImages(),
		image.GetImageCount(),
		metadata,
		subresources
	);
	if (!CheckResult(hr, L"Failed to prepare texture upload: " + path.wstring())) return -1;

	DXTexture dxTexture;
	dxTexture.resource = textureResource;
    if (!m_Graphics->UploadTexture(dxTexture, subresources))
        return -1;

	// DxTextureをリストに追加し、ハンドルを返す
	m_Textures.push_back(dxTexture);
	m_TextureCache[path.string()] = static_cast<TextureHandle>(m_Textures.size() - 1);
	return static_cast<TextureHandle>(m_Textures.size() - 1);
}

const DXTexture& TextureManager::GetTexture(TextureHandle handle)
{
	if(handle < 0)
	{
		MessageBox(nullptr, L"Invalid texture handle.", L"Error", MB_OK);
		return m_InvalidTexture;
	}
	else if(handle >= m_Textures.size())
	{
		MessageBox(nullptr, L"Texture handle out of range.", L"Error", MB_OK);
		return m_InvalidTexture;
	}

	return m_Textures[handle];
}
