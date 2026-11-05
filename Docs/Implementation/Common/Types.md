# Commonの型

[処理・構造](README.md)

## ベクトル・行列・回転

| 型 | 役割・表現 | 定義 |
| --- | --- | --- |
| `Vector2` | floatのx/y。2次元の成分演算、長さ、正規化、補間等 | [Vector2.h](../../../Common/Include/Math/Vector2.h) |
| `Vector3` | floatのx/y/z。3次元方向・位置、内積・外積等 | [Vector3.h](../../../Common/Include/Math/Vector3.h) |
| `Vector4` | floatのx/y/z/w。4成分・同次座標等 | [Vector4.h](../../../Common/Include/Math/Vector4.h) |
| `Vector2_SIMD` | 2成分Vector演算のSIMD版 | [Vector2_SIMD.h](../../../Common/Include/Math/Vector2_SIMD.h) |
| `Vector3_SIMD` | 3成分Vector演算のSIMD版 | [Vector3_SIMD.h](../../../Common/Include/Math/Vector3_SIMD.h) |
| `Vector4_SIMD` | 4成分Vector演算のSIMD版 | [Vector4_SIMD.h](../../../Common/Include/Math/Vector4_SIMD.h) |
| `Matrix3x3` | 3×3行列の値型。積、転置、逆行列など | [Matrix3x3.h](../../../Common/Include/Math/Matrix3x3.h) |
| `Matrix4x4` | float m[4][4]。行ベクトル変換、SRT、View/Projection生成等 | [Matrix4x4.h](../../../Common/Include/Math/Matrix4x4.h) |
| `Matrix3x3_SIMD` | 3×3行列演算のSIMD版 | [Matrix3x3_SIMD.h](../../../Common/Include/Math/Matrix3x3_SIMD.h) |
| `Matrix4x4_SIMD` | 4×4行列演算のSIMD版。Graphicsの定数にも使用 | [Matrix4x4_SIMD.h](../../../Common/Include/Math/Matrix4x4_SIMD.h) |
| `Matrix3x3_SIMD::RowReference` / `ConstRowReference` | SIMD内部の行を添字経由で読み書きするproxy / 読み取りproxy。所有行列の寿命に依存 | 同上の3×3定義 |
| `Matrix4x4_SIMD::RowReference` / `ConstRowReference` | 4×4版の行参照proxy / 読み取りproxy | 同上の4×4定義 |
| `Quaternion` | floatの回転四元数。積、正規化、回転生成・補間等 | [Quaternion.h](../../../Common/Include/Math/Quaternion.h) |
| `Quaternion_SIMD` | 回転四元数のSIMD版 | [Quaternion_SIMD.h](../../../Common/Include/Math/Quaternion_SIMD.h) |

## 曲線・色・乱数

| 型 | 役割・契約 | 定義 |
| --- | --- | --- |
| `Curve<T>` | vector<Key>を所有し、キー編集、時刻正規化、評価、stream入出力を提供 | [Curve.h](../../../Common/Include/Math/Curve.h) |
| `Curve<T>::Key` | value、time、inControl、outControl。値だけ指定すると制御値も同じ値になる | 同上 |
| `CurveVector3` | x/y/zごとのCurve<float>を所有し、Vector3として評価 | 同上 |
| `CurveColor` | RGBA成分ごとの曲線をまとめ、Colorとして評価 | 同上 |
| `ColorHSV` | h/s/v/a。既定値はh/s/v=0、a=1 | [Color.h](../../../Common/Include/Color.h) |
| `Color` | r/g/b/a。既定は不透明白。成分演算、色生成、HSV変換など | 同上 |
| `Random` | staticの範囲乱数・球内/球面/円錐方向生成。mt19937は内部共有状態 | [Random.h](../../../Common/Include/Random.h) |

Math名前空間とSIMD相互変換関数は型ではないが、演算規約の構成要素として [処理・構造](README.md) に含める。各演算の詳細な引数・戻り値・assert条件はリンク先の宣言と実装に対応する。
