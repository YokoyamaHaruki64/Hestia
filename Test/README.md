# Hestia の自動テスト

`RunTests.ps1` は `Test|x64` で App と Engine を static library としてビルドし、GoogleTest の実行ファイルを起動します。通常実行は `./Test/RunTests.ps1`、ビルドのみの場合は `./Test/RunTests.ps1 -BuildOnly`、テストを絞る場合は `./Test/RunTests.ps1 -Filter 'SuiteName.TestName'` を使います。

## ディレクトリ

| 場所 | 用途 |
|---|---|
| `Hestia_Tests/` | GoogleTest の実行ファイルを作る Visual Studio プロジェクトとテストコード |
| `ThirdParty/GoogleTest/` | 固定した GoogleTest のソースとライセンス |
| `Support/` | 共通 Fixture、Mock、テスト補助コードの配置先 |
| `Data/` | テスト入力データの配置先 |
| `Build/` | Test 構成のビルド生成物 |
| `Results/` | 実行ごとの結果とログ |
| `Test_Common.props` | Test 構成の出力先などの共通設定 |

## 実行結果

`Results/<実行ID>/Summary.md` に実行全体の結果と、成功を含むテスト名・対象関数・結果の一覧を保存します。`Failures.md` には失敗したテストだけを抽出し、XML の `<failure>` または `<error>` のメッセージ全文を記録します。メッセージにはファイル名・行番号やアサーションの詳細が含まれます。対象関数は GoogleTest の `RecordProperty("target", ...)` から取得します。

| ファイル | 用途 |
|---|---|
| `Summary.md` | 人が読む実行結果とテスト別一覧 |
| `Failures.md` | 失敗テストとエラーメッセージだけの一覧 |
| `run-summary.json` | 実行全体の件数、終了コード、ログパスなどの機械処理用データ |
| `msbuild-console.log` | ビルドの概要 |
| `msbuild.log` | ビルド診断の詳細 |
| `msbuild.binlog` | MSBuild の構造化ログ |
| `gtest-console.log` | テスト結果の表示 |
| `gtest_stdout.log` / `gtest_stderr.log` | テスト実行ファイルの標準出力と標準エラー |
| `gtest-results.xml` | GoogleTest の機械処理用結果。テスト別一覧の元データ |

BuildOnly やビルド失敗などでテストを実行しなかった場合、GoogleTest のログや XML は生成されません。テストが 0 件の場合は失敗として終了します。
