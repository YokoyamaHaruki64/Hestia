# Automation Plan

Write this skill and its references, along with Task/Report files, in English by default. Keep user-facing dialogue and Q&A in Japanese.

リポジトリ相対パスを入出力に使い、長いデータはファイルへ保存して、必要な結果と参照先だけを返す。

## 設定ファイル

Skill 内の [settings.jsonc](../settings.jsonc) を使う。行コメント付き JSON とし、候補と固定値は未設定とする。

```json
{
  "schemaVersion": 1,
  "modelSelection": { "upper": "inherit", "lower": "select" },
  "candidateSequence": [],
  "fixedModels": {
    "upper": { "model": null, "effort": null },
    "lower": { "model": null, "effort": null }
  }
}
```

ユーザーが今回の作業に指定した値を優先し、次に設定ファイルを参照する。不足した値は確認する。
モデルと effort の組み合わせは利用環境で対応するものか確認し、不正な値を別のモデルへ黙って置き換えない。
記入方法・例・利用可能な値は `//` の行コメントで記載する。コメント以外は通常の JSON 構文とし、末尾カンマは使わない。
Windows PowerShell 5.1 では、JSON 文字列とエスケープを識別して文字列外の行コメントを除去した後に `ConvertFrom-Json` へ渡す。単純な行単位の切り捨てで文字列内の `//` を壊さない。
Task／Report の JSON メタデータは引き続きコメントなしの JSON とする。
コメント内のモデルと effort 一覧は作成時点の SubAgent ツール仕様に基づく。実行時の情報を優先し、API やアプリ UI のモデル一覧と同一とは扱わない。

- `modelSelection.upper`: `inherit`、`fixed`、`select`。`inherit` は現在のチャットの設定を継承する。
- `modelSelection.lower`: `fixed`、`select`。
- `fixed`: `fixedModels` の同じ役割の model／effort を使う。
- `select`: 上位モデルが `candidateSequence` から選ぶ。候補は `{ "model": "モデルID", "effort": "推論強度" }` の sequence とし、順序を優先順として扱う。
- `fixedModels`: `upper`／`lower` をキーとする map。各値は model／effort を持つ。使用しない役割の固定値が null でもよい。
- 選択に使う候補が空、または使用する固定値が null の場合は未設定として返す。実際に委任が必要になる前に確認する。
- 設定読み込みは候補と選択方法を返す。作業に合わせた候補選択は上位モデルが行い、選択した値と理由を Task に記録する。
- 上位モデルの固定・選択設定は、現在のモデルと照合するためにも使う。スクリプトが実行中のチャットを切り替える機能は想定しない。

## Task／Report のメタデータ

Markdown の最初のコードブロックを JSON とし、本文には短い指示・回答・報告を書く。
Task と Report は同じ sessionId／runId／taskId を持つ。以下は Task の形式例であり、値を既定設定として扱わない。
JSON は PowerShell 標準の `ConvertFrom-Json`／`ConvertTo-Json` で扱える。YAML に対する処理速度・トークン消費量の優位は未計測であり、保証しない。

```json
{
  "schemaVersion": 1,
  "sessionId": "s01",
  "runId": "r01",
  "taskId": "engine-001",
  "kind": "implementation",
  "phase": "header",
  "status": "ready",
  "assignedModel": { "model": null, "effort": null },
  "editPaths": ["Engine/Include/Engine.h"],
  "verification": { "decision": "pending", "reason": "", "steps": [] }
}
```

- `sessionId`／`runId`／`taskId`: 保存先の `session-.../run-.../Task-.../` と一致させる。再開時は同じ値を引き継ぐ。
- `kind`: `research`、`implementation`。調査のみでは編集を行わず、editPaths は空にする。
- `phase`: `research`、`header`、`test`、`implementation`、`review`、`complete`。
- `status`: `ready`、`in_progress`、`awaiting_review`、`awaiting_upper`、`awaiting_user`、`complete`。工程と状態を重複させない。
- `assignedModel`: 選択結果。未決定の null と、実際に指定した値を区別する。選択理由は本文に記載する。
- `editPaths`: 編集を担当するリポジトリ相対パス。既存変更と他の進行中 Task の範囲を確認する。
- Report のメタデータは schemaVersion、3つの ID、phase、status とし、質問・根拠・変更・検証結果は本文にまとめる。
- `verification.decision`: `pending`、`run`、`skip`。pending または欠落は未決定であり、検証不要とは扱わない。
- `verification.reason`: 上位モデルが選択した理由。skip では理由を記載する。
- `verification.steps`: 実行項目の sequence。run では1件以上、skip では空とする。

