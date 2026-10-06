# Asset System
---
Engine が所有する AssetSystem と Game 向け公開面の位置だけを示す。
SubsystemApiFacadeDesign の Asset 例は実際の AssetSystem の仕様として採用せず、具体的な設計は一から検討する。

## AssetSystem
---
**役割**

Engine 内部のアセット状態を保持する System。読み込み対象・保持対象・公開操作の詳細は未確定。

```cpp
namespace Hestia
{
    class AssetSystem
    {
    public:
        // 未確定：lifecycleとアセット操作のPublic API

    private:
        static AssetSystem* s_instance;

        // 未確定：実際に保持するアセット状態の主要メンバ
    };
}
```

[Engine](./Engine.md#engine)  
[AssetAPI](#assetapi)

**所有・参照**：Engine が AssetSystem を値として所有する。s_instance は自身への非所有ポインタで private に置く。System の実体・実体を取得する Getter は public に公開しない（U06）。

**決定（U07）**：以前の Load／IsLoaded／GetDetailedInfo、AssetStorage、AssetID／Handle／Info の宣言は元資料の例に由来するため、現設計から取り下げる。具体的な API や型の採用には使わない。

**未確定**：アセットの保持方法、読み込み・参照・公開情報、必要な lifecycle はまだ設計していない。

**提案**：後続でこのリポジトリの関連情報と必要な用途を繋げ、責務・状態・操作を一から整理する。現段階ではその型や内部クラスを追加しない。

### AssetAPI
---
**役割**

AssetSystem のうち Game に必要な操作だけを公開する Boundary API。

```cpp
namespace Hestia
{
    class ENGINE_API AssetAPI
    {
    public:
        void Initialize(AssetSystem* system);

        // 未確定：Gameへ公開するアセット操作

    private:
        AssetSystem* m_system = nullptr;
    };
}
```

[AssetSystem](#assetsystem)  
[GameEngineAPI](./Engine.md#gameengineapi)

**所有・参照**：Engine が AssetAPI を値として所有し、AssetSystem の利用準備後に Initialize で m_system を設定する。m_system は非所有ポインタ。これは共通の Boundary API 方針として採用し、Asset 固有の操作は未確定とする。

**未確定**：共有するアセット型と公開操作はまだ定めない。ENGINE_API の export／import 定義と共有 Header の配置も未確定。

**提案**：実際の AssetSystem を設計する際、Game が使用する情報と操作を選んで公開する。

### Assets
---
**役割**

Game Script に AssetAPI の操作を static API として提供する Facade。

```cpp
class GameRuntime;

namespace HestiaGame
{
    class Assets
    {
    public:
        // 未確定：Game向けのアセット操作

    private:
        static void Bind(Hestia::AssetAPI* api);
        static void Unbind();

        inline static Hestia::AssetAPI* s_api = nullptr;

        friend class ::GameRuntime;
    };
}
```

[AssetAPI](#assetapi)  
[GameRuntime](./GameRuntime.md#gameruntime)

**所有・参照**：s_api は Engine 所有の AssetAPI を非所有で参照する。GameRuntime が private Bind／Unbind を呼び、Script に API 接続を公開しない（U04）。Facade はアセット実体を持たない。

**未確定**：Asset 固有の static API は未定。

**提案**：AssetAPI の設計と同時に、Script が必要とする操作を決める。
