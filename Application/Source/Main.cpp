/*=============================================================================

 File   : Main.cpp
 Desc   : Application のエントリーポイントと実行ライフサイクルを定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/05
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Application.h"

int APIENTRY wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine,
    int nCmdShow)
{
    Application app;

    if (!app.Initialize(nCmdShow)) return -1;

    const int exitCode = app.Run();

    app.Finalize();

    return exitCode;
}