steps の検証項目は `kind` と必要な引数を持つ。`kind` は既存の `RunTests.ps1`、`VerifyDocs`、`VerifyProject`、`BuildSolution` に対応させる。
任意のシェル文字列を検証項目として実行せず、既知のコマンドと構造化した引数に変換する。
スクリプトは Task が実行可能な工程に進んでいることと、上位モデルの検証判断を確認して実行する。指示ファイルに項目があるだけで自動開始しない。

## フォルダーと再開

`.codex/working/session-<SessionId>/run-<RunId>/Task-<TaskId>/Task.md` と `Report.md` を使う。
新規作業はテンプレートからローカルコピーする。再開は既存ファイルを読み、新しい ID の発行やテンプレートでの上書きを行わない。
競合検出は全 session／run の進行中 Task のメタデータを対象とし、モデルへ渡す報告本文は担当 Task だけにする。

## ID の発行と再利用

Session／Run の ID 解決は `ResolveWorkContext.ps1` にまとめ、TaskId の発行は Task を作る `NewWorkTask.ps1` に任せる。
Session／Run／Task の操作を区別し、Task 作成のたびに親の ID を新規発行しない。

| 操作 | 発行する ID | 再利用する ID |
| --- | --- | --- |
| 新しい Session を開始 | sessionId | なし |
| Session 内で新しい Run を開始 | runId | sessionId |
| Run 内で Task を追加 | taskId | sessionId、runId |
| 途中再開・工程変更・質問への回答 | なし | sessionId、runId、taskId |

`ResolveWorkContext.ps1` は `NewSession`、`NewRun`、`Resume` を明示的な mode として持つ。
- `NewSession`: sessionId と Session フォルダーを新規作成する。
- `NewRun`: 既存 sessionId を受け取り、runId と Run フォルダーを新規作成する。
- `Resume`: 既存 sessionId／runId の保存先を確認して返す。指定が不明・存在しない場合は、その旨を返し、新規作成へ自動で切り替えない。

`NewWorkTask.ps1` は解決済みの sessionId／runId を受け取り、既存の親フォルダーを確認して Task だけを追加する。
同じ Run に追加した作業や、依頼を継続する追加回答・調整では既存 runId を使う。独立した新しい依頼を始めるときだけ新しい Run にする。
ID の返却はまとめてよいが、返却することと新規発行することを区別する。Task の識別には3つの ID または Task フォルダーのパスを使う。

## スクリプト

| スクリプト | 入力 | 機械処理 | 短い出力 |
| --- | --- | --- | --- |
| `ResolveSettings.ps1` | 設定ファイル、今回の明示指定 | JSONC の行コメントを除去し、選択方法、候補、固定値を検査する | 選択方法・有効な候補・固定値、未設定・不正・確認不能の項目 |
| `ResolveWorkContext.ps1` | mode、既存 sessionId／runId | Session／Run の新規発行または既存の保存先を確認する | sessionId／runId とパス、今回発行した ID |
| `NewWorkTask.ps1` | sessionId、runId、作業名、必要なら taskId | 一意な taskId のフォルダーを作り、テンプレートをローカルコピーし、JSON メタデータを設定する | 3つの ID、作成した2ファイルのパス |
| `InspectWorkScope.ps1` | Task、進行中の Task | パス、Git の変更状態、編集対象の重複を確認する | 既存変更、重複する TaskId と対象 |
| `ReadHandoff.ps1` | Task フォルダーのパス、必要な項目 | 対象の Task／Report から指示・質問・結果・残作業を抽出する | 指定された項目のみ |
| `InvokeVerification.ps1` | Task の verification、現在の工程 | run／skip／pending を区別し、実行可能な場合だけ指定された既存コマンドを順番に実行する | 実行・省略・未決定、終了コード、実行フォルダー |
| `SummarizeVerification.ps1` | 実行フォルダー | Summary を読み、失敗時だけ Failures と不足するログを解析する | 件数、終了コード、失敗箇所、ログパス |
| `SummarizeChanges.ps1` | Task、作業開始時の対象差分記録 | 変更ファイルと変更量を集約し、担当範囲外の変化を検出する | 変更一覧と範囲外の候補 |

