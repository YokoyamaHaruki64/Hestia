# MaterialShader と DX12 Forward 描画の実装仕様

[設計書](../../Design/MaterialShader.md) / [Graphics](README.md) / [Asset](../Asset/README.md) / [全体索引](../README.md)

2026-11-07 時点のソースを根拠とする。従来の CSO 描画と、MaterialShader を用いる Forward 描画が共存している。

## Asset と Property

| 型・処理 | 現行の契約 |
| --- | --- |
| [AssetSystem](../../../Engine/Source/Asset/AssetSystem.cpp) | `MaterialShader` は名前から .matsh をロードできる。 `Material` は Shader を設定して手動登録する。Material ファイルの Loader はない。 |
| [MaterialShader](../../../Engine/Include/Asset/MaterialShader.h) | Shader 名、共通 Property、CBuffer サイズ、宣言順の Pass、参照中の Material の逆索引を保持する。Create は Property 名・ハッシュ衝突・SRV Slot の重複・CBuffer 範囲と重なり・Pass の要求 Stream を検証する。 |
| [Material](../../../Engine/Include/Asset/Material.h) | 型ごとの `SetBool`、`SetInt`、`SetFloat`、`SetFloat2/3/4`、`SetColor`、`SetMatrix3x3/4x4`、`SetTexture` と対応する Getter を提供する。名前と ID の両方を受ける。Setter は成功時に true、取得に失敗した Getter はログを出して型の既定値を返す。 |
| [MaterialPropertyDesc](../../../Engine/Include/Asset/MaterialProperty.h) | 名前、型付き既定値、CBuffer の Offset とサイズ、SRV Slot、Reflection 上の行列方向。定数・SRV として使われない項目の Offset／Slot は `UINT32_MAX`。 |

`MakePropertyID` は名前のバイト列に [StringToHash](../../../Common/Include/StringUtils.h) の FNV-1a 64-bit を適用する。Material の数値を更新すると変更箇所を CBuffer キャッシュに反映する。Texture は Handle のみを更新する。同じ値の再設定では `GetValueRevision()` の世代を進めない。Shader が破棄されると Material は Shader 参照と値・定数キャッシュを解除する。 [Material.cpp](../../../Engine/Source/Asset/Material.cpp)

`bool` は 32-bit の 0／1 として書く。 `Matrix4x4` は行優先ならそのまま、列優先なら転置して書く。 `Matrix3x3` は Reflection の行／列方向に従い、各ベクトルを 16-byte 間隔に配置する。CBuffer の View は Material が所有する。 [PackConstant](../../../Engine/Source/Asset/Material.cpp)

## .matsh の解析とコンパイル

| 段階 | 現行の動作 |
| --- | --- |
| [Loader](../../../Engine/Source/Asset/MaterialShaderLoader.cpp) | UTF-8 のパスで .matsh を読み、Parser と Compiler を呼ぶ。名前付き Texture を AssetSystem で解決し、`white` は空 Handle として描画時の既定 Texture に委ねる。最後に MaterialShader を作成して登録する。 |
| [Parser](../../../Engine/Source/Asset/MaterialShaderParser.cpp) | `MaterialShader`、`Properties`、共通 HLSL、Pass を解析する。全体の描画設定を読み終えてから各 Pass に適用し、Pass 固有の設定を優先する。 |
| [DXMaterialShaderCompiler](../../../Engine/Source/Graphics/DXMaterialShaderCompiler.cpp) | 生成した Property 宣言・共通 HLSL・Pass 本文を連結し、`vs_6_0` と `ps_6_0` を `-Zpr` でコンパイルする。 `-I` には .matsh の親フォルダを渡す。DXC の診断をログに出し、Reflection の統合後に Bytecode と描画用データを返す。 |

Parser は DSL の空白とコメントを読み飛ばす。HLSL 本文は境界と行頭の `#` を探して一括コピーする。コメント・引用符内の境界は無視し、独自の `#pragma vertex`・`pixel`・`surface` を取り出した範囲だけを空白に置き換える。 `#include` などは HLSL 本文に残る。

Property は Bool、Int、Float、Vector2/3/4、Color、Matrix3x3/4x4、Texture2D を扱う。 `Range` は端点の順序を検証し、値型は Float として保持する。固定描画設定は `RenderType`、`RenderPhase`、`ShadingModel`、`Fill`、`Cull`、`Blend`、`Topology`、`ZWrite`、`ZTest` を受け付ける。Forward では vertex と pixel の指定が必要で、ShadingModel は `None`。GBuffer では Parser が vertex と surface の指定を確認し、Compiler が未実装エラーを返す。

