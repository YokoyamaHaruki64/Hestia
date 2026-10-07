/*=============================================================================

 File   : pch.h
 Desc   : Engine プロジェクトで共通使用する事前コンパイルヘッダーを定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/06
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ENGINE_PCH_H_
#define _ENGINE_PCH_H_

// Windows / COM
#include "Common/Include/WindowsHeaders.h"
#include <objbase.h>
#include <wrl/client.h>

// DirectX / Direct3D(Engineのみ)
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>

// 基本型・診断
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

// 文字列・コンテナ・View
#include <array>
#include <deque>
#include <list>
#include <queue>
#include <span>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// アルゴリズム・汎用処理・所有
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

// 時間・数学
#include <chrono>
#include <cmath>

// スレッド・同期
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#endif // _ENGINE_PCH_H_
