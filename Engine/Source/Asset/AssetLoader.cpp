/*=============================================================================

 File   : AssetLoader.cpp
 Desc   : Mesh / Texture Loader の初期化とロード境界を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Asset/AssetLoader.h"

#include <filesystem>
#include <memory>
#include <string>

#include "Asset/AssetSystem.h"
#include "DirectXTex/DirectXTex.h"
#include "Graphics/GraphicsSystem.h"
#include "Log/Logger.h"

namespace Hestia
{
    bool AssetLoader<Texture>::Initialize(GraphicsSystem* graphics)
    {
        if (graphics == nullptr)
            return false;

        m_graphics = graphics;
        return true;
    }

    AssetHandle<Mesh> AssetLoader<Mesh>::Load(std::string_view name, std::string_view path)
    {
        Logger::Error("Mesh file loading is not supported");
        return {};
    }

    AssetHandle<Texture> AssetLoader<Texture>::Load(
        std::string_view name, std::string_view path)
    {
        if (m_graphics == nullptr || name.empty() || path.empty())
        {
            Logger::Error("Texture loading requires an initialized loader, name, and path");
            return {};
        }

        // 画像を CPU 上でデコード
        const std::filesystem::path filePath{ std::string(path) };
        DirectX::TexMetadata metadata = {};
        DirectX::ScratchImage decodedImage;
        const HRESULT loadResult = DirectX::LoadFromWICFile(
            filePath.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, decodedImage);
        if (FAILED(loadResult) || metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D ||
            metadata.arraySize != 1 || metadata.mipLevels != 1 ||
            metadata.width > std::numeric_limits<uint32_t>::max() ||
            metadata.height > std::numeric_limits<uint32_t>::max())
        {
            Logger::Error("Texture file could not be decoded as a single 2D image");
            return {};
        }

        const bool isSrgb = DirectX::IsSRGB(metadata.format);
        const DXGI_FORMAT targetFormat = isSrgb
            ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            : DXGI_FORMAT_R8G8B8A8_UNORM;
        const DirectX::Image* sourceImage = decodedImage.GetImage(0, 0, 0);
        if (sourceImage == nullptr)
        {
            Logger::Error("Texture decoder returned no image data");
            return {};
        }

        DirectX::ScratchImage convertedImage;
        const DirectX::Image* uploadImage = sourceImage;
        if (sourceImage->format != targetFormat)
        {
            const HRESULT convertResult = DirectX::Convert(
                *sourceImage,
                targetFormat,
                DirectX::TEX_FILTER_DEFAULT,
                DirectX::TEX_THRESHOLD_DEFAULT,
                convertedImage);
            if (FAILED(convertResult))
            {
                Logger::Error("Texture pixels could not be converted to RGBA8");
                return {};
            }

            uploadImage = convertedImage.GetImage(0, 0, 0);
            if (uploadImage == nullptr)
            {
                Logger::Error("Texture conversion returned no image data");
                return {};
            }
        }

        auto texture = std::make_unique<Texture>();
        texture->m_desc.m_width = static_cast<uint32_t>(metadata.width);
        texture->m_desc.m_height = static_cast<uint32_t>(metadata.height);
        texture->m_desc.m_mipLevels = 1;
        texture->m_desc.m_format = isSrgb ? TextureFormat::RGBA8Srgb : TextureFormat::RGBA8Unorm;
        if (!texture->m_desc.IsValid())
        {
            Logger::Error("Texture description is invalid");
            return {};
        }

        const TextureDesc textureDesc = texture->m_desc;
        const AssetHandle<Texture> handle = AssetSystem::RegisterFromLoader(
            name, std::move(texture));
        if (!handle.IsValid())
            return {};

        if (!m_graphics->UploadTexture(
            handle,
            textureDesc,
            uploadImage->pixels,
            uploadImage->rowPitch,
            uploadImage->slicePitch))
            Logger::Error("Texture was cached on CPU but GPU upload failed");

        return handle;
    }
}
