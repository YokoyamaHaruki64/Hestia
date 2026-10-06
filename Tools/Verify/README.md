# 検証バッチ

## 使い方

リポジトリルートから実行します。対象に応じて3つのバッチを選びます。

```bat
Tools\Verify\VerifyDocs.cmd
Tools\Verify\VerifyProject.cmd -Project App
Tools\Verify\BuildSolution.cmd
```

引数の詳細は各バッチに `-Help` を付けて確認できます。

## 引数一覧

| バッチ                                     | 引数                        | 既定値・説明                                                                          |
| --------------------------------------- | ------------------------- | ------------------------------------------------------------------------------- |
| `VerifyDocs.cmd`                        | `-Paths "Docs;AGENTS.md"` | 確認するリポジトリ内のパス。省略時は `Docs` と `AGENTS.md`。セミコロン区切り。                               |
| `VerifyProject.cmd`                     | `-Project App`            | 必須。`.vcxproj` の名前またはリポジトリ相対パス。                                                  |
| `VerifyProject.cmd`                     | `-Configuration <構成>`     | `Debug_Editor`、`Debug_Game`、`Release_Editor`、`Release_Game`。既定は `Debug_Editor`。 |
| `VerifyProject.cmd`                     | `-Paths "Props;Shared"`   | 対象プロジェクト以外に確認範囲を追加。省略可能。                                                        |
| `BuildSolution.cmd`                     | `-Configuration <構成>`     | 四構成名または `All`。既定は `All`。                                                        |
| `VerifyProject.cmd`／`BuildSolution.cmd` | `-Rebuild`                | 指定時はクリーン後にビルド。省略時は増分ビルド。                                                        |
| すべて                                     | `-Help`                   | 使用方法を表示。                                                                        |

## 仕様

- 任意の作業ディレクトリから実行でき、Windows PowerShell 5.1、Git、ビルド時はVS 2022 C++ビルドツールを使います。プラットフォームはx64です。
- Docs用は対象Markdownのローカルファイルリンクと競合マーカーを確認します。外部URL、見出しアンカー、Wikiリンク、複数行リンクは確認しません。
- プロジェクト用は対象プロジェクト内の競合マーカーを確認してビルドします。依存プロジェクトは `.vcxproj` の `ProjectReference` に従い、MSBuildが必要なものを増分ビルドします。
- sln用は `Hestia.sln` を指定構成でビルドします。`All` ではEditor／Game × Debug／Releaseの四構成を順番に実行し、失敗後も残りを続けます。
- Docs／プロジェクト用はGit状態と対象範囲の変更量を表示します。無関係な変更では失敗にしません。行末空白検査、修正、整形、改行変換、ステージング、コミットは行いません。
- ログはGit除外済みの `Build/Verification/<実行ごとのフォルダー>/` に保存します。ビルドログ、binlog、対象・検出内容、終了コードを記録します。ビルド警告だけでは失敗になりません。
- 終了コードは成功 `0`、確認／ビルド失敗 `1`、引数・ツールなどの実行エラー `2` です。実行時・GUI動作は別途確認してください。


