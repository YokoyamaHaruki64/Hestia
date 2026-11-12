/*=============================================================================

 File   : DX12Device.h
 Desc   : デバイス、Direct Queue、共有フェンスを管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _DX12_DEVICE_H_
#define _DX12_DEVICE_H_

#include "Graphics/DX12Object.h"
#include <dxgi1_6.h>

namespace Hestia
{
    /// @brief 描画と転送で共用するデバイス、キュー、フェンスを所有する。
    class DX12Device
    {
    public:
        bool CreateDevice(ComPtr<IDXGIFactory7>& dxgiFactory);
        bool CreateQueue();
        bool CreateFence();
        bool WaitForGPU();
        void Finalize();

        ID3D12Device* GetDevice() const { return m_Device.Get(); }
        ID3D12CommandQueue* GetQueue() const { return m_CommandQueue.Get(); }
        ID3D12Fence* GetFence() const { return m_Fence.Get(); }
        HANDLE GetFenceEvent() const { return m_FenceEvent; }

        // 転送とフレーム描画で共通のフェンス番号を発行する。
        UINT64 NextFenceValue() { return ++m_globalFenceValue; }
        UINT64 LastFenceValue() const { return m_globalFenceValue; }

    private:
        ComPtr<ID3D12Device> m_Device;
        ComPtr<ID3D12CommandQueue> m_CommandQueue;
        ComPtr<ID3D12Fence> m_Fence;
        HANDLE m_FenceEvent = nullptr;
        UINT64 m_globalFenceValue = 0;
    };
}

#endif // _DX12_DEVICE_H_
