# Develop Notes に基づく Hestia 設計
---
Develop_Notes の最新ノートを設計の根拠として、責務・所有関係・主要メンバ・Public API を整理する。
クラス宣言は概念設計用であり、Production コードやそのままコンパイルする Header ではない。

## 基本方針
---
設計の正は [Develop_Notes](https://github.com/chaba0426/Develop_Notes) の `main`。取得したコミットと全出典は [SourceNotes](./SourceNotes.md) に記録する。ノートに存在する宣言はできるだけそのまま引き継ぐ。ノート間で一致しない箇所は、片方を暗黙に採用しない。今回の回答で決まった内容は OpenQuestions に記録し、原文よりその確認内容を優先する。

[ClassDesign](./ClassDesign.md) の順に責務、所有・参照、主要メンバ、主要 API を考える。[NamingConvention](./NamingConvention.md) と [記述テンプレート](./Class/_FormatTemplate.md) に従う。ワークスペースの見出し下線ルールを追加適用する。

この資料の開始時は Develop_Notes だけを設計根拠とした。未記載の System は後続でこのリポジトリの情報を繋げて構築する方針とし、今回は詳細を未確定に留める。`SimpleArchitecture/`、`ImplementationSketches/`、過去チャット・記憶の設計判断は自動的に持ち込まない。複雑な失敗状態、Validation、Result 型、抽象 Interface、厳密な寿命保証などを追加しない。Engine 内部の Initialize／Finalize、所有者、通常の破棄順序は維持する。各 System に一律の Initialize は要求しない。

## 基本概念
---
- Application は Window、Engine.dll、条件付き Editor とメインループを管理する。
- Application が受け取る Engine の識別子は `Hestia::EngineHandle = void*`。全 EngineAPI の入口で同じ Handle を使用する。
- Engine は Subsystem、Boundary API、Game.dll、GameRuntime の寿命と更新を管理する。
- Subsystem は Engine 内部の状態を保持する。static は一部の便利な入口であり、状態は instance member に置く。各 System は private s_instance を持ち、実体を取得する public Getter は設けない。
- Boundary API は System の利用準備後に Initialize(XxxSystem*) で非所有ポインタを設定し、Game に許可した操作だけを公開する。
- Engine が TimeSystem と TimeAPI を所有し、TimeAPI は TimeData を非所有で参照する。Game は HestiaGame::Time から時間を取得する。
- Game Facade は `HestiaGame` に置き、Boundary API を非所有で参照する。
- GameRuntime は Game.dll 内部に隠し、Engine は opaque handle と GameRuntimeAPI を使用する。

## 設計資料
---
| 資料 | 対象 |
| --- | --- |
| [Application](./Class/Application.md) | Application、ApplicationAPI、Window、Clock |
| [Engine](./Class/Engine.md) | Engine、EngineHandle、EngineAPI、GameEngineAPI、GameRuntimeAPI |
| [AssetSystem](./Class/AssetSystem.md) | AssetSystem／AssetAPI／Assets の位置。具体的な Asset 設計は再検討 |
| [Subsystems](./Class/Subsystems.md) | GraphicsSystem、InputSystem、AudioSystem、PhysicsSystem と Boundary API／Facade |
| [GameRuntime](./Class/GameRuntime.md) | GameRuntime と Facade の Bind／Unbind |
| [Time](./Class/Time.md) | TimeSystem、TimeData、TimeAPI、HestiaGame::Time |
| [Editor](./Class/Editor.md) | Application が保持する Editor の未確定箇所 |
| [SubsystemApiFacadeDesign](./SubsystemApiFacadeDesign.md) | System／Boundary API／Facade の関係 |
| [Relationships](./Relationships.md) | 所有・参照・呼び出しを示す Mermaid 図 |
| [OpenQuestions](./OpenQuestions.md) | ノートの矛盾・不足と、その直下の提案 |

## 記載の読み方
---
「ノート由来」は出典で確認できる内容。「追加草案」はノートの責務をクラス宣言に展開した未採用の提案。「未確定」はまだ決めていない内容。「決定」は今回の回答・会話で確認した内容とする。OpenQuestions の回答原文は残す。

クラス資料の見出しは設計対象型に使い、各型を **役割 → 説明 → 宣言 → 参照リンク → 所有・参照／補足** の順で記載する。判断が必要な箇所には「未確定」と書き、その直下に「提案」を置く。詳細な矛盾の記録は OpenQuestions を参照する。

## 通常の処理の流れ
---
Application が Window を生成する。LoadEngine が DLL ロード → GetEngineAPI 取得 → ApplicationAPI 作成 → Create を行い、初期化済み EngineHandle を保持する。Create 内では引数なしで Engine を生成し、Engine::Initialize(applicationAPI) を呼ぶ。

Engine は各 System の利用準備後に Boundary API へポインタを設定し、GameEngineAPI を構成して GameRuntime に渡す。GameRuntime が private Bind／Unbind を呼べる friend として各 Facade を接続する。

Application が実経過時間を蓄積し、固定刻みを消費するたびに FixedUpdate を呼ぶ。Time の通常更新は Engine::FrameExecute 内で行い、その後 GameRuntime::Update を実行する。GameRuntime の更新は引数なしで、時間を HestiaGame::Time から読む。その他の System の詳細な更新順は未確定。

WindowProc は Engine／Editor に void の通知として配送し、Application の処理後に DefWindowProc の返り値を返す。Engine は HWND member を持たず、必要な場面で ApplicationAPI::GetWindowHandle を利用する。

終了時は Application が Destroy を呼び、Engine::Finalize → delete までを Engine.dll 内で行う。GameRuntime は終了処理 → Facade Unbind → 破棄の後に Game.dll を Unload する。Destroy が返ってから Application が Engine.dll を Unload する。

Game → Application は別の公開 API を GameRuntime に渡す方向。API の型と操作は未確定。Editor は Runtime と Game が形になってから詳細を検討する。
