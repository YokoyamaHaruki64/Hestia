# Commonの処理・構造

[全体索引](../README.md) / [型](Types.md)

CommonはSystemの更新周期を持たない共通ライブラリであり、値型とheader内の演算を中心に構成する。[HMath.h](../../../Common/Include/HMath.h) が数学型の入口、[MathCommon.h](../../../Common/Include/Math/MathCommon.h) が定数・基本数学関数、[SimdInterop.h](../../../Common/Include/Math/SimdInterop.h) が通常型とSIMD型の相互運用を担う。

## 数学の規約

[Matrix4x4.h](../../../Common/Include/Math/Matrix4x4.h) の規約はrow-major、行ベクトル `v * M`、`m[row][column]`、左手系。SRTは `S * R * T`、親子は `Local * ParentWorld`。GraphicsのView/Projectionもこの規約で渡す。既定構築したMatrix4x4はゼロであり、単位行列が必要ならIdentityを使う。

通常のVector/Matrix/Quaternion型は保存・一般演算を担い、SIMD型はSIMD演算と通常型へのLoad/Store等を提供する。行参照proxyやSIMD内部表現を永続化用のファイル形式として扱わない。投影のnear/far等の前提は各関数のassert・契約に従う。

## Curve、Color、Random

[Curve.h](../../../Common/Include/Math/Curve.h) はキー時刻を0〜1へ収めて並べ、Evaluate時刻も0〜1へ収める。空ならT{}、単一キーならその値、範囲端なら端の値を返し、内部区間を制御値による補間で評価する。Curve<float>の既定構築は0→1のキーを持つ。Vector3/Color版は成分別のCurve<float>をまとめる。

[Color.h](../../../Common/Include/Color.h) はRGBAとHSVの変換、色の演算を提供する。成分演算を行うため、すべての操作が自動的に0〜1へ制限されるとは扱わない。

[Random.h](../../../Common/Include/Random.h) は関数内staticのrandom_deviceとmt19937を使う。float Rangeは実数分布、int Rangeは両端を含む整数分布。InsideUnitSphereは最大32回の棄却試行後Upを返し、OnUnitSphereは小さいベクトルの場合もUpを返す。RandomConeは角度を度で受け、方向周りの円錐内方向を生成する。公開seed設定・状態保存・thread別engineは実装されていない。

DLL export/importとWindowsのinclude設定は [DllAPI.h](../../../Common/Include/DllAPI.h)、[WindowsHeaders.h](../../../Common/Include/WindowsHeaders.h) に集約する。これらは型を持つSystemではない。
