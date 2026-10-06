# Hestia 実装リポジトリの全体構造

## 読み方と適用範囲

本書は実装の基準となる骨格を整理する。**資料に明記された内容**は [DevelopNotes/Overview](../Reference/HestiaDesign/DevelopNotes/Overview.md)、[Class/Engine](../Reference/HestiaDesign/DevelopNotes/Class/Engine.md)、[OpenQuestions](../Reference/HestiaDesign/DevelopNotes/OpenQuestions.md) の確認済みの記述に基づく。個別機能の設計草案は [DesignSources](./DesignSources.md)、採用を要する事項は [OpenDecisions](./OpenDecisions.md) に分ける。以下の構造は現行コードの実装状況を表すものではない。

## 責務と依存方向

| 層 | 資料に明記された内容 | 実装リポジトリ向けの提案・留保 |
| --- | --- | --- |
| Application.exe | `Application` が `Window`、Engine.dll のロード状態、条件付き `Editor`、時間計測とメインループを管理する。Engine は `EngineAPI` を通して操作する。[Application](../Reference/HestiaDesign/DevelopNotes/Class/Application.md) | 既存の `Application/` をこの入口として伸ばす。Editor の配置・接続は未確定。 |
| Engine.dll | `Engine` が各 System、Boundary API、`GameEngineAPI` を値所有し、Game.dll と GameRuntime の寿命を管理する。`ApplicationAPI` を値保持する。[Engine](../Reference/HestiaDesign/DevelopNotes/Class/Engine.md) | 既存の `Engine/` に内部実装と Game 向け公開境界を置く。ファイル／ライブラリ分割は API 確定時に決める。 |
| Game.dll | GameRuntime 実体を内部に隠し、`GameRuntimeAPI` で生成・更新・破棄する。Script は `HestiaGame::Xxx` Facade を使う。[GameRuntime](../Reference/HestiaDesign/DevelopNotes/Class/GameRuntime.md) | Game プロジェクトはまだ存在しない。ECS Plan2 の初期 C++ `.h/.cpp` 案を候補とし、GameRuntime の内部所有型は決めない。[Plan2](../Reference/HestiaDesign/ECS/ECSArchitecture_Plan2.md) |

呼出し方向は **Application → EngineAPI → Engine**、**Engine → ApplicationAPI → Application**、**Engine → GameRuntimeAPI → GameRuntime**、**Game → HestiaGame Facade → Boundary API → System**。Game から Application への別経路は方向だけが示され、型・操作は未確定である。[Relationships](../Reference/HestiaDesign/DevelopNotes/Relationships.md)、[OpenQuestions U03](../Reference/HestiaDesign/DevelopNotes/OpenQuestions.md)

Engine 内部の System を Game.dll に直接公開しない。`XxxAPI` が System の操作から Game に許すものだけを選び、Facade は API へ委譲する。共通の `ISystem`／`ISystemAPI` は設けず、各 System の状態は instance member に置く。private `s_instance` は自己参照用で、System 実体を返す public Getter ではない。[SubsystemApiFacadeDesign](../Reference/HestiaDesign/DevelopNotes/SubsystemApiFacadeDesign.md)

## System と分野別設計の接続

| System | 確認済みの位置と具体化に使う資料 |
| --- | --- |
| `TimeSystem` | Engine が `TimeSystem`／`TimeAPI` を所有。`TimeData` は System が所有し、API は非所有で参照する。通常更新は `FrameExecute` 内、固定時間値は `FixedUpdate` 内で更新する。[Time](../Reference/HestiaDesign/DevelopNotes/Class/Time.md) |
| `AssetSystem` | System／API／Facade の位置のみ確認済み。具体的な Load 等は [AssetManagement](../Reference/HestiaDesign/Asset/AssetManagement.md)、[AssetDB](../Reference/HestiaDesign/Asset/AssetDB.md)、[AssetType](../Reference/HestiaDesign/Asset/AssetType.md)、[Material](../Reference/HestiaDesign/Asset/Material.md) と突き合わせて採用を決める。[Class/AssetSystem](../Reference/HestiaDesign/DevelopNotes/Class/AssetSystem.md) |
| `GraphicsSystem` | 描画状態を担う System／API／Facade の骨格があり、機能入口は追加草案。初期描画経路は Main Thread の通常経路を示す更新を優先して検討する。[Subsystems](../Reference/HestiaDesign/DevelopNotes/Class/Subsystems.md)、[RenderingArchitecture](../Reference/HestiaDesign/Graphics/RenderingArchitecture.md) |
| `InputSystem` | Engine に届く Window Message と Game 向け入力状態を担当する草案。デバイス・状態型・問い合わせ API は未確定。[Subsystems](../Reference/HestiaDesign/DevelopNotes/Class/Subsystems.md) |
| `AudioSystem` | 再生状態を担う草案。ライブラリ、データ型、操作は未確定。[Subsystems](../Reference/HestiaDesign/DevelopNotes/Class/Subsystems.md) |
| `PhysicsSystem` | 固定更新で物理を進める草案。物理実体、World との所有関係、更新順は未確定。[Subsystems](../Reference/HestiaDesign/DevelopNotes/Class/Subsystems.md) |

**未確定事項**：ECS の NativeECS／Sparse Script は [Plan1](../Reference/HestiaDesign/ECS/ECSArchitecture_Plan1.md) と [Plan2](../Reference/HestiaDesign/ECS/ECSArchitecture_Plan2.md) に構想があるが、DevelopNotes の Engine member や GameRuntime の所有型へ自動的に加えない。`WorldSystem`／`LoggerSystem` の具体的な配置もこのコピーからは確定できない。[OpenDecisions](./OpenDecisions.md)

## 所有、非所有参照、DLL 境界

