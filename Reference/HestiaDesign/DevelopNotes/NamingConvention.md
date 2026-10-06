# Naming Convention
HestiaEngineで使用する名前空間とC++識別子の命名規約。
必要なルールだけを定義し、細かすぎる規約は必要になった時点で追加する。

## Namespace
### Hestia
Engine.dll内部の型、およびGame.dllとのDLL境界で共有する型は `Hestia` 名前空間に置く。

```cpp
Hestia::Engine
Hestia::Time
Hestia::TimeAPI
Hestia::AssetHandle
```

Subsystemごとの細かい名前空間は原則作らない。

```cpp
// 原則避ける
Hestia::Engine::Graphics::Resource::Texture
```

### HestiaGame
Game.dll側でGame Scriptへ公開するstatic Facadeは `HestiaGame` 名前空間に置く。

```cpp
HestiaGame::Time
HestiaGame::Assets
HestiaGame::Graphics
```

Script専用共通Headerでは、Game Scriptから利用する共有型とFacadeを簡潔に扱うため、以下を使用してよい。

```cpp
using namespace Hestia;
using namespace HestiaGame;
```

Engine内部実装型はGame.dll向け共有Headerへ公開しないため、同名のEngine内部型とGame FacadeがScript側で衝突しない構成とする。

## Identifier
| 対象 | 形式 | 例 |
| --- | --- | --- |
| Namespace | UpperCamel | `HestiaGame` |
| Class / Struct | UpperCamel | `GraphicsSystem` |
| Type Alias | UpperCamel | `EntityID` |
| Enum | UpperCamel | `RenderPhase` |
| Enum Value | UpperCamel | `Transparent` |
| Function | UpperCamel | `SetTimeScale()` |
| Constant | UPPER_SNAKE_CASE | `MAX_ENTITY_COUNT` |
| Macro | UPPER_SNAKE_CASE | `HESTIA_EDITOR` |
| Member Variable | `m_lowerCamel` | `m_timeScale` |
| Static Member Variable | `s_lowerCamel` | `s_instance` |
| Local Variable | lowerCamel | `deltaTime` |
| Parameter | lowerCamel | `timeScale` |
| File Name | UpperCamel | `GraphicsSystem.h` |

`const` や `constexpr` であっても、一時的なローカル変数は通常の `lowerCamel` とする。
名前付きの定数として定義するものは `UPPER_SNAKE_CASE` とする。

## API Table
APIテーブル内の公開関数ポインタは、データメンバではなく公開関数相当として扱い、関数名と同じ `UpperCamel` を使用する。

```cpp
struct EngineAPI
{
    Engine* (*Create)(const ApplicationAPI*);
    void (*Destroy)(Engine*);
    void (*FrameExecute)(Engine*, float);
};
```

APIテーブル内部で保持するContextやCallbackなど、実装用の通常メンバは `m_lowerCamel` を使用する。
