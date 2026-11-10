/*=============================================================================

 File   : DX12Backend.h
 Desc   : GraphicsSystem と DX12 の機能クラスを接続する。

------------------------------------------------------------------------------

 Date   : 2026/09/30
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _DX12_BACKEND_H_
#define _DX12_BACKEND_H_

#include "Common/Include/WindowsHeaders.h"
#include "Graphics/DX12Device.h"
#include "Graphics/DX12Descriptors.h"
#include "Graphics/DX12Presentation.h"
#include "Graphics/DX12Resources.h"
#include "Graphics/DX12Pipelines.h"
#include "Graphics/DX12Frames.h"
#include "Graphics/ForwardRenderPass.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

namespace Hestia
{
    /// @brief GraphicsSystem からの要求を受け、各機能クラスの初期化順と連携を管理する。
    class DX12Backend
    {
        bool m_IsInitialized = false;
        DX12Device m_device;
        DX12Descriptors m_descriptors;
        DX12Presentation m_presentation;
        DX12Resources m_resources;
        DX12Pipelines m_pipelines;
        DX12Frames m_frames;
        ForwardRenderPass m_forwardPass;
        std::vector<RenderData> m_submissions;

    public:
        static constexpr size_t MAX_DRAW_COUNT = DX12_MAX_MATERIAL_COUNT;

        DX12Backend() = default;
        ~DX12Backend()
        {
            Finalize();
        };

        bool Initialize(HWND hWnd, int width, int height);
        void Finalize();
        void Update();

        /// @brief SIMD版のViewとProjectionを使ってフレームの描画記録を開始する
        bool BeginDraw(std::vector<RenderData>& renderData, const Matrix4x4_SIMD& view,
            const Matrix4x4_SIMD& projection);

        void Draw();
        void EndDraw();

        /// @brief CPU Mesh の Stream と Index を Default Heap Buffer へ登録
        /// @param handle CPU Mesh と共有する Handle
        /// @param mesh 登録する Mesh の Stream と Index
        /// @note Handle、Mesh 内容、重複登録を実行時に確認する
        /// @return 成功: true / 無効または不一致 Handle・不正 Mesh・重複 Handle・GPU 作成失敗: false
        bool UploadMesh(AssetHandle<Mesh> handle, const Mesh& mesh);

        /// @brief RGBA8 Texture を作成して画素を同じ Direct Queue へ送信
        /// @param handle CPU Texture と共有する Handle
        /// @param desc 画像寸法、Format、Mip 数
        /// @param pixels 先頭 Mip の画素データ
        /// @param rowPitch 1 行のバイト数
        /// @param slicePitch 画素データ全体のバイト数
        /// @return 成功: true / 無効 Handle・不正 pitch・未初期化・重複 Handle・GPU 作成失敗: false
        /// @note 復帰前に画素を Upload Resource へコピーし、Fence 完了まで同 Resource を保持
        bool UploadTexture(
            AssetHandle<Texture> handle,
            const TextureDesc& desc,
            const void* pixels,
            size_t rowPitch,
            size_t slicePitch);

        ID3D12GraphicsCommandList* GetCommandList()
        {
            return m_frames.GetCommandList();
        }

        ID3D12Device* GetDevice()
        {
            return m_device.GetDevice();
        }
    };
}

#endif // _DX12_BACKEND_H_
