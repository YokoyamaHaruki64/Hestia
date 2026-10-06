# Hestia 作業規約

このファイルはリポジトリ全体に適用する。共通作業方針を重複して記載せず、設計の詳細は `Docs/` に置く。

## 設計の参照

- 全体構造は [Architecture](Docs/Architecture.md)、出典は [DesignSources](Docs/DesignSources.md)、未確定事項は [OpenDecisions](Docs/OpenDecisions.md) を確認する。
- 設計を具体化するときは [DevelopNotes](Reference/HestiaDesign/DevelopNotes/Overview.md) を起点に、対象 System の分野別資料と照合する。
- 比較案・追加草案を決定として扱わない。資料と実装が異なる場合は、変更前に差分と採用理由を整理する。

## 実装規約

- Engine 内部型と DLL 境界の共有型は `Hestia`、Game Script 向け Facade は `HestiaGame` に置く。
- 名前空間・型・列挙型と値・関数・ファイル名は `UpperCamel`、ローカル変数・引数は `lowerCamel`、メンバは `m_lowerCamel`、static メンバは `s_lowerCamel` とする。
- マクロと名前付き定数は `UPPER_SNAKE_CASE`。一時的な `const`／`constexpr` ローカルは `lowerCamel`。API テーブルの公開関数ポインタは関数と同じ `UpperCamel` とする。
- 命名規約は新規コードと変更箇所に適用し、無関係な既存コードは一括改名しない。
- インクルードガードは `#ifndef`／`#define`／`#endif` 形式で統一する。通常の Header はファイルを識別できる一意なガード名、PCH は `プロジェクト名_PCH_` を使う。
- 責務、所有と非所有参照、破棄順序を明確にし、公開 API と DLL 境界には利用側に必要な操作だけを置く。
- 関数間と目的の異なる処理群を空白行で区切る。前提や理由が読み取りにくい箇所だけ簡潔な日本語コメントを付ける。
- 改行・インデント・文字コードはルートの `.editorconfig` を正とする。

## 実装フロー

Public API と期待する振る舞いを先に定義し、テストを実行可能な仕様として扱う。C++ の実装では次のフローを基本とする。

1. Header / Public API を定義する
2. `cpp-automated-testing` skill を用いて期待する振る舞いをテストとして定義する
3. 実装前にテストが失敗することを確認する
4. 実装する
5. テストを実行し、成功を確認する

- 工程別の作業には、同スキルが指定する `cpp-test-*` を使う。必要に応じてリファクタリングし、テストの成功を再確認する。
- Entity 管理、Handle、Math、Asset 管理、Reflection、Serializer、Allocator、状態管理など、ロジックや Public API を独立して検証できるものは、このフローを原則とする。
- すべての機能に厳密な TDD を強制しない。DirectX 12 の実描画、SwapChain、Present、GPU ドライバ依存処理、Window／OS 依存処理など、単体テストが不自然・困難なものは、統合テスト、Validation、デバッグレイヤー、実行時検証など適切な方法を選ぶ。
- テストは可能な限り公開された振る舞い・契約を検証し、内部データ構造の変更だけで不要に壊れるテストを避ける。
- `Test/RunTests.ps1` の結果を確認するときは、まず実行フォルダーの `Summary.md` を読み、失敗件数が 0 件ならそこで確認を終える。失敗がある場合は同じフォルダーの `Failures.md` を読み、記載されたファイルと行番号から原因を調べる。XML と個別ログは、`Failures.md` がない場合や情報が不足する場合に確認する。

## 変更と検証

- 作業前に Git の変更状態を確認し、無関係な変更を保持する。
- 行末空白は検査・修正せず、エディター側の設定に任せる。
- 文書変更は [VerifyDocs](Tools/Verify/VerifyDocs.cmd) で対象のリンクと競合マーカーを確認する。引数と確認範囲は [使い方](Tools/Verify/README.md) を参照する。
- 通常の実装確認は [VerifyProject](Tools/Verify/VerifyProject.cmd) で対象プロジェクトの `Debug_Editor|x64` をビルドする。共有 Header・DLL 境界の変更と機能単位の完了時は [BuildSolution](Tools/Verify/BuildSolution.cmd) で x64 四構成を確認する。
- 実行確認が必要な挙動と未確認の範囲を、完了時に区別して報告する。

## コミットメッセージ

- `[英語概要]日本語コメント` の形式とする。英語概要には `Add`、`Update`、`Refactor` など作業単位を表す語を使い、後半に変更内容を日本語で簡潔に書く（例：`[Update]ビルド構成を追加`）。
