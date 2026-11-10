/*=============================================================================

 File   : DX12Support.h
 Desc   : DX12 内部の共通容量と診断。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _DX12_SUPPORT_H_
#define _DX12_SUPPORT_H_

#include "Graphics/DX12Object.h"

namespace Hestia
{
    inline constexpr int DX12_FRAME_COUNT = 2;
    inline constexpr int DX12_MAX_MATERIAL_COUNT = 1 << 15;
    inline constexpr int DX12_MAX_OBJECT_COUNT = 1 << 16;

    // 共通 Root Signature で使用する CBV と Descriptor Table の位置。
    enum class DX12RootParameterIndex { PerFrameCBV, PerObjectCBV, PerMaterialCBV, PerSystemSRV, PerMaterialSRV, InstancesSRV };

    inline bool CheckDX12Result(HRESULT hr, const wchar_t* message)
    {
        if (FAILED(hr))
        {
            MessageBox(nullptr, message, L"Error", MB_OK);
            return false;
        }
        return true;
    }
}

#endif // _DX12_SUPPORT_H_
