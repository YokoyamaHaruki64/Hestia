# Shader 読み込み・Reflection の参考実装

このフォルダーは、GM_Framework の Direct3D 11 向け Shader 読み込み処理を参照用に収録している。Hestia で採用する DSL と Asset の契約は [MaterialShader の設計](../HestiaDesign/DevelopNotes/Class/MaterialShader.md) と [MaterialShader DSL](../HestiaDesign/Graphics/MaterialShaderDSL.md) に従う。

## 参考実装の読み込み順

1. `ShaderManager` がパスで既存 Shader を検索し、未ロードなら `ShaderLoader` に渡す。
2. `ShaderLoader` が宣言と Pass を解析し、数値プロパティの CBuffer と Texture の SRV 宣言を各 Pass の HLSL に付ける。
3. `ShaderFactory` がコンパイル、VS 入力レイアウトの生成、VS／PS の Reflection と統合、Direct3D 11 オブジェクトの生成を担当する。
4. `ShaderLoader` が Pass をまたいでプロパティの Offset と Size の一致を確認し、`GraphicsShader` に保持する。

## ヘッダー

| ファイル | 内容 |
| --- | --- |
| [InputLayoutBuilder.h](include/InputLayoutBuilder.h) | VS の入力シグネチャから DX11 入力レイアウトと Vertex Stream の対応を作る入口。 |
| [ShaderCompiler.h](include/ShaderCompiler.h) | HLSL ソース、エントリーポイント、ターゲットプロファイルを受け取り、コンパイル済み Blob を返す宣言。 |
| [ShaderFactory.h](include/ShaderFactory.h) | VS／PS のコンパイル、DX11 オブジェクト生成、Reflection 統合をまとめる Pass 生成の宣言。 |
| [ShaderInc.h](include/ShaderInc.h) | Stage、CBuffer、変数、Texture、入力スロット、型付きプロパティの共通記述。プロパティ型から HLSL 型への変換も持つ。 |
| [ShaderLoader.h](include/ShaderLoader.h) | ファイル単位のロードと、プロパティに反映する共通 CBuffer レイアウトの解決を宣言する。 |
| [ShaderManager.h](include/ShaderManager.h) | Shader の所有、パス／ID 検索、既定 Shader と個別削除の API。 |
| [ShaderParser.h](include/ShaderParser.h) | Shader 名、プロパティ、Pass、描画状態、HLSL、独自 pragma の抽出と既定値のパースを宣言する。 |
| [ShaderProgram.h](include/ShaderProgram.h) | `GraphicsShader` と `GraphicsShaderPass` の保持データ。プロパティと Pass の索引、DX11 オブジェクト、Reflection 結果を含む。 |
| [ShaderReflector.h](include/ShaderReflector.h) | DX11 Reflection の取得と、Stage ごとの CBuffer、変数、Texture 情報の出力を宣言する。 |
| [ShaderToken.h](include/ShaderToken.h) | 旧 DSL のキーワード、コメント、ブロック境界、pragma、描画状態の文字列定数。 |

## 実装ファイル

| ファイル | 内容 |
| --- | --- |
| [InputLayoutBuilder.cpp](source/InputLayoutBuilder.cpp) | VS 入力のセマンティクスと型から Stream、形式、オフセットを解決して `ID3D11InputLayout` を生成する。Instance ID は入力レイアウトから除く。 |
| [ShaderCompiler.cpp](source/ShaderCompiler.cpp) | `D3DCompile` で HLSL をコンパイルし、標準の include 解決とコンパイラー診断を扱う。 |
| [ShaderFactory.cpp](source/ShaderFactory.cpp) | VS／PS、入力レイアウトを生成し、両 Stage の CBuffer／Texture 情報を統合して Pass に設定する。 |
| [ShaderLoader.cpp](source/ShaderLoader.cpp) | ファイル読込から Shader 生成までを調停する。旧仕様の `Properties : register(b3)` と SRV 宣言を挿入し、Pass タグと共通レイアウトを検証する。 |
| [ShaderManager.cpp](source/ShaderManager.cpp) | `unique_ptr` で Shader を所有し、同一パスのロードを再利用する。起動時の既定 Shader 読込と終了時の解放を担う。 |
| [ShaderParser.cpp](source/ShaderParser.cpp) | コメントを処理し、ブロック、ステート指定、エントリーポイント、行単位のプロパティ宣言と既定値を読み取る。 |
| [ShaderProgram.cpp](source/ShaderProgram.cpp) | DX11 Device Context への Pass のバインドと、プロパティおよび Pass の検索を実装する。 |
| [ShaderReflector.cpp](source/ShaderReflector.cpp) | コンパイル済み Blob から DX11 Reflection を取得し、CBuffer のバインドとサイズ、変数の Offset／Size、Texture スロットを列挙する。 |

## Hestia へ接続する際の対応

- 所有と名前キャッシュは `AssetSystem` に置く。`MaterialShader` が保持するのは CPU 上の型付きプロパティ、共通レイアウト、VS／PS バイトコード、入力レイアウト、Pass 情報。
- 数値は `PerMaterial : register(b2)`、Material の Texture は `register(tN, space1)` に配置する。VS／PS と全 Pass の Reflection から共通レイアウトを確定し、使用されない値もスキーマに残す。
- VS 入力は現行の Mesh Stream と対応付ける。PSO の生成と描画コマンドの記録は DX12 の描画側で扱う。
- DSL は `MaterialShader`、`SHARED_HLSL`、Pass 内の独自 pragma と現行の描画状態に従う。Forward Pass をコンパイルし、GBuffer Pass は `#pragma surface` と設定を解析したうえで未実装エラーを返す。
