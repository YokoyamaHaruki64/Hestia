/*=============================================================================

 File   : TextureManager.h
 Desc   : テクスチャ管理の宣言    
          DX12におけるテクスチャ管理用のスタブであり、後に汎用Asset管理システムへ移行

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/
#ifndef _TEXTURE_MANAGER_H_
#define _TEXTURE_MANAGER_H_

#include "Graphics/DX12Backend.h"
#include "Graphics/DX12Object.h"
#include <unordered_map>
#include <filesystem>
#include <vector>


namespace Hestia
{

    class TextureManager
    {
        static std::vector<DXTexture> m_Textures;
        static std::unordered_map<std::string, TextureHandle> m_TextureCache;

        static DXTexture m_InvalidTexture; // 無効なテクスチャを表すDxTexture

        static DX12Backend* m_Graphics;

    public:
        static void Initialize(DX12Backend* graphics)
        {
            m_Graphics = graphics;
        }

        /// @brief GPU 完了後にキャッシュと Backend の非所有参照を解除する
        static void Finalize();

        static TextureHandle LoadTexture(const std::filesystem::path& path);
        static const DXTexture& GetTexture(TextureHandle handle);
    };

}
#endif // _TEXTURE_MANAGER_H_
