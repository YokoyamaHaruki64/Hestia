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
- 責務、所有と非所有参照、破棄順序を明確にし、公開 API と DLL 境界には利用側に必要な操作だけを置く。
- 関数間と目的の異なる処理群を空白行で区切る。前提や理由が読み取りにくい箇所だけ簡潔な日本語コメントを付ける。

## 変更と検証

- 作業前に Git の変更状態を確認し、無関係な変更を保持する。
- コード変更時は `Debug_Editor|x64` のビルドを基本とする。共有 Header・DLL 境界の変更と機能単位の完了時は、Editor／Game × Debug／Release の x64 四構成を確認する。
- 実行確認が必要な挙動と未確認の範囲を、完了時に区別して報告する。

## コミットメッセージ

- `[英語概要]日本語コメント` の形式とする。英語概要には `Add`、`Update`、`Refactor` など作業単位を表す語を使い、後半に変更内容を日本語で簡潔に書く（例：`[Update]ビルド構成を追加`）。
