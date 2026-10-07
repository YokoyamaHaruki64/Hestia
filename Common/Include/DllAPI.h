/*=============================================================================

 File   : DllAPI.h
 Desc   : DLL ごとの公開 API 属性を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/07
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _DLL_API_H_
#define _DLL_API_H_

// DLL のエクスポート/インポートを定義する。

// テスト時は DLL のエクスポート/インポートを無効化する。
#if defined(HESTIA_TEST) && HESTIA_TEST
#define HESTIA_ENGINE_API
#define HESTIA_GAME_API
#else

// HESTIA_ENGINE_EXPORTS は Engine.dll のビルド時に定義される。
#if defined(HESTIA_ENGINE_EXPORTS)
#define HESTIA_ENGINE_API __declspec(dllexport)
#else
#define HESTIA_ENGINE_API __declspec(dllimport)
#endif

// HESTIA_GAME_EXPORTS は Game.dll のビルド時に定義される。
#if defined(HESTIA_GAME_EXPORTS)
#define HESTIA_GAME_API __declspec(dllexport)
#else
#define HESTIA_GAME_API __declspec(dllimport)
#endif

#endif

#endif // _DLL_API_H_
