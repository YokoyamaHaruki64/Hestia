# ノートの回答と設計判断
---
回答原文を保持し、更新ノートと今回の会話で決まった内容を整理する。
未確定事項はその直下に提案を置き、仮置きの内部構造を正式仕様として扱わない。

取り消し線は回答済みの見出しを表す。回答後も残る検討事項は上段へ分け、回答原文と決定の記録は下段に保持する。

## 未解決項目
---
### U03：Game → Application の API
---
**未確定**：Game → Application の API の具体的な型・操作・引数は未確定。

**提案**：Game に必要な Application 操作と各 System の用途を整理してから、対応する API と処理順を追加する。

[回答・決定の記録](#u03：Time とフレーム順序)

### U06：System の s_instance の登録・解除
---
**未確定**：未記載 System の登録・解除の具体的なタイミングは lifecycle と一緒に設計する。

**提案**：個別 System を設計する際に s_instance の関連付けを決め、共通基底や汎用 singleton 管理層は増やさない。

[回答・決定の記録](#u06system-の-static-入口)

### U07：Asset の具体設計
---
**未確定**：Asset 固有の責務・保持型・操作・共有型は一から検討する。

**提案**：後続でこのリポジトリの関連情報と必要な用途を繋げて構築する。今回は型や機能を補完しない。

[回答・決定の記録](#u07asset-と-dll-境界共有型)

### U08：Editor の詳細設計
---
**未確定**：DLL／exe 配置、編集情報、UI 状態、フレーム内の詳細な処理順は未確定。

**提案**：Runtime と Game の構造が固まった段階で、表示・変更する情報を起点に設計する。

[回答・決定の記録](#u08editor)

### U10：GameRuntime の内部構造の詳細
---
**未確定**：SparceScript 等を保持する GameLogic 側の基盤になる方向は候補。具体的な所有型・API・内部構成は未確定。

**提案**：ゲーム状態の要件と関連情報を繋げて、保持する型と責務を後続で決める。

[回答・決定の記録](#u10gameruntime-の内部構造)

### U12：未記載 System の具体設計
---
**未確定**：内部状態、必要な機能 API、共有型、更新順序は後続で検討する。

**提案**：このリポジトリの情報を繋げて用途ごとに設計し、現段階で不足を推測して確定しない。

[回答・決定の記録](#u12graphicsinputaudiophysics-の詳細)

## 回答済み
---
### ~~U01：GameEngineAPI の保持方法~~
---
**決定**：Engine が各 System と Boundary API を値所有し、GameEngineAPI は各 API の非所有ポインタを保持する。System の利用準備後に API::Initialize(XxxSystem*) で接続する。通常 member は m_lowerCamel とする。更新ノートでもこの所有構造に統一された。

### ~~U02：Engine の初期化と API テーブル~~
---
**決定**：EngineAPI::Create は引数なしの new Engine() → Engine::Initialize(applicationAPI) → 初期化済み EngineHandle を返す、の順で処理する。EngineAPI::Destroy は Finalize → delete を行う。Engine 内部の Initialize／Finalize は残し、API テーブルに独立した Initialize／Finalize は置かない。

Application は DLL ロード → GetEngineAPI 取得 → Create → フレーム実行 → Destroy → DLL Unload を行う。ApplicationAPI は constructor ではなく Engine::Initialize で受け取って値保持する。

最新ノートの EngineAPI にある独立した Initialize／Finalize は、今回の回答と一致しないため新資料には戻さない。

### ~~U03：Time とフレーム順序~~
---
**決定**：更新された [Time](./Class/Time.md) の TimeSystem／TimeData／TimeAPI／HestiaGame::Time を採用する。Engine が System と API を値所有し、GameEngineAPI に TimeAPI* m_time を置く。Getter は TimeData を inline で読み、SetTimeScale は System へ委譲する。

**決定**：TimeSystem::Update は Engine::FrameExecute 内で行う。Application が実経過時間を m_accumulatedTime に蓄積し、固定刻みを消費するたびに EngineAPI::FixedUpdate を呼ぶ。Engine の固定更新経路で TimeSystem::UpdateFixed を呼ぶ。TotalTime は通常の Update でのみ加算する。

**決定**：固定時間更新は TimeScale によって増減する。

**決定した方向**：Game → Application は別途 API を GameRuntime に渡す。既存の ApplicationAPI を渡すか、Game 向けに別の公開 API を設けるかはまだ定めない。

回答:Timeの項目がDevelop_Notesで更新されているのでそのまま参照すること

回答:固定時間更新はTimeScaleによって増減する

### ~~U04：Facade の Bind／Unbind のアクセス~~
---
**決定**：各 Facade の Bind／Unbind は private のまま、GameRuntime だけを friend とする。GameRuntime の初期化・終了処理から接続・解除し、Game Script に公開しない。

回答:friend方式でOK,publicに出してscriptからBindを呼び出し可能になるのを防ぐ。

### ~~U05：ApplicationAPI の namespace~~
---
**決定**：境界共有型は Hestia::ApplicationAPI に統一する。Application と Window はグローバルに維持する。ApplicationAPI の friend は ::Application を指す。

回答:Hestia::ApplicationAPI でOK

### ~~U06：System の static 入口~~
---
**決定**：各 System は private の static XxxSystem* s_instance を保持する。static 入口がまだない System にも置く。System 実体や実体を取得する Getter は public にしない。状態自体は instance member に置く。

各 System に一律の Initialize は要求しない。TimeSystem は更新ノートの constructor／destructor の形を維持する。

回答:各Systemはstatic XxxSystem* s_instance を持つ方向性。static入口がないものも、あとから追加される可能性があるため保持しておく。ただし、インスタンスやGetterをpublicで公開することはしない。

### ~~U07：Asset と DLL 境界共有型~~
---
**決定**：SubsystemApiFacadeDesign の Asset 例は、実際の AssetSystem の仕様ではない。現資料の Load／IsLoaded／GetDetailedInfo と AssetID／Handle／Info／Storage の宣言を採用済み設計から取り下げる。AssetSystem.md 自体は残し、System／API／Facade の位置と確認済みの共通方針だけを記載する。

回答:元資料のSubSystemAPIFacedeDesignはあくまで例であり、今回のAssetSystemに準じていないので一度0から考え直す。消しても良い

### ~~U08：Editor~~
---
**決定した順序**：Editor の詳細設計は Runtime と Game が形になってから行う。Application が HESTIA_EDITOR の場合に値所有する位置は維持する。

回答:まずはRuntimeとGameが形になってから考える。後回しでよい

### ~~U09：Clock~~
---
**決定**：Clock は std::chrono::steady_clock の alias。時間計測用とし、フレーム待機の Win32 Timer とは役割を分ける。

回答:それでよい。というかそうした

### ~~U10：GameRuntime の内部構造~~
---
**決定した扱い**：GameRuntime はグローバル内部型の仮置きを維持する。GameEngineAPI を非所有で参照し、Facade の Bind／Unbind と GameRuntimeAPI からの更新をまとめる。

回答:一旦仮置きでよい。おそらくSparceScript等を保持するGameLogic側の基盤になると思われる

### ~~U11：GameEngineAPI の version と member 命名~~
---
**解消**：更新ノートでは version が削除され、GameEngineAPI の member が m_lowerCamel に統一された。新資料も同じ形を採用する。

回答:更新済み

### ~~U12：Graphics／Input／Audio／Physics の詳細~~
---
**決定した扱い**：Graphics／Input／Audio／Physics の詳細は未確定のままにする。System／API／Facade の所有・接続、private s_instance、GameRuntime の friend は共通方針として反映する。現資料の機能入口と lifecycle は採用前の追加草案として維持し、一律の Initialize を要求しない。

回答:まだこのDevelop_Notesも全部は網羅していない。Graphics/Input/Audio/Physicsの詳細はこの後詰めていく。

### ~~U13：WindowProc の返り値と関連付け~~
---
**決定**：Application が LRESULT WindowProc を持ち、Engine／Editor へ void の通知として配送する。Application の処理後、最後に DefWindowProc の返り値を返す。境界に処理済みフラグや結果型を追加しない。

Window の userData は Application への非所有の関連付けとして維持する。

回答:ApplicationがLRESULT WindowProcを持ち、Engine/Editorへ返り値voidで配送→最後にDefWindowProcで処理する

### ~~U14：Graphics 初期化と Window の受け渡し~~
---
**決定**：ApplicationAPI に HWND GetWindowHandle() const を追加する。Application が所有する Window の Handle を非所有で返す。Engine は HWND member を保持せず、必要な場面で API を呼んで System などへ渡す。

Graphics の具体的な保持方法は U12 の後続設計で決める。

回答:GetWindowHandleを作る。EngineはHWNDを保有しない。

### ~~U15：EngineHandle と DLL 境界~~
---
**決定**：共有 API Header に Hestia::EngineHandle = void* を置く。Create は EngineHandle を返し、Destroy／ProcessMessage／FixedUpdate／FrameExecute はすべて EngineHandle を受け取る。Application の m_engine は EngineHandle とする。

各 API の Engine.dll 内の入口で static_cast<Engine*>(handle) に戻す。Engine の型定義を Application 向け共有 Header に公開しない。Engine の生成・初期化・終了・破棄は Engine.dll 内で行う。

[EngineHandle](./Class/Engine.md#enginehandle) と [Application の生成手順](./Class/Application.md#application) に反映した。
