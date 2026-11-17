# Engineの型とDLL境界

[処理・構造](README.md)

| 型・入口 | 役割 | 根拠 |
| --- | --- | --- |
| `Hestia::Engine` | 各 System と API を値所有し、Game.dll のロード状態、GameRuntimeAPI の借用、不透明な Runtime Handle を保持。ApplicationAPI の context は非所有 | [Engine.h](../../../Engine/Include/Engine.h) |
| `EngineHandle` | Engine実体を隠すvoid*。生成したDLLのDestroyへ戻す。任意のvoid*を渡せる契約ではない | [EngineAPI.h](../../../Engine/Include/API/EngineAPI.h) |
| `EngineAPI` | Create、Destroy、ProcessMessage、FixedUpdate、FrameExecuteの関数テーブル。Engine実体の代わりにApplicationが保持する | 同上 |
| `GetEngineAPI` | C linkageのexport入口。テーブルはDLL所有で、呼び出し側はdeleteしない | [EngineAPI.cpp](../../../Engine/Source/API/EngineAPI.cpp) |

更新・通知用wrapperはnull Handleなら何もしない。Create時のnull APIは失敗になる。非null HandleについてはEngineへのcastを行うため、型・生成元・寿命が正しいことは利用側の前提となる。

DLL の export/import は `DllAPI.h` で制御する。テーブルの寿命は DLL のロード期間に従う。GameRuntime との双方向の接続は [GameRuntime の型と API](../GameRuntime/Types.md) を参照する。 [DllAPI.h](../../../Common/Include/DllAPI.h)
