/*=============================================================================

 File   : DllMain.cpp
 Desc   : Engine DLL のプロセスおよびスレッド通知入口を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/06
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
