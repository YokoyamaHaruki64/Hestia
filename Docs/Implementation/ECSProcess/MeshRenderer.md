# MeshRenderer

[ECS](../ECS/README.md) / [Transform](Transform.md) / [RenderPassの設計](../../Design/RenderPass.md)

## Componentと描画提出

`MeshRenderer` は描画の有効フラグ、共通設定の格納先 `RendererSettings`、MeshとMaterialSlot順のMaterialへの非所有参照、インスタンス色を保持する。`RendererSettings` は空で、インスタンス色の既定値は白。Asset実体はAssetSystemが所有する。

`MeshRendererProcess` は `PreRender` で `TransformProcess` の後に登録されている。EntityとRendererが有効でMeshとMaterial配列が設定されている行から、`TransformCache` のWorld行列、Asset参照、インスタンス色を描画キューへ提出する。World行列とMaterialSlot配列は提出側にコピーする。 [ComponentとProcess](../../../Engine/Include/Graphics/MeshRenderer.h) · [ECS登録](../../../Engine/Source/ECS/ECSSystem.cpp)

## 動作確認用シーン

`Engine::CreateDebugScene` は、一つのMeshとMaterialを共有する立方体6個を作る。位置とインスタンス色は個体ごとに設定する。 [シーン生成](../../../Engine/Source/Engine.cpp) · [Unlit_Inst.matsh](../../../Assets/Shaders/MatShaders/Unlit_Inst.matsh)
