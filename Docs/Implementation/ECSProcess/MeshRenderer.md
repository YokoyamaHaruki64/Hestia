# MeshRenderer

[ECS](../ECS/README.md) / [Transform](Transform.md) / [RenderPassの設計](../../Design/RenderPass.md)

## Componentと描画提出

`MeshRenderer` は描画の有効フラグ、共通設定の格納先 `RendererSettings`、MeshとMaterialSlot順のMaterialへの非所有参照、インスタンス色を保持する。`RendererSettings` は空で、インスタンス色の既定値は白。Asset実体はAssetSystemが所有する。

`MeshRendererProcess` は `PreRender` で `TransformProcess` の後に登録されている。EntityとRendererが有効でMeshとMaterial配列が設定されている行から、`TransformCache` のWorld行列、Asset参照、インスタンス色を描画キューへ提出する。World行列とMaterialSlot配列は提出側にコピーする。 [ComponentとProcess](../../../Engine/Include/Graphics/MeshRenderer.h) · [ECS登録](../../../Engine/Source/ECS/ECSSystem.cpp)

## 動作確認用シーン

`Scenes/Startup.scene` は生成役の Entity と型 GUID を持つ。`DebugCubeGenerator::OnStart` が、組み込み Cube と `Materials/DebugCube.material` を共有する立方体を `100 × 25 × 40` の格子に生成する。位置とインスタンス色は個体ごとに設定し、固定のカメラ行列は Engine の起動処理で設定する。 [生成Script](../../../Assets/Gameplay/DebugCubeGenerator.cpp) ・ [共有Material](../../../Assets/Materials/DebugCube.material) ・ [Unlit_Inst.matsh](../../../Assets/Shaders/MatShaders/Unlit_Inst.matsh)

Scene・Prefab では `mesh` と `materials` を永続参照として保存する。InstanceColor は生成 Script や Game Facade が実行時に設定する。 [保存フィールドの定義](../../../Engine/Include/Graphics/MeshRenderer.h)

## Game 向け Facade

`World::AddProcess<HestiaGame::MeshRenderer>` で必要な Component を追加し、`World::GetProcess<HestiaGame::MeshRenderer>` で Facade を取得する。操作対象の Entity が破棄予約された場合、`IsValid` と各 Setter は `false` を返す。Facade の `IsActive`／`SetActive` は Renderer 自身の設定を扱う。 [MeshRenderer Facade](../../../Engine/Include/Game/MeshRenderer.h) ・ [MeshRendererAPI](../../../Engine/Source/API/ECSProcess/MeshRendererAPI.cpp)

Mesh と Material を `AssetHandle` で識別し、Engine 側で登録済みの Asset を解決する。`SetMaterial(index, material)` は `index` の MaterialSlot を更新する。必要に応じて空スロットを挟んで配列を広げ、空の Handle を既存スロットに渡すとそのスロットを解除する。`GetMaterialCount` は空スロットを含む数を返す。Material の取得には `Assets::LoadMaterial(path)` を使える。 [Game 向け Asset 操作](../Asset/Types.md) ・ [Process の登録と公開](../ECS/DevelopmentGuide.md#公開processのapiとfacade)
