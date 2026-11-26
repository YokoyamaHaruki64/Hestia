# 実装仕様書

2026-11-26に、Scene の保存・切り替え、Script の GUID 復元・コード生成・保存監視、共通の YAML 保存経路、単独 Material と組み込み Mesh の永続参照を反映した。

## 読み方

各フォルダの `README.md` は責務、所有関係、処理の流れ、制約を説明し、`Types.md` は型と利用上の契約を説明する。ソースへのリンクは仕様の確認先であり、実行確認済みを意味しない。

| システム | 処理・構造 | 型・開発手引き |
| --- | --- | --- |
| Application / Window | [Application](Application/README.md) | [Types](Application/Types.md) |
| Engine / DLL境界 / Scene | [Engine](Engine/README.md)、[Scene の保存と切り替え](Engine/README.md#scene-の保存と切り替え) | [Types](Engine/Types.md) |
| GameRuntime / Game向けAPI | [GameRuntime](GameRuntime/README.md) | [型とAPI](GameRuntime/Types.md)、[Processの登録と公開](ECS/DevelopmentGuide.md#公開processのapiとfacade) |
| Time | [Time](Time/README.md) | [Types](Time/Types.md) |
| Log | [Log](Log/README.md) | [Types](Log/Types.md) |
| Asset | [Asset](Asset/README.md) | [Types](Asset/Types.md)、[Assetの設計](../Design/AssetSystem.md) |
| Graphics / DX12 / Shader | [Graphics](Graphics/README.md)、[MaterialShader と Forward 描画](Graphics/MaterialShader.md) | [Types](Graphics/Types.md)、[MaterialShader の設計](../Design/MaterialShader.md)、[RenderPassの設計](../Design/RenderPass.md) |
| ECS | [ECS](ECS/README.md) | [Types](ECS/Types.md)、[Process](ECS/Process.md)、[開発手引き](ECS/DevelopmentGuide.md) |
| ECSProcess | [Transform](ECSProcess/Transform.md)、[MeshRenderer](ECSProcess/MeshRenderer.md) | [Transformの設計](../Design/Transform.md) |
| Common / 数学・色・乱数 | [Common](Common/README.md) | [Types](Common/Types.md) |

## 全体の接続

ApplicationがWindowとフレーム進行を管理し、Engine.dllのAPIテーブルを取得してEngineを生成する。EngineはLog、Time、Graphics、Asset、ECSを所有し、Game.dllからGameRuntimeを生成する。GameRuntimeはScriptを所有し、Engineから借りたAPIでEntityやProcessを操作する。AssetのCPU実体とGraphicsのGPU実体は型付きHandleで対応付ける。ECSは単一Worldを使い、SceneSystemの静的関数がフレーム末尾に保存と切り替えを処理する。

既存の [Architecture](../Architecture.md)、[DesignSources](../DesignSources.md)、[OpenDecisions](../OpenDecisions.md) は設計資料の整理である。一部に現在より前の実装状態の記述があるため、現在の動作は本書のリンク先コードで確認する。

## 対象の境界

Gameは `World` を通じて `HestiaGame::Transform` と `HestiaGame::MeshRenderer` を取得する。Game向けの公開面にはEntity、Process、Time、Log、Assetが含まれる。Input、Audio、Physics、Editorは未接続。PCH、DLL入口、Windows Header、DLL exportマクロはEngine/Commonの補助部分として扱う。

各文書はソースの静的調査を根拠とする。2026-11-05のEngineビルドとECS関連19件の成功、当時の全体テスト5件の失敗は[ECSの検証記録](ECS/README.md#検証記録2026-11-05)を参照。2026-11-07のAppビルドとForward描画を含む関連テスト7件の成功は[MaterialShaderの検証記録](Graphics/MaterialShader.md#検証記録)を参照。

## Wikiの再生成

正本はこのフォルダのMarkdown。閲覧用の[ImplementationWiki.html](../ImplementationWiki.html)は、Node.jsで[Generate.cmd](../../Tools/SpecWiki/Generate.cmd)を実行して再生成する。npm installは不要で、Markdown変換器とライセンスはTools/SpecWiki/vendorに同梱する。

```bat
Tools\SpecWiki\Generate.cmd
Tools\SpecWiki\Generate.cmd --check
```

本文・分類・図を変更したら再生成し、--checkで分類、見出しリンク、図の根拠ファイル、生成内容の一致を確認する。本文はMarkdown、分類と図の接続はcatalog.json、図はTools/SpecWiki/diagramsで編集する。
