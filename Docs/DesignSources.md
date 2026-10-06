# 設計資料の出典と位置づけ

## コピーの範囲と時点

現在の収録先は **`Reference/HestiaDesign/`** であり、以下の相対リンクはこの配置を指す。31 ファイルは本リポジトリのコミット `3d0fb19116ad2cf2efe613b13864e82913d42495`（2026-10-06 16:09:59 JST）で、当時の `Refarence/HestiaDesign/` に追加された。現在の配置への移動は作業ツリー上の変更であり、このコミットに含まれない。各分野の元リポジトリのコミットや個別コピー時刻は収録資料から確認できないため、このコミットを**本リポジトリで再現できる元のコピー時点**とする。コミット ID と時刻は Git 履歴で確認した値である。

DevelopNotes の基になった外部 `chaba0426/Develop_Notes` は [SourceNotes](../Reference/HestiaDesign/DevelopNotes/SourceNotes.md) に `main` の `1e76747792192b2b56abf3da0dbddf2e2d731af5`、確認日 2026-10-06 JST と記録されている。ただし本リポジトリの DevelopNotes は原文だけでなく会話での決定と追加草案を含む。外部リポジトリの**現在**の状態を確認した記録ではない。

## 資料の優先関係

**資料に明記された内容**：全体構造は [DevelopNotes/Overview](../Reference/HestiaDesign/DevelopNotes/Overview.md) と [OpenQuestions](../Reference/HestiaDesign/DevelopNotes/OpenQuestions.md) の回答済み判断を起点とする。クラス資料は [ClassDesign](../Reference/HestiaDesign/DevelopNotes/ClassDesign.md) が示す概念設計で、コンパイル可能な Header ではない。分野別資料は System の用途と候補を具体化するが、未確定の Boundary API を自動で確定しない。

| 分野 | 参照資料 | 位置づけ |
| --- | --- | --- |
| 全体・関係 | [Overview](../Reference/HestiaDesign/DevelopNotes/Overview.md)、[Relationships](../Reference/HestiaDesign/DevelopNotes/Relationships.md)、[SubsystemApiFacadeDesign](../Reference/HestiaDesign/DevelopNotes/SubsystemApiFacadeDesign.md)、[OpenQuestions](../Reference/HestiaDesign/DevelopNotes/OpenQuestions.md) | 所有、依存、API 接続、回答済み／未解決事項の起点。 |
| 出典・記述規則 | [SourceNotes](../Reference/HestiaDesign/DevelopNotes/SourceNotes.md)、[ClassDesign](../Reference/HestiaDesign/DevelopNotes/ClassDesign.md)、[NamingConvention](../Reference/HestiaDesign/DevelopNotes/NamingConvention.md)、[_FormatTemplate](../Reference/HestiaDesign/DevelopNotes/Class/_FormatTemplate.md) | 外部出典の記録と概念宣言の書式。 |
| Application・DLL 境界 | [Application](../Reference/HestiaDesign/DevelopNotes/Class/Application.md)、[Engine](../Reference/HestiaDesign/DevelopNotes/Class/Engine.md)、[GameRuntime](../Reference/HestiaDesign/DevelopNotes/Class/GameRuntime.md) | メインループ、所有、EngineAPI／GameRuntimeAPI、Facade Bind の骨格。GameRuntime の内部型は仮置き。 |
| System 共通・時間・Editor | [AssetSystem](../Reference/HestiaDesign/DevelopNotes/Class/AssetSystem.md)、[Subsystems](../Reference/HestiaDesign/DevelopNotes/Class/Subsystems.md)、[Time](../Reference/HestiaDesign/DevelopNotes/Class/Time.md)、[Editor](../Reference/HestiaDesign/DevelopNotes/Class/Editor.md) | Time は更新経路まで具体化。Asset／Graphics／Input／Audio／Physics の機能入口と Editor lifecycle は未採用の追加草案を含む。 |
| Asset | [AssetDB](../Reference/HestiaDesign/Asset/AssetDB.md)、[AssetManagement](../Reference/HestiaDesign/Asset/AssetManagement.md)、[AssetType](../Reference/HestiaDesign/Asset/AssetType.md)、[Material](../Reference/HestiaDesign/Asset/Material.md) | 名前解決、型付き Handle、CPU／GPU 所有分離、Material／Shader 移行の分野別草案。旧 Facade／GameAPI.lib 記述を DevelopNotes の API と照合する必要がある。 |
| 描画入口・実行 | [RenderingArchitecture](../Reference/HestiaDesign/Graphics/RenderingArchitecture.md)、[RenderingPipeLine](../Reference/HestiaDesign/Graphics/RenderingPipeLine.md)、[RenderGraph](../Reference/HestiaDesign/Graphics/RenderGraph.md)、[MultiThreadRendering](../Reference/HestiaDesign/DirectX/MultiThreadRendering.md) | RenderItem と描画順の候補。RenderingArchitecture 冒頭は初期 Main Thread 経路を優先し、Render Thread／FramePacket と RenderGraph は後段案。MultiThreadRendering はさらに後の Build→Sort→Record 草案。 |
| 描画資源・照明 | [RenderTargetBudget](../Reference/HestiaDesign/Graphics/RenderTargetBudget.md)、[Lighting](../Reference/HestiaDesign/Graphics/Lighting.md) | Format・容量の概算と Light／Shadow／Reflection 方針。計測前の見積りを実測値として使わない。 |
| Shader DSL | [MaterialShaderDSL](../Reference/HestiaDesign/Graphics/MaterialShaderDSL.md)、[LightingShaderDSL](../Reference/HestiaDesign/Graphics/LightingShaderDSL.md)、[ScreenEffectShaderDSL](../Reference/HestiaDesign/Graphics/ScreenEffectShaderDSL.md) | Material、ShadingModel、画面効果の契約案。最初の EngineAPI 実装の前提にはしない。 |
| ECS・Game 制作 | [ECSArchitecture_Plan1](../Reference/HestiaDesign/ECS/ECSArchitecture_Plan1.md)、[ECSArchitecture_Plan2](../Reference/HestiaDesign/ECS/ECSArchitecture_Plan2.md) | Plan1 は NativeECS と C# Script の基本案。Plan2 は C++ Script を採用する旨を明記し、初期は通常の `.h/.cpp` と Game.dll、Generator／`.cscript` は後段とする。 |

空ファイルの [Graphics.md](../Reference/HestiaDesign/Graphics/Graphics.md) には判断根拠がない。独立した `Runtime/` と `Editor/` ディレクトリはこのコピーに存在しない。Runtime は DevelopNotes の Application／Engine／GameRuntime、[ECS Plan2](../Reference/HestiaDesign/ECS/ECSArchitecture_Plan2.md) と描画資料、Editor は [Class/Editor](../Reference/HestiaDesign/DevelopNotes/Class/Editor.md) と Plan2 内の記述を参照する。コピーが参照する `SimpleArchitecture` などの未収録資料は判断根拠にしない。

**実装リポジトリ向けの提案**：判断を確定する際は、該当資料の先頭にある更新注記と OpenQuestions の回答を読み、採用した範囲を [OpenDecisions](./OpenDecisions.md) に記録する。**未確定事項**：分野別資料の元コミットと未収録資料の正本は確認できていない。
