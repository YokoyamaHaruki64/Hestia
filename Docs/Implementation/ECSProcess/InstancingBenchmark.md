# インスタンシングのベンチマーク

`Engine::CreateDebugScene` は100列×25行×40層の格子に立方体100,000個を置き、斜め上から透視投影する。全個体が一つのMeshとMaterialを共有し、Transformとインスタンス色を個別に持つ。デバッグ用の `MoveTransform` がUpdateでアクティブなTransformを回転させる。シーンの立方体は親を持たない。 [シーン生成](../../../Engine/Source/Engine.cpp) · [MoveTransform](../../../Engine/Source/Transform/TransformProcess.cpp) · [Unlit_Inst.matsh](../../../Assets/Shaders/MatShaders/Unlit_Inst.matsh)

2026-11-10にユーザー環境で、Transform更新ありの100,000オブジェクトで **70 FPS（約14.3 ms／フレーム）** を確認した。CPU側のPass処理は直列で、Mesh・Materialが異なるシーンの性能はこの記録に含まない。ハードウェア構成、ビルド構成、計測時間は記録されていない。

この計測では提出キュー上限をローカルで1,000,000件に変更した。通常描画は1フレーム65,536件分のObject定数領域を使うため、上限変更を取り込む際は通常描画の書き込み範囲を先に見直す。
