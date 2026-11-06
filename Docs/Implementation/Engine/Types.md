# Engineの型とDLL境界

[処理・構造](README.md)

| 型・入口 | 役割 | 根拠 |
| --- | --- | --- |
| `Hestia::Engine` | 各System、API、debug Materialを値所有。Graphics/Asset/ECS等の初期化状態を個別に保持。ApplicationAPIのcontextは非所有 | [Engine.h](../../../Engine/Include/Engine.h) |
| `EngineHandle` | Engine実体を隠すvoid*。生成したDLLのDestroyへ戻す。任意のvoid*を渡せる契約ではない | [EngineAPI.h](../../../Engine/Include/EngineAPI.h) |
| `EngineAPI` | Create、Destroy、ProcessMessage、FixedUpdate、FrameExecuteの関数テーブル。Engine実体の代わりにApplicationが保持する | 同上 |
| `GetEngineAPI` | C linkageのexport入口。テーブルはDLL所有で、呼び出し側はdeleteしない | [EngineAPI.cpp](../../../Engine/Source/EngineAPI.cpp) |

更新・通知用wrapperはnull Handleなら何もしない。Create時のnull APIは失敗になる。非null HandleについてはEngineへのcastを行うため、型・生成元・寿命が正しいことは利用側の前提となる。

DLL export/importは [DllAPI.h](../../../Common/Include/DllAPI.h) で制御する。テーブルの寿命はDLLロード期間に従う。STLを含む各SystemのC++ APIと、この関数テーブルによる境界は区別する。
