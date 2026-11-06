/*=============================================================================

 File   : pch.h
 Desc   : Application プロジェクトで共通使用する事前コンパイルヘッダーを定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/05
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _APP_PCH_H_
#define _APP_PCH_H_

// Windows / COM
#include "Common/Include/WindowsHeaders.h"
#include <objbase.h>
#include <wrl/client.h>

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

#endif // _APP_PCH_H_