生成する数値宣言は `PerMaterial` CBuffer にまとめる。Texture は宣言順に `t0` から割り当て、16 枠を超える宣言を拒否する。Reflection は使われた定数の Offset・サイズ・行列方向、使われた Texture の Slot を確定し、Stage／Pass 間の不一致を拒否する。Compiler が生成する `HESTIA_MATERIAL_PROPERTIES_DEFINED` は共通 HLSL の従来 Material 宣言と重複するのを防ぐ。

## 提出から描画まで

1. [RenderData](../../../Engine/Include/Graphics/DX12Object.h) に Mesh Handle、World 行列、MaterialSlot 順の `materials` を渡して `GraphicsSystem::Submit` する。 `materials` が空の場合は従来の `LegacyMaterial` と CSO の経路を使う。
2. Backend はアップロード時に GPU Mesh へ SubMesh の範囲をコピーする。SubMesh 未指定時は Index 全体を Slot 0 にする。描画時に Slot の Handle を解決し、Slot が範囲外または Material／Shader を解決できない場合はその SubMesh を飛ばす。
3. [DX12MaterialPipeline.cpp](../../../Engine/Source/Graphics/DX12MaterialPipeline.cpp) が、登録済み Shader Handle の index／generation と Pass 番号で PSO を検索する。未作成なら `m_requiredVertexStreams` に対応する要素を固定表から取り出し、Blend、Rasterizer、Depth、Topology と共に PSO を生成する。
4. [DX12Backend.cpp](../../../Engine/Source/Graphics/DX12Backend.cpp) が Opaque → Masked → Transparent の順に Forward Pass を実行する。Material 定数と Texture をバインドし、要求されたスロットの頂点バッファを設定して、SubMesh の `firstIndex` と `indexCount` で `DrawIndexedInstanced` を記録する。

Compiler は VS Reflection のシステム値を除外し、頂点入力のセマンティクス、インデックス 0、float32 型、成分マスクを確認する。対応する `VertexStream` は Pass 内に1件だけ登録する。Engine からの提出例は [デバッグ平面の初期化とフレーム更新](../../../Engine/Source/Engine.cpp) を参照。

`NormalTangent` を要求した PSO の Input Layout には `NORMAL` と `TANGENT` が入り、どちらも同一スロットを参照する。Mesh に任意 Stream がない場合は共通の既定バッファを使う。法線・接線の既定値は `(0, 0, -1)` と `(1, 0, 0, 1)`。 [AssetType.h](../../../Engine/Include/Asset/AssetType.h)

| Root Signature の用途 | レジスタ |
| --- | --- |
| フレーム・オブジェクト・Material の CBV | `b0`、`b1`、`b2`、いずれも space 0 |
| System Texture テーブル | `t0–t15`, space 0 |
| Material Texture テーブル | `t0–t15`, space 1 |
| 静的 Sampler | `s0–s3`、Linear／Point × Wrap／Clamp |

FrameContext は Material 定数用の 1 MiB の Upload ページを必要に応じて確保する。1 Draw ごとに 256-byte 境界の領域を割り当て、64 KiB を超える定数を拒否する。Texture Slot が空または GPU 側で解決できない場合は白 Texture を使う。Draw ごとに 16 個の SRV を Visible Heap にコピーする。定数領域と Visible Heap は対象フレームのフェンス完了後に再利用する。提出は最大 32768 件、Material 経路の Draw はフレームの SRV 使用数が上限に達すると打ち切る。

## ビルドと現在の制約

- 従来の Sprite VS／PS は [Engine プロジェクト](../../../Engine/Engine.vcxproj) の `FxCompile` で SM 6.0 にコンパイルする。
- .matsh は実行時に DXC を使うため、実行ファイルと同じ出力フォルダに `dxcompiler.dll` と `dxil.dll` を配置する。
- GBuffer 描画、LightingShader、透明物の距離ソート、個別アンロード時の PSO 回収は未実装。現状の PSO Cache は Backend 終了時に解放する。

## 検証記録

- 2026-11-07 の Forward 描画統合時に、App の `Debug_Editor|x64` ビルドと関連テスト 7 件が成功した。 [Loader テスト](../../../Test/Hestia_Tests/Source/AssetTests.cpp) · [DX12 統合テスト](../../../Test/Hestia_Tests/Source/AssetGraphicsTests.cpp)
- 追加した Forward 統合テストでは、NORMAL を要求する VS と法線 Stream のない Mesh を使い、Slot 1 の SubMesh で GPU が 1 プリミティブを処理し、PS を実行したことを統計で確認した。
