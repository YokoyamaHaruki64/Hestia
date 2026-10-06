#ifndef _APP_PCH_H_
#define _APP_PCH_H_

// Windows の min/max マクロと不要な追加ヘッダーの取り込みを抑える。
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Windows / COM
#include <Windows.h>
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
