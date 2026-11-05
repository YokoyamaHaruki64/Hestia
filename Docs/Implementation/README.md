# 実装仕様書

2026-11-02の実装調査を基に、2026-11-05にECSとEngine接続の記述を更新した。設計案の採否や将来の予定はこの文書から決定しない。

## 読み方

各フォルダの `README.md` は責務、所有関係、処理の流れ、制約を説明し、`Types.md` は型と利用上の契約を説明する。ソースへのリンクは仕様の確認先であり、実行確認済みを意味しない。

| システム | 処理・構造 | 型 |
| --- | --- | --- |
| Application / Window | [Application](Application/README.md) | [Types](Application/Types.md) |
| Engine / DLL境界 | [Engine](Engine/README.md) | [Types](Engine/Types.md) |
| Time | [Time](Time/README.md) | [Types](Time/Types.md) |
| Log | [Log](Log/README.md) | [Types](Log/Types.md) |
| Asset | [Asset](Asset/README.md) | [Types](Asset/Types.md) |
| Graphics / DX12 / Shader | [Graphics](Graphics/README.md) | [Types](Graphics/Types.md) |
| ECS | [ECS](ECS/README.md) | [Types](ECS/Types.md)、[Process](ECS/Process.md) |
| Common / 数学・色・乱数 | [Common](Common/README.md) | [Types](Common/Types.md) |

## 全体の接続

ApplicationがWindowとフレーム進行を管理し、Engine.dllのAPIテーブルを取得してEngineを生成する。EngineはLog、Time、Graphics、Asset、ECSを所有する。AssetのCPU実体とGraphicsのGPU実体は型付きHandleで対応付ける。ECSは現在単一Worldで、Engineがphaseを進める。

既存の [Architecture](../Architecture.md)、[DesignSources](../DesignSources.md)、[OpenDecisions](../OpenDecisions.md) は設計資料の整理である。一部に現在より前の実装状態の記述があるため、現在の動作は本書のリンク先コードで確認する。

## 対象の境界

Input、Audio、Physics、Editor、GameRuntime、HestiaGameのFacadeは、現在の稼働コードとして実装されたシステムに数えない。Engine.h内のコメント化された宣言を実装済みとは扱わない。PCH、DLL入口、Windows Header、DLL exportマクロはEngine/Commonの補助部分として扱う。外部ライブラリ本体、テスト基盤、ビルド・検証ツールはエンジンのランタイムシステムから除外する。

各文書はソースの静的調査を根拠とする。2026-11-05にはEngineのDebug_EditorビルドとECS関連19件のテストが成功した。全体テストには5件の失敗が残り、GPU描画・Window操作の実機確認は行っていない。条件と未解消事項は[ECSの検証記録](ECS/README.md#検証記録2026-11-05)を参照する。

## Wikiの再生成

正本はこのフォルダのMarkdown。閲覧用の[ImplementationWiki.html](../ImplementationWiki.html)は、Node.jsで[Generate.cmd](../../Tools/SpecWiki/Generate.cmd)を実行して再生成する。npm installは不要で、Markdown変換器とライセンスはTools/SpecWiki/vendorに同梱する。

```bat
Tools\SpecWiki\Generate.cmd
Tools\SpecWiki\Generate.cmd --check
```

本文・分類・図を変更したら再生成し、--checkで分類、見出しリンク、図の根拠ファイル、生成内容の一致を確認する。本文はMarkdown、分類と図の接続はcatalog.json、図はTools/SpecWiki/diagramsで編集する。
