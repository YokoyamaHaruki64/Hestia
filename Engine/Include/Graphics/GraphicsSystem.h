/*=============================================================================

 File   : GraphicsSystem.h
 Desc   : GraphicsSystem クラスの宣言。Engineのグラフィックス関連の処理を管理する。

 ------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

===============================================================================*/

#ifndef _GRAPHICS_SYSTEM_H_
#define _GRAPHICS_SYSTEM_H_

#include "Graphics/DX12Object.h"
#include <filesystem>
#include <memory>
#include "Common/Include/WindowsHeaders.h"


namespace Hestia
{
    class DX12Backend;

    class GraphicsSystem
    {
    public:

        /// @brief 一件の描画要求を次の Render へ提出する
        /// @return 受付成功: true / 未初期化・容量超過・無効な Mesh: false
        static bool Submit(RenderData renderData);

        /// @brief 次の Render が使用する行ベクトル用 View を設定する
        static void SetView(const Matrix4x4& view);

        /// @brief 次の Render が使用する行ベクトル用 Projection を設定する
        static void SetProj(const Matrix4x4& projection);

        /// @brief CPU Mesh を GPU へ登録する仮の Engine 内部 API
        /// @return 登録成功: true / 未初期化・無効な入力・GPU 資源作成失敗: false
        static bool CreateMesh(const Mesh& mesh, DXMesh& result);

        /// @brief 画像を読み込み、GPU Texture の仮ハンドルを返す
        /// @return 成功: 0 以上 / 未初期化・読み込み失敗: -1
        static int LoadTexture(const std::filesystem::path& path);

        GraphicsSystem();
        ~GraphicsSystem();
        GraphicsSystem(const GraphicsSystem&) = delete;
        GraphicsSystem& operator=(const GraphicsSystem&) = delete;

        /// @brief Window と固定 Shader 一組を使って初期化する
        /// @param hWnd ウィンドウのハンドル
        /// @param width ウィンドウの幅
        /// @param height ウィンドウの高さ
        /// @note 同じスレッドで初期化・資源登録・描画・終了する。Resize は未対応
        bool Initialize(HWND hWnd, int width, int height);
        void Finalize();
        void Render();

    private:
        inline static GraphicsSystem* s_instance = nullptr;

        std::vector<RenderData> m_renderQueue;
        Matrix4x4 m_view = Matrix4x4::Identity();
        Matrix4x4 m_projection = Matrix4x4::Identity();

        std::unique_ptr<DX12Backend> m_DX12Backend;
    };
}

#endif // _GRAPHICS_SYSTEM_H_
