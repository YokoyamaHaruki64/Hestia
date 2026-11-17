# HestiaEngine API構造再設計案

> 作成: 2026-11-15  
> 状態: 設計案（未実装）  
> 対象: [Hestia](https://github.com/YokoyamaHaruki64/Hestia) の `main` ブランチ  
> 関連: [SubsystemApiFacadeDesign.md](./SubsystemApiFacadeDesign.md)

## 1. 目的と方針

EngineのDLL公開APIを整理し、内部サブシステムと公開境界の違いをファイル構造でも明確にする。
また、多数追加されるECSProcess向けAPIに限定して、集約構造体と関数テーブル登録コードを自動生成する。

- API宣言を `Engine/Include/API/` に、API実装を `Engine/Source/API/` に集約する
- 各Subsystemの内部実装（ECS、Transform、Asset、Timeなど）は従来のSubsystemフォルダに残す
- APIの実装形態はSubsystemごとに自由とし、全体を画一的に自動生成しない
- ECSProcess APIだけ、ファイル名・配置規則から集約コードを生成する
- 個別のECSProcess APIは原則 `.h` と `.cpp` の2ファイル。`Impl` 専用ファイルは作らない
- APIImplは単なる委譲ラッパーに限定せず、個別Entity操作の実装本体を担当できる
- Engineを更新したらGameもソースからリビルドする。旧Game DLLとのバイナリ互換性は要求しない
- 既存の公開API名・呼び出し方は、可能な限り維持してソース互換性を重視する

本資料は移行計画であり、現行実装がすでにこの構成になっていることを意味しない。

## 2. 現状のファイル構造

2026-11-15時点のHestiaリポジトリの関連ファイル抜粋。

```text
Hestia/
├─ Application/
│  └─ Include/
│     └─ ApplicationAPI.h
├─ Common/
├─ Engine/
│  ├─ Include/
│  │  ├─ Engine.h
│  │  ├─ EngineAPI.h
│  │  ├─ ECS/
│  │  │  ├─ ECSSystem.h
│  │  │  ├─ ECSWorld.h
│  │  │  ├─ ECSProcess.h
│  │  │  └─ ...
│  │  ├─ Transform/
│  │  │  ├─ TransformData.h
│  │  │  └─ TransformProcess.h
│  │  ├─ Game/
│  │  │  └─ Transform.h
│  │  ├─ Asset/
│  │  │  ├─ AssetSystem.h
│  │  │  ├─ AssetAPI.h
│  │  │  └─ ...
│  │  ├─ Time/
│  │  │  ├─ TimeSystem.h
│  │  │  ├─ TimeAPI.h
│  │  │  └─ TimeData.h
│  │  ├─ Log/
│  │  │  ├─ LogSystem.h
│  │  │  ├─ LogAPI.h
│  │  │  └─ ...
│  │  └─ Graphics/
│  │     ├─ GraphicsSystem.h
│  │     └─ ...
│  └─ Source/
│     ├─ Engine.cpp
│     ├─ EngineAPI.cpp
│     ├─ ECS/
│     ├─ Transform/
│     │  ├─ TransformProcess.cpp
│     │  └─ TransformFacade.cpp
│     ├─ Asset/
│     │  └─ AssetAPI.cpp
│     ├─ Time/
│     │  └─ TimeAPI.cpp
│     ├─ Log/
│     │  └─ LogAPI.cpp
│     └─ Graphics/
└─ Tools/
```

### 現状の特徴

- `AssetAPI`、`TimeAPI`、`LogAPI` はそれぞれのSubsystemフォルダに存在する
- `HestiaGame::Transform` の実装は `TransformFacade.cpp` にあり、直接ECSへアクセスしている
- `TransformProcess` はECSの一括更新とTransform固有の共通操作を持つ
- `Engine.h` 内には将来の `GameEngineAPI` の保持・生成に関するコメントアウト済みのコードがある
- `EngineAPI` はApplicationからEngine.dllを操作する既存の入口

参照: [Engine.h](https://github.com/YokoyamaHaruki64/Hestia/blob/main/Engine/Include/Engine.h) / [TransformFacade.cpp](https://github.com/YokoyamaHaruki64/Hestia/blob/main/Engine/Source/Transform/TransformFacade.cpp) / [EngineAPI.h](https://github.com/YokoyamaHaruki64/Hestia/blob/main/Engine/Include/EngineAPI.h)

## 3. 変更後のフォルダ構造案

既存APIを `API/` 以下に集約する。サブシステム内部処理の配置はそのまま維持する。

```text
Engine/
├─ Include/
│  ├─ Engine.h
│  ├─ API/
│  │  ├─ EngineAPI.h
│  │  ├─ GameEngineAPI.h                 // 新規
│  │  ├─ ECSProcessAPI.h                // 新規・手書き
│  │  ├─ AssetAPI.h
│  │  ├─ TimeAPI.h
│  │  ├─ LogAPI.h
│  │  ├─ GraphicsAPI.h                  // 将来の例
│  │  ├─ ECSProcess/
│  │  │  ├─ TransformAPI.h
│  │  │  ├─ CameraAPI.h                 // 将来の例
│  │  │  └─ MeshRendererAPI.h           // 将来の例
│  │  └─ Generated/
│  │     └─ ECSProcessAPI.generated.h
│  ├─ ECS/
│  ├─ Transform/
│  ├─ Asset/
│  ├─ Time/
│  ├─ Log/
│  ├─ Graphics/
│  └─ Game/
└─ Source/
   ├─ Engine.cpp
   ├─ API/
   │  ├─ EngineAPI.cpp
   │  ├─ GameEngineAPI.cpp              // 新規
   │  ├─ AssetAPI.cpp
   │  ├─ TimeAPI.cpp
   │  ├─ LogAPI.cpp
   │  ├─ GraphicsAPI.cpp                // 将来の例
   │  ├─ ECSProcess/
   │  │  ├─ TransformAPI.cpp
   │  │  ├─ CameraAPI.cpp               // 将来の例
   │  │  └─ MeshRendererAPI.cpp         // 将来の例
   │  └─ Generated/
   │     └─ ECSProcessAPI.generated.cpp
   ├─ ECS/
   ├─ Transform/
   ├─ Asset/
   ├─ Time/
   ├─ Log/
   └─ Graphics/

Tools/
└─ APIGenerator/
   ├─ GenerateECSProcessAPI.py
   └─ Templates/
      ├─ ECSProcessAPI.h.tpl
      └─ ECSProcessAPI.cpp.tpl
```

`GraphicsAPI`、`CameraAPI`、`MeshRendererAPI` は配置を説明するための例であり、現行実装の存在を示さない。

### 境界

| フォルダ | 責務 |
| --- | --- |
| `Engine/Include/API/` | Engineから公開するAPI宣言 |
| `Engine/Source/API/` | 公開APIの実装 |
| `API/ECSProcess/` | 増加する個別ECSProcess API |
| `API/Generated/` | ECSProcess APIの集約定義・初期化コード |
| `ECS/` | ECSWorld、Entity、Archetype、Queryなど |
| `Transform/` | Transformデータ、一括更新、共通処理 |
| その他Subsystem | 対応するSystemの内部実装 |
| `Game/` | Game側OOP Facade（今後の配置は別途検討） |

APIはEngine.dllの公開境界なので、リポジトリ直下の別プロジェクトではなくEngine内に配置する。
配置を統一しても、`TimeAPI` や `AssetAPI` の設計・公開方法まで強制的に統一しない。

## 4. ECSProcess APIと既存Processの責務

```text
Gameコード
  └─ HestiaGame::Transform              // OOP Facade
       ↓
GameEngineAPI
  └─ ecsProc.Transform.SetLocalPosition(...)
       ↓
TransformAPI（関数テーブル）
       ↓
TransformAPIImpl::SetLocalPosition(...) // 個別操作の実装本体
       ├─ ECSSystem / ECSWorld          // EntityとComponentの取得
       └─ TransformProcess             // 共通変換・Dirty処理

TransformProcess::Run(...)
  └─ ECS Queryによるチャンク単位の一括更新
```

- **Game向けFacade**: EntityHandleとAPIテーブルを隠し、ゲームコードの操作感を提供
- **公開API**: Game/Engine間で共有する関数ポインタの型
- **APIImpl**: 個別Entityへの操作の実装本体。エンジン内部から直接呼んでもよい
- **Process**: ECS Queryによる一括更新。共通計算処理はAPIImplからも利用できる

APIImplを機械的なラッパーにする必要はない。逆に全てをProcessへ移して二重定義を増やす必要もない。

現在の `TransformFacade.cpp` には個別操作の本体があり、`TransformProcess` のprivateメソッドを `HestiaGame::Transform` がfriendとして利用している。移行時は処理を重複させず、必要なアクセス権や共通関数の担当を調整する。

## 5. 個別ECSProcess APIの実装例

原則 `TransformAPI.h` と `TransformAPI.cpp` の2ファイルで完結させる。

### TransformAPI.h

```cpp
#ifndef _TRANSFORM_API_H_
#define _TRANSFORM_API_H_

#include "ECS/Entity.h"
#include "Common/Include/HMath.h"

namespace Hestia
{
    struct TransformAPI
    {
        bool (*SetLocalPosition)(
            EntityHandle entity,
            const Vector3& position);
    };

#ifdef HESTIA_ENGINE_INTERNAL

    class TransformAPIImpl
    {
    public:
        static TransformAPI CreateAPI();

        static bool SetLocalPosition(
            EntityHandle entity,
            const Vector3& position);
    };

#endif
}

#endif // _TRANSFORM_API_H_
```

`HESTIA_ENGINE_INTERNAL` はEngineプロジェクトのプリプロセッサ定義。Game側では定義しない。公開テーブル自体の構造は両側で同じに保つ。

### TransformAPI.cpp

```cpp
#include "API/ECSProcess/TransformAPI.h"

#include "ECS/ECSSystem.h"
#include "Transform/TransformProcess.h"

namespace Hestia
{
    TransformAPI TransformAPIImpl::CreateAPI()
    {
        return {
            .SetLocalPosition = &TransformAPIImpl::SetLocalPosition
        };
    }

    bool TransformAPIImpl::SetLocalPosition(
        EntityHandle entity,
        const Vector3& position)
    {
        // Entity / Componentを取得
        // LocalPositionを変更
        // 必要なDirty処理を適用
        // 既存のTransform操作処理を移行
        return true;
    }
}
```

これは構造を示す例であり、実際のECS操作と入力検証の実装は省略している。

## 6. ECSProcess APIの自動集約

自動生成の対象は `Engine/Include/API/ECSProcess/` 直下の `*API.h` のみ。
マクロによる登録宣言やC++ソース解析は行わず、配置・ファイル名の規則だけで型名を決定する。

| 入力ファイル名 | 型名 | メンバ名 | 実装クラス |
| --- | --- | --- | --- |
| `TransformAPI.h` | `TransformAPI` | `Transform` | `TransformAPIImpl` |
| `CameraAPI.h` | `CameraAPI` | `Camera` | `CameraAPIImpl` |
| `MeshRendererAPI.h` | `MeshRendererAPI` | `MeshRenderer` | `MeshRendererAPIImpl` |

各Implは `static XxxAPI CreateAPI()` を持つ。

### ECSProcessAPI.h（手書き）

```cpp
#ifndef _ECS_PROCESS_API_H_
#define _ECS_PROCESS_API_H_

#include "API/Generated/ECSProcessAPI.generated.h"

#endif // _ECS_PROCESS_API_H_
```

### ECSProcessAPI.generated.h（自動生成）

```cpp
#ifndef _ECS_PROCESS_API_GENERATED_H_
#define _ECS_PROCESS_API_GENERATED_H_

#include "API/ECSProcess/CameraAPI.h"
#include "API/ECSProcess/MeshRendererAPI.h"
#include "API/ECSProcess/TransformAPI.h"

namespace Hestia
{
    struct ECSProcessAPI
    {
        CameraAPI Camera;
        MeshRendererAPI MeshRenderer;
        TransformAPI Transform;
    };

#ifdef HESTIA_ENGINE_INTERNAL
    ECSProcessAPI CreateECSProcessAPI();
#endif
}

#endif // _ECS_PROCESS_API_GENERATED_H_
```

### ECSProcessAPI.generated.cpp（自動生成）

```cpp
#include "API/ECSProcessAPI.h"

namespace Hestia
{
    ECSProcessAPI CreateECSProcessAPI()
    {
        return {
            .Camera = CameraAPIImpl::CreateAPI(),
            .MeshRenderer = MeshRendererAPIImpl::CreateAPI(),
            .Transform = TransformAPIImpl::CreateAPI()
        };
    }
}
```

`GameEngineAPI.h` や `ECSProcessAPI.h` は手書きで維持し、Gameコードはgeneratedファイルを直接includeしない。

## 7. GameEngineAPIへの統合

他Subsystemの集約は従来どおり手動で管理する。具体的なメンバ形態はSubsystemに応じて決める。

```cpp
struct GameEngineAPI
{
    TimeAPI* time;
    AssetAPI* asset;

    ECSProcessAPI ecsProc;
};
```

```cpp
GameEngineAPI Engine::CreateGameEngineAPI()
{
    return {
        .time = &m_timeAPI,
        .asset = &m_assetAPI,
        .ecsProc = CreateECSProcessAPI()
    };
}
```

これらは新しいECSProcess APIを組み込むための構造例であり、実際のライフサイクルやメンバ形態はGame.dll接続時に決める。

命名は現行を基本とする。

| 名称 | 提供 → 利用 |
| --- | --- |
| `EngineAPI` | Engine → Application |
| `ApplicationAPI` | Application → Engine |
| `GameEngineAPI` | Engine → Game |
| `GameRuntimeAPI` | Game → Engine |

`EngineAPI` と `ApplicationAPI` はすでに異なる境界を指しているので、名前を衝突させるような一括改名は行わない。

## 8. 生成スクリプトの仕様

`Tools/APIGenerator/GenerateECSProcessAPI.py` がテンプレートを展開する。

1. `Engine/Include/API/ECSProcess/*API.h` を収集
2. ファイル名から型名・メンバ名・Impl名を導出
3. ファイル名順にソートして出力順を固定
4. テンプレートへinclude・メンバ・`CreateAPI()` 登録を挿入
5. 既存のgeneratedファイルと生成結果を内容比較
6. 内容が変わったファイルだけ書き込む

入力ヘッダーの内部を解析する必要はない。`.cpp` も生成器の解析対象としない。
関数宣言と `CreateAPI()` の一致はC++コンパイラで検査する。

通常の規模では毎ビルド走査しても低コストであり、内容が同じ場合は書き込まないことで無駄な再コンパイルを防げる。タイムスタンプキャッシュは必要になるまで導入しない。

生成処理はEngine・Game側のコンパイルより前に完了させる。生成ファイルの出力先は固定とする。

## 9. ビルドと互換性

- Engineの更新時にはGameもソースからリビルドする
- Gameのソースコードはメンバ名（例: `ecsProc.Transform.SetLocalPosition`）でAPIを参照する
- コンパイル時には新しいAPI定義からメンバの配置が解決される
- そのため、EngineとGameが同一の更新済み定義でビルドされる限り、APIメンバの追加・並び替え自体は許容する
- APIの削除・改名・シグネチャ変更はゲームコードのソース互換性を壊し得るため極力避ける
- 旧Gameバイナリを新Engine DLLへそのまま接続する互換性は今回の要求に含めない

## 10. 移行順序

### Phase 1: API配置の整理

既存 `EngineAPI`、`AssetAPI`、`TimeAPI`、`LogAPI` などを `Include/API/` と `Source/API/` へ移動。
includeパスを更新する。既存APIの振る舞いは変えない。

### Phase 2: TransformAPIの導入

`TransformAPI.h/.cpp` と `ECSProcessAPI` を追加。
`TransformAPIImpl` を個別Entity操作の実装本体とし、`TransformProcess::Run` は一括更新を担当する。

### Phase 3: ECSProcess集約のコード生成

名前規則に基づいて `ECSProcessAPI.generated.h/.cpp` を生成し、集約メンバと登録処理を自動化する。

### Phase 4: Game側Facadeの移行

既存 `TransformFacade.cpp` の操作を `TransformAPIImpl` へ移す。
`HestiaGame::Transform` はAPIテーブルを利用するOOP Facadeに変更し、実装の二重化を避ける。

### Phase 5: GameEngineAPIの接続

`GameEngineAPI` に `ECSProcessAPI` を追加し、Game.dllから利用可能にする。

## 11. 実装時に判断する事項

- `GameEngineAPI` の実際の所有・参照形態と初期化タイミング
- Game側OOP Facadeの最終的な配置
- `TransformProcess` と `TransformAPIImpl` 間の共通処理・アクセス権の整理
- generatedファイルをGit管理するかどうか

## まとめ

APIの配置はEngine内の `API/` に集約するが、API設計そのものはSubsystemごとに自由度を残す。
ECSProcessに限り、`*API.h` の命名・配置規則から集約と登録を自動生成する。
個別APIは `.h/.cpp` の2ファイル構成とし、`Impl` の宣言をEngine内部だけに公開する。

既存のSubsystem構成を壊さず、公開APIの境界を整理すると同時に、ECSProcessの追加コストを下げる。
