# 出典と転記方針
---
2026-10-06（日本時間）に接続済み GitHub の読み取り機能で更新ノートを確認した。
今回は Develop_Notes の更新と OpenQuestions の回答・この会話での決定を反映する。未記載 System の詳細は未確定のままにする。

## 参照したコミット
---
- Repository: [chaba0426/Develop_Notes](https://github.com/chaba0426/Develop_Notes)
- Branch: `main`
- Commit: [`1e76747792192b2b56abf3da0dbddf2e2d731af5`](https://github.com/chaba0426/Develop_Notes/tree/1e76747792192b2b56abf3da0dbddf2e2d731af5)
- Git tree の取得は `truncated: false`。設計に関係する Markdown は下記 8 ファイル。初回に参照したコミットは 7ab8eb158e74785a04472d3b23f0f9c77c2a129f。

## 出典一覧
---
| 出典 | 反映先・使い方 |
| --- | --- |
| [ClassDesign](https://github.com/chaba0426/Develop_Notes/blob/1e76747792192b2b56abf3da0dbddf2e2d731af5/HestiaDesign/ClassDesign.md) | ClassDesign.md に転記。概念設計の範囲を定義 |
| [NamingConvention](https://github.com/chaba0426/Develop_Notes/blob/1e76747792192b2b56abf3da0dbddf2e2d731af5/HestiaDesign/NamingConvention.md) | NamingConvention.md に転記 |
| [Class/_FormatTemplate](https://github.com/chaba0426/Develop_Notes/blob/1e76747792192b2b56abf3da0dbddf2e2d731af5/HestiaDesign/Class/_FormatTemplate.md) | Class/_FormatTemplate.md に転記 |
| [Class/Application](https://github.com/chaba0426/Develop_Notes/blob/1e76747792192b2b56abf3da0dbddf2e2d731af5/HestiaDesign/Class/Application.md) | Application の宣言・時間制御・Window Message を転記 |
| [Class/Engine](https://github.com/chaba0426/Develop_Notes/blob/1e76747792192b2b56abf3da0dbddf2e2d731af5/HestiaDesign/Class/Engine.md) | Engine と DLL API の宣言・通常の寿命関係を転記 |
| [Class/Time](https://github.com/chaba0426/Develop_Notes/blob/1e76747792192b2b56abf3da0dbddf2e2d731af5/HestiaDesign/Class/Time.md) | TimeSystem／TimeData／TimeAPI／Facade を反映 |
| [SubsystemApiFacadeDesign](https://github.com/chaba0426/Develop_Notes/blob/1e76747792192b2b56abf3da0dbddf2e2d731af5/HestiaDesign/SubsystemApiFacadeDesign.md) | System／Boundary API／Facade のポインタ接続を反映。Asset 固有の例は実設計として採用しない |
| [README](https://github.com/chaba0426/Develop_Notes/blob/1e76747792192b2b56abf3da0dbddf2e2d731af5/README.md) | 資料の位置づけを確認 |

## 転記と追加の区別
---
Application・Engine はノートを起点に、回答で決まった Hestia::ApplicationAPI、GetWindowHandle、EngineHandle、Create／Destroy の役割と更新順を反映する。Engine の内部 Initialize／Finalize は残す。最新ノートの EngineAPI に残る独立した Initialize／Finalize は今回の回答を優先して公開しない。

Asset の例は実際の AssetSystem に準じた設計ではないという回答を採用し、機能 API と共有型の宣言を取り下げる。ファイルは残し、所有・接続の位置と未確定事項を記録する。

Time は新しい Class/Time.md を反映する。Getter の本体など単純な例は概念宣言と文章へ整理し、System・共有データ・API・Facade の形を維持する。private Bind／Unbind に GameRuntime の friend を追加する。

Graphics・Input・Audio・Physics の詳細は未確定。private s_instance、API の Initialize(XxxSystem*)、Facade の friend だけを共通方針として更新する。GameRuntime の内部構造は仮置きを維持し、Editor の詳細は Runtime と Game が形になってから検討する。

未記載 System は後続でこのリポジトリの情報を繋げて構築する方向。今回は関連する既存設計を読み込んで確定する作業は行わない。SimpleArchitecture／ImplementationSketches と過去の設計判断を自動的に持ち込まない。

## 作業規則
---
HestiaDesign の `AGENTS.md` と `_Templates/DesignDraft.md` は書式・作業境界のために参照した。クラス資料には Develop_Notes の Class テンプレートを優先し、全体説明には DesignDraft の「基本方針 → 概念 → 処理・構成」の順を用いる。

既存フォルダの未コミット変更は保持する。新しい設計は DevelopNotesDesign に置き、ルート README の新資料への入口には参照コミットを更新する。OpenQuestions に利用者が記入した「回答:」は原文を保持する。

## 初回作成時の確認結果（2026-10-05）
---
- 参照元 main を資料作成後に再確認し、取得コミットから更新されていないことを確認した。
- 実資料の相対リンク 133 件について、参照ファイルと見出し anchor の存在を確認した。テンプレート中の Other.md などの例示リンクは検査対象から除いた。
- Mermaid 4 ブロックは、既存環境の Mermaid 12.1.0 と jsdom を使って構文解析できた。Obsidian 上での表示は未確認。
- 作業開始時の既存ファイル 62 件を SHA-256 で照合し、README 以外の 61 件が未変更であることを確認した。SimpleArchitecture／ImplementationSketches は内容を変更していない。
- Production コード・依存関係・Git のコミットは追加していない。概念宣言には未確定の型があり、C++ のビルド対象にはしていない。

## 今回の反映確認（2026-10-06）
---
- 更新ノートの参照コミットは 1e76747792192b2b56abf3da0dbddf2e2d731af5。Time と Boundary API の接続方法を更新し、回答で決まった EngineHandle・内部 lifecycle・時間更新・ApplicationAPI を反映した。
- 相対リンク 123 件について参照ファイルと見出し anchor の存在を確認した。テンプレート内の例示リンクは除外した。
- Mermaid 5 ブロックを既存の Mermaid と jsdom で構文解析できた。Obsidian 上での表示は未確認。
- OpenQuestions の回答原文 12 件を保持した。EngineAPI の全操作が EngineHandle を使い、Engine 内部の Initialize／Finalize が残ることを確認した。
- 作業開始時の 92 ファイルの SHA-256 を照合し、今回変更した 13 ファイル以外が未変更であることを確認した。SimpleArchitecture／ImplementationSketches／.obsidian は変更していない。
- 設計資料のみの更新であり、Production コードの実装・C++ ビルド・依存関係の追加・コミットは行っていない。