- **資料に明記された内容**：Application が Engine.dll と `EngineHandle = void*` の寿命を管理する。`GetEngineAPI` の関数テーブルは Engine.dll 側が保持し、Application は非所有で参照する。Engine の生成・破棄は DLL 内の `Create`／`Destroy` に閉じる。[EngineAPI](../Reference/HestiaDesign/DevelopNotes/Class/Engine.md)
- **資料に明記された内容**：Engine が System と Boundary API を値所有する。System の利用準備後、各 API の `Initialize(XxxSystem*)` で非所有ポインタを結び、`GameEngineAPI` に API への非所有ポインタを載せる。GameRuntime の実体は Game.dll 側で生成・破棄し、Engine は opaque handle と非所有の関数テーブルを保持する。[Engine](../Reference/HestiaDesign/DevelopNotes/Class/Engine.md)、[SubsystemApiFacadeDesign](../Reference/HestiaDesign/DevelopNotes/SubsystemApiFacadeDesign.md)
- **資料に明記された内容**：Game Facade の API ポインタは非所有。`Bind`／`Unbind` は private で GameRuntime のみが実行する。GameRuntime の終了処理後に Unbind、実体破棄、その後 Game.dll Unload の順とする。[GameRuntime](../Reference/HestiaDesign/DevelopNotes/Class/GameRuntime.md)
- **資料に明記された内容**：Window の `HWND` は Application 側で保持し、Engine は member に持たず `ApplicationAPI::GetWindowHandle()` から必要時に得る。[Application](../Reference/HestiaDesign/DevelopNotes/Class/Application.md)
- **実装リポジトリ向けの提案**：共有 Header には境界で必要な Handle、API テーブル、共有値だけを置き、内部 System／DX12 実体の型定義は分離する。Header の配置、`ENGINE_API`、STL を含む C++ 型の境界契約は未確定。[SubsystemApiFacadeDesign](../Reference/HestiaDesign/DevelopNotes/SubsystemApiFacadeDesign.md)、[Plan2](../Reference/HestiaDesign/ECS/ECSArchitecture_Plan2.md)

Asset の候補設計では `AssetHandle<T>` は非所有の識別子、CPU Asset は Cache が所有し、DX12 Resource／Descriptor は Graphics 側が所有する。Material の正本は型付き値、GPU Layout の正本は Reflection とする。これらは **分野別の設計草案** であり、DevelopNotes の `AssetAPI` 公開操作としては未採用である。[AssetType](../Reference/HestiaDesign/Asset/AssetType.md)、[Material](../Reference/HestiaDesign/Asset/Material.md)、[Class/AssetSystem](../Reference/HestiaDesign/DevelopNotes/Class/AssetSystem.md)

## 初期化、更新、終了

1. **資料に明記された内容**：Application が Window を作り、Engine.dll をロードして `GetEngineAPI` を取得する。`ApplicationAPI` を作成し、`EngineAPI::Create` から初期化済み EngineHandle を受け取る。[Application](../Reference/HestiaDesign/DevelopNotes/Class/Application.md)
2. **資料に明記された内容**：`Create` は Engine を生成し、内部 `Engine::Initialize(applicationAPI)` を呼ぶ。Engine は System の利用準備、Boundary API の接続、GameEngineAPI の構成、Game.dll のロード、GameRuntime の生成・Facade Bind の順に進む。System に一律の `Initialize` は要求しない。[Engine](../Reference/HestiaDesign/DevelopNotes/Class/Engine.md)、[SubsystemApiFacadeDesign](../Reference/HestiaDesign/DevelopNotes/SubsystemApiFacadeDesign.md)
3. **資料に明記された内容**：Application は Window Message を Engine／条件付き Editor に void 通知し、自身の処理後に `DefWindowProc` の値を返す。実経過時間を蓄積して固定刻みごとに `EngineAPI::FixedUpdate` を呼び、その後 `FrameExecute` を呼ぶ。Engine は Time 値を先に更新し、GameRuntime の引数なし `FixedUpdate`／`Update` へ進める。Game は `HestiaGame::Time` から時間を読む。その他の System と Editor の細かな順序は未確定。[Relationships](../Reference/HestiaDesign/DevelopNotes/Relationships.md)、[Time](../Reference/HestiaDesign/DevelopNotes/Class/Time.md)
4. **資料に明記された内容**：Application が `EngineAPI::Destroy` を呼ぶ。Engine は GameRuntime 終了、Facade Unbind、GameRuntime 破棄、Game.dll Unload と内部終了を行い、`Destroy` 内で Engine を delete する。戻った後に Application が Engine.dll を Unload し、Window を終了する。[Relationships](../Reference/HestiaDesign/DevelopNotes/Relationships.md)

**未確定事項**：`Create` 失敗時の返し方と部分初期化の後片付け、各 System の生成・終了順、Editor と GPU 資源の終了順は実装前に決める。[Engine](../Reference/HestiaDesign/DevelopNotes/Class/Engine.md)、[OpenDecisions](./OpenDecisions.md)

## 現行コードとの差分

現行の [Application.h](../Application/Include/Application.h) と [Application.cpp](../Application/Source/Application.cpp) は Window・Waitable Timer・`ApplicationAPI` の一部・固定刻みループを持つが、Engine のロードと更新はコメントまたは未実装で、`ApplicationAPI` はグローバル名前空間にある。`GetWindowHandle` も未実装である。[Engine/dllmain.cpp](../Engine/dllmain.cpp) は DLL エントリのみで、EngineAPI や System はまだない。[Window.cpp](../Application/Source/Window.cpp) と [Application.cpp](../Application/Source/Application.cpp) の Window Message／Window 終了処理も資料の経路とは一致していない。今回はコードを変更しない。
