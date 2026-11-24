# 設計資料の出典と位置づけ

## コピーの範囲と時点

現在の収録先は **`Reference/HestiaDesign/`**。最初の31ファイルはコミット `3d0fb19116ad2cf2efe613b13864e82913d42495`（2026-10-06 16:09:59 JST）で、当時の `Refarence/HestiaDesign/` に追加された。各分野の元リポジトリのコミットや個別コピー時刻は収録資料から確認できないため、このコミットを本リポジトリで再現できるコピー時点とする。

DevelopNotes の基になった外部 `chaba0426/Develop_Notes` は [SourceNotes](../Reference/HestiaDesign/DevelopNotes/SourceNotes.md) に `main` の `1e76747792192b2b56abf3da0dbddf2e2d731af5`、確認日 2026-10-06 JST と記録されている。ただし本リポジトリの DevelopNotes は原文だけでなく会話での決定と追加草案を含む。外部リポジトリの**現在**の状態を確認した記録ではない。

## 資料の優先関係

以下は原資料の内容と位置づけを確認するための一覧。現行の採用設計は [全体設計](Architecture.md) からたどれる。クラス資料は [ClassDesign](../Reference/HestiaDesign/DevelopNotes/ClassDesign.md) の書式で概念設計を表し、分野別資料には System の用途や候補が含まれる。

| 分野 | 参照資料 | 位置づけ |
| --- | --- | --- |
| 全体・関係 | [Overview](../Reference/HestiaDesign/DevelopNotes/Overview.md)、[Relationships](../Reference/HestiaDesign/DevelopNotes/Relationships.md)、[SubsystemApiFacadeDesign](../Reference/HestiaDesign/DevelopNotes/SubsystemApiFacadeDesign.md)、[OpenQuestions](../Reference/HestiaDesign/DevelopNotes/OpenQuestions.md) | 所有、依存、API 接続、回答済み／未解決事項の起点。 |
| 出典・記述規則 | [SourceNotes](../Reference/HestiaDesign/DevelopNotes/SourceNotes.md)、[ClassDesign](../Reference/HestiaDesign/DevelopNotes/ClassDesign.md)、[NamingConvention](../Reference/HestiaDesign/DevelopNotes/NamingConvention.md)、[_FormatTemplate](../Reference/HestiaDesign/DevelopNotes/Class/_FormatTemplate.md) | 外部出典の記録と概念宣言の書式。 |
| Application・DLL 境界 | [Application](../Reference/HestiaDesign/DevelopNotes/Class/Application.md)、[Engine](../Reference/HestiaDesign/DevelopNotes/Class/Engine.md)、[GameRuntime](../Reference/HestiaDesign/DevelopNotes/Class/GameRuntime.md) | メインループ、所有、EngineAPI／GameRuntimeAPI、Facade Bind の骨格。GameRuntime の内部型は仮置き。 |
| System 共通・時間・Editor | [AssetSystem](../Reference/HestiaDesign/DevelopNotes/Class/AssetSystem.md)、[Subsystems](../Reference/HestiaDesign/DevelopNotes/Class/Subsystems.md)、[Time](../Reference/HestiaDesign/DevelopNotes/Class/Time.md)、[Editor](../Reference/HestiaDesign/DevelopNotes/Class/Editor.md) | Time は更新経路まで具体化。Asset／Graphics／Input／Audio／Physics の機能入口と Editor lifecycle は未採用の追加草案を含む。 |
| Asset | [AssetDB](../Reference/HestiaDesign/Asset/AssetDB.md)、[AssetManagement](../Reference/HestiaDesign/Asset/AssetManagement.md)、[AssetType](../Reference/HestiaDesign/Asset/AssetType.md)、[Material](../Reference/HestiaDesign/Asset/Material.md)、[DevelopNotes AssetSystem](../Reference/HestiaDesign/DevelopNotes/Class/AssetSystem.md) | 名前解決、型付きHandle、CPU／GPU所有分離の出典。採用設計は[AssetSystem](Design/AssetSystem.md)と[AssetDB](Design/Asset/AssetDB.md)、名前解決方式の経緯は[旧設計](Legacy/AssetSystem.md)、現在の処理は[Asset実装仕様](Implementation/Asset/README.md)を参照。 |
| 描画入口・実行 | [RenderingArchitecture](../Reference/HestiaDesign/Graphics/RenderingArchitecture.md)、[RenderingPipeLine](../Reference/HestiaDesign/Graphics/RenderingPipeLine.md)、[RenderGraph](../Reference/HestiaDesign/Graphics/RenderGraph.md)、[MultiThreadRendering](../Reference/HestiaDesign/DirectX/MultiThreadRendering.md) | RenderItem と描画順の候補。RenderingArchitecture 冒頭は初期 Main Thread 経路を優先し、Render Thread／FramePacket と RenderGraph は後段案。MultiThreadRendering はさらに後の Build→Sort→Record 草案。 |
| 描画資源・照明 | [RenderTargetBudget](../Reference/HestiaDesign/Graphics/RenderTargetBudget.md)、[Lighting](../Reference/HestiaDesign/Graphics/Lighting.md) | Format・容量の概算と Light／Shadow／Reflection 方針。計測前の見積りを実測値として使わない。 |
| Shader DSL | [MaterialShaderDSL](../Reference/HestiaDesign/Graphics/MaterialShaderDSL.md)、[LightingShaderDSL](../Reference/HestiaDesign/Graphics/LightingShaderDSL.md)、[ScreenEffectShaderDSL](../Reference/HestiaDesign/Graphics/ScreenEffectShaderDSL.md) | Material、ShadingModel、画面効果の契約案。最初の EngineAPI 実装の前提にはしない。 |
| ECS・Game 制作 | [ECSArchitecture_Plan1](../Reference/HestiaDesign/ECS/ECSArchitecture_Plan1.md)、[ECSArchitecture_Plan2](../Reference/HestiaDesign/ECS/ECSArchitecture_Plan2.md) | Plan1 は NativeECS と C# Script の基本案。Plan2 は C++ Script を採用する旨を明記し、初期は通常の `.h/.cpp` と Game.dll、Generator／`.cscript` は後段とする。 |