テンプレートの正本は [Task.md](../assets/Task.md) と [Report.md](../assets/Report.md)。作業ファイルはこの内容をローカルコピーして作る。

## 呼び出し

リポジトリルートで実行する例。`<sessionId>`、`<runId>`、`<Taskのパス>` は前の呼び出しが返した値に置き換える。
共通の `-RepositoryRoot` は任意で、省略時はスクリプト配置から Hestia のルートを求める。

```powershell
$scripts = '.agents/skills/hestia-implementation-flow/scripts'

# 設定の読み込み。今回の明示指定がある場合はオプションで渡す。
& "$scripts/ResolveSettings.ps1" -LowerModel gpt-6-luna -LowerEffort xhigh

# 初回のみ Session を作り、新しい依頼の開始時に Run を作る。
& "$scripts/ResolveWorkContext.ps1" -Mode NewSession
& "$scripts/ResolveWorkContext.ps1" -Mode NewRun -SessionId '<sessionId>'

# 同じ Run の Task 追加では SessionId / RunId を再利用する。
& "$scripts/NewWorkTask.ps1" -SessionId '<sessionId>' -RunId '<runId>' -TaskName 'Engine ownership research' -Purpose 'Research ownership, lifetime, and destruction order; report evidence and unverified scope.' -Kind research -Phase research

# 再開は既存の ID を返し、新しい ID を発行しない。
& "$scripts/ResolveWorkContext.ps1" -Mode Resume -SessionId '<sessionId>' -RunId '<runId>'

& "$scripts/InspectWorkScope.ps1" -TaskPath '<Taskのパス>'
& "$scripts/ReadHandoff.ps1" -TaskPath '<Taskのパス>' -Source Report
& "$scripts/SummarizeChanges.ps1" -TaskPath '<Taskのパス>'
& "$scripts/InvokeVerification.ps1" -TaskPath '<Taskのパス>'
& "$scripts/SummarizeVerification.ps1" -RunDirectory '<検証結果フォルダー>'
```

NewWorkTask は Task 開始時の既存差分を `Baseline.json` に保存する。範囲と差分の確認はこの記録を再利用し、元からある変更と作業中の変化を区別する。
Task／Report 以外のファイルは、Baseline や長い検証ログなど機械処理に必要なものだけを保存する。

## 共通結果

各スクリプトは `schemaVersion`、`command`、`status`、`exitCode`、`data`、`warnings`、`errors`、`artifacts` を持つ短い JSON を返す。
詳細ファイルがある場合は artifacts にパスを返し、全文を出力しない。
終了コードは正常処理が0、実行した検証の失敗が1、引数・環境・入力形式のエラーが2。
pending／skip の返却も正常処理だが、終了コード0だけを根拠に「検証成功」と扱わない。status と検証結果を確認する。

## 機械処理の前提

- sessionId／runId／taskId はフォルダー名に安全に使える形式で生成し、既存の Task／Report を上書きしない。
- コピー時はテンプレート全文をモデルに出力させず、ローカルファイル操作を使う。
- 機械処理は確認・作成・集約を担当し、下位モデルの起動と完了通知は専用ツールで行う。
- Git の既存変更は開始時に記録する。作業後に見えた差分のすべてを今回の担当による変更と断定しない。
- 重複や範囲外の変化は上位モデルへ報告し、既存変更を自動修復・破棄しない。
- `Summary.md` の失敗件数が0ならそこで確認を終える。失敗時は `Failures.md` を読み、情報不足の場合だけ XML・個別ログへ進む。
- パス・終了コード・件数などを短い構造化データで返す。長いログを受け渡しファイルへ貼り付けない。
- 保存済みの使用量や時間が取得できる場合だけ集計し、モデル指定から節約効果を推定値として断定しない。
