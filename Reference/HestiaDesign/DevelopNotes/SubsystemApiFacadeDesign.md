# Subsystem / Boundary API / Game Facade Design
---
更新された Develop_Notes の同名資料と OpenQuestions の回答に基づく責務分担。
具体的な宣言は Class 配下、採用判断は OpenQuestions にまとめる。

## 基本方針
---
Engine 内部の System 本体を Game.dll に直接公開しない。System の操作のうち必要なものを Boundary API で選び、Game Script は HestiaGame の static Facade から呼ぶ。

```text
Engine 内部                 DLL 境界                 Game.dll 内
XxxSystem      ← System* ── XxxAPI      ← API* ───── HestiaGame::Xxx
instance 状態               Engine 所有               static 入口
```

System と境界共有型は Hestia、Script 向け Facade は HestiaGame に置く。共通基底 ISystem／ISystemAPI は設けない。

## System
---
Engine が各 System を値として所有する。状態は instance member に置き、Engine 内部で広く使う操作の一部だけ static 入口を設ける。

各 System は private の static XxxSystem* s_instance を持つ。static 入口が未定でも保持し、System 実体・実体の取得 Getter は public に公開しない。静的な状態コンテナや汎用 singleton 管理機構は追加しない。

static／instance と Game への公開可否は別の判断である。lifecycle は各 System に合わせ、一律の Initialize を要求しない。TimeSystem は更新ノートの constructor／destructor と Update／UpdateFixed を持つ。

## Boundary API
---
Engine が各 Boundary API を値として所有する。API はデフォルト構築し、System の利用準備後に Initialize(XxxSystem*) で非所有ポインタを設定する。現在の System 参照を受け取る constructor から更新する。

API 自体は System の実データを所有せず、Game に許可した操作だけを公開する。通常の公開操作は static 入口または System instance の操作へ委譲する。

TimeAPI は非所有の TimeSystem* と const TimeData* を保持する。Getter は inline で TimeData を読み、SetTimeScale のみ System へ処理を渡す。これは更新ノートに記載された時間取得の経路であり、すべての System に同じ共有データ方式を要求しない。

## GameEngineAPI と注入
---
GameEngineAPI は Engine 所有の各 API の非所有ポインタをまとめる。member は m_lowerCamel に統一し、version は置かない。TimeAPI* m_time を含める。

通常の順序は各 System の利用準備 → 各 API の接続 → GameEngineAPI への格納 → Game.dll ロード → GameRuntime へ注入 → Facade Bind。

## Game Facade と寿命
---
Facade は Game.dll 内の API への非所有ポインタを持つ。Bind／Unbind は private に残し、GameRuntime のみ friend にする。Game Script に接続操作を公開しない。

GameRuntime の終了処理後に Facade を Unbind し、GameRuntime を破棄してから Game.dll を Unload する。Hot Reload の際も Engine の System・API・GameEngineAPI を維持し、Game 側へ再注入する関係を保つ。検出機構や状態移行の詳細は未確定。

## DLL 境界
---
更新ノートは境界で Handle、ID、POD、明示的な View を使用し、STL container、所有が不明な生ポインタ、Engine 内部の実装型そのものを避ける。ApplicationAPI は Hestia 名前空間とし、Application 側の Context を通じて操作する。

Application → Engine の公開面は EngineAPI。EngineHandle をすべての操作へ渡す。Create／Destroy の中で Engine 内部の Initialize／Finalize を実行する。

Game → Application の操作は別途 API を GameRuntime に渡す想定。TimeAPI に混ぜず、具体的な型と公開操作は未確定。

## Asset 例と未記載 System
---
元資料の AssetSystem／Load／IsLoaded／AssetHandle などは三層構造を説明する例であり、実際の Asset 設計の正ではないことを確認した。Class/AssetSystem.md には System／API／Facade の位置と確認済みの接続方針だけを残し、Asset 固有の宣言を取り下げる。

**未確定（U07／U12）**：Asset、Graphics、Input、Audio、Physics の内部状態・公開操作・共有型は後続で検討する。

**提案**：このリポジトリの情報と必要な用途を繋げて個別に設計する。現段階では未記載の詳細を推測で完成させない。

[AssetSystem](./Class/AssetSystem.md)  
[Subsystems](./Class/Subsystems.md)  
[Time](./Class/Time.md)  
[Engine](./Class/Engine.md)  
[OpenQuestions](./OpenQuestions.md)