空ファイルの [Graphics.md](../Reference/HestiaDesign/Graphics/Graphics.md) には判断根拠がない。独立した `Runtime/` と `Editor/` ディレクトリはこのコピーに存在しない。Runtime は DevelopNotes の Application／Engine／GameRuntime、[ECS Plan2](../Reference/HestiaDesign/ECS/ECSArchitecture_Plan2.md) と描画資料、Editor は [Class/Editor](../Reference/HestiaDesign/DevelopNotes/Class/Editor.md) と Plan2 内の記述を参照する。コピーが参照する `SimpleArchitecture` などの未収録資料は判断根拠にしない。

判断を確定する際は、該当資料の先頭にある更新注記と OpenQuestions の回答を確認する。未決事項は [OpenDecisions](./OpenDecisions.md) を参照する。分野別資料の元コミットと未収録資料の所在は確認できていない。

## ECS資料の追加と実装との差分

[Class/ECS](../Reference/HestiaDesign/DevelopNotes/Class/ECS/ECSWorld.md)の9ファイルは上記31ファイルとは別の追加資料。2026-11-05に、HestiaDesignの正本 `DevelopNotesDesign/Class/ECS/` とコピーの内容一致を確認した。正本側のコミットIDは未記録であり、この照合を元コミットの特定として扱わない。

以下は ECS を導入した時点でのコピーと実装の差分。採用設計は [ECS の設計](Design/ECS.md)、現在の型・API・失敗時の動作は [ECS の実装仕様](Implementation/ECS/README.md) を参照する。

- ID方式をUTF-8登録名のFNV-1a 64bit、Chunkの既定値を16KiBとして実装した。
- Registryの検索・登録はoptionalやポインタで不在・失敗を表し、草案にあるComponentのCopy構築操作は実装していない。
- 構造変更では不足Componentを先に構築し、成功後に共通Componentをnoexcept Moveする。失敗時の逆Moveは行わない。
- Process関数の例外は受付済み要求の反映後に現在のRunを中断する。Engineのphase接続とEndFrameの位置も実装仕様書に記載する。
- 導入時点の対象は Engine 内部の ECS。Game 向け API と Script の寿命は後続の [World](Design/GameRuntime/World.md) と [Script](Design/GameRuntime/Script.md) に定めた。
