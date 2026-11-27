# インスタンシングのベンチマーク

計測当時は100列×25行×40層の格子に立方体100,000個を置き、斜め上から透視投影した。全個体が一つのMeshとMaterialを共有し、Transformとインスタンス色を個別に持っていた。現在の起動デモは Scene から復元した Script で立方体を生成する。 [現在の動作確認用シーン](MeshRenderer.md#動作確認用シーン)

## Script 版と Process 版の比較

2026-11-19 に報告されたユーザー実測。立方体 100,000 個を描画し、毎 `Update` で Z 軸まわりに 0.01 ラジアンの回転を加える処理を切り替えて比較した。

| 更新方式 | 回転の実行経路 | FPS | FPS から換算したフレーム時間 |
| --- | --- | ---: | ---: |
| Script 版 | Game.dll の `RotateTransformScript` が、`OnStart` で取得した Transform Facade の Getter／Setter を使う | 約 55 | 約 18.2 ms |
| Process 版 | Engine の `MoveTransform` が、ECS の Transform Component 列を直接更新する | 約 77 | 約 13.0 ms |

当時の Script 版は手書きの Game Setup でアタッチしていた。Process 版の更新関数は Engine 側にある。 [MoveTransform](../../../Engine/Source/Transform/TransformProcess.cpp)

| 測定環境 | 構成 |
| --- | --- |
| CPU | Intel Core i5-14600KF（14 コア／20 スレッド） |
| GPU | NVIDIA GeForce RTX 4060 Ti（VRAM 8 GB、ドライバー 610.88） |
| メモリ | 32 GiB |
| OS | Windows 11 Home 25H2（ビルド 26200） |

FPS は描画を含めたアプリ全体の観測値。ビルド構成、実行時の解像度、測定時間、集計方法は未記録。

## 2026-11-10 の測定

2026-11-10にユーザー環境で、Transform更新ありの100,000オブジェクトで **70 FPS（約14.3 ms／フレーム）** を確認した。CPU側のPass処理は直列で、Mesh・Materialが異なるシーンの性能はこの記録に含まない。ハードウェア構成、ビルド構成、計測時間は記録されていない。

この計測では提出キュー上限をローカルで1,000,000件に変更した。通常描画は1フレーム65,536件分のObject定数領域を使うため、上限変更を取り込む際は通常描画の書き込み範囲を先に見直す。
