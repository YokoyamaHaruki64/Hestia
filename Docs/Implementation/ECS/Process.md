# ECS Processの実行と型

[処理・構造](README.md) / [Entity・配置の型](Types.md)

根拠: [ECSProcessExecution.h](../../../Engine/Include/ECS/ECSProcessExecution.h)、[ECSProcess.h](../../../Engine/Include/ECS/ECSProcess.h)、[ECSProcess.cpp](../../../Engine/Source/ECS/ECSProcess.cpp)、[ECSWorld.h](../../../Engine/Include/ECS/ECSWorld.h)

## 関数の契約

Process関数はfree関数またはstatic関数で、`void(size_t count, const Entity* entities, Components*...)` 形式。noexcept付きも扱う。Component引数はSoA列の先頭を指し、count行分の対応する配列として使用する。const Component*はRead、それ以外はWriteとして記録する。EntityをComponentとして要求することと、ポインタ型そのものをComponentとすることはstatic_assertで拒否する。同一型やconst違いの重複要求は、ECSProcess::Create時に型IDで検査し、ログを出して生成を拒否する。

受け渡すcountにはinactiveと破棄予約済みEntityも含まれる。処理対象をactiveだけにする場合は、関数内で各Entity.IsActiveを確認する。実行器がactive行だけに圧縮して渡す仕様ではない。

## 検索・実行

登録時に関数型から要求型IDとRead/Writeを生成し、共通入口を作る。要求する型をすべて含むArchetypeが対象となり、MatchesArchetypeで検証したものだけRegisterArchetypeへ渡す。Processごとに非所有Archetype参照と引数順の列番号をcacheする。Archetypeの型ID順と関数引数順が異なっても列番号で接続する。新しいArchetypeの接続中に例外が起きた場合は、途中まで登録した非所有参照を解除してから失敗を返す。

World.Runは指定phaseのProcessを登録順に処理する。Processは対象ArchetypeのChunkを順に走査し、空Chunkを除いて共通入口を呼ぶ。同じ関数をphaseを変えて重複登録することはできない。Process完了後に保留構造変更を反映する。Read/Write情報は保持するが、依存グラフに基づく並列スケジューリングは実装されていない。

Process関数が例外を送出すると、そのProcessの残りのChunk走査は中断する。Worldが例外を記録し、実行中フラグを戻して受付済みの構造変更を反映した後、現在のRunを終了する。同じRunの後続Processは呼ばない。既に書き換えたComponent値は巻き戻さない。noexcept宣言した関数からの例外はterminateになるため、このcatchでは回復しない。

## 型一覧

| 型・定義 | 責務 |
| --- | --- |
| `ECSProcessPhase` | FrameBegin / Update / LateUpdate / PreRender / FixedUpdate。実際の順序はEngine側の呼び出しで決まる |
| `ECSComponentAccess` | Read / Write |
| `ECSProcessComponent` | ComponentTypeIdとアクセス種別 |
| `ECSProcessInvoke` | `void (*)(Chunk&, std::span<const size_t>)` の共通呼出型 |
| `ECSProcessFunctionTraits<F>` | 関数ポインタ型の特殊化に要求情報生成とInvokeをまとめる。noexcept版は通常版を継承して再利用 |
| `ProcessKey<Function>` | 関数ごとの共通入口を表すconstexpr変数。型ではなく関数重複判定のkey |
| `ECSProcessArchetype` | 非所有Archetype*と要求順の列番号vector |
| `ECSProcess` | phase、要求情報、対象Archetype cache、invokeを保持。Worldが所有し、Archetypeは借用 |
