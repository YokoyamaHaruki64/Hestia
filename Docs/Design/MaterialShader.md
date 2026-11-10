# MaterialShader と Forward 描画の設計

2026-11-07 時点で採用した方針。クラスの定義、読み込み手順、現在の制約は [実装仕様書](../Implementation/Graphics/MaterialShader.md) を参照する。

## 責務と所有

| 要素 | 責務と所有するデータ |
| --- | --- |
| `MaterialShader` | Property の共通定義と、各 Pass の描画設定・コンパイル済み Shader・必要な頂点 Stream を所有する。複数の Material が共有する。 |
| `Material` | Shader を借用し、Material ごとの型付き Property 値と GPU 転送用の定数キャッシュを所有する。 |
| `AssetSystem` | MaterialShader、Material、Mesh、Texture の CPU 実体を所有する。取得・登録には型付きHandleを使い、描画提出には解決済みの参照を渡す。 |
| `DXMaterialShaderCompiler` | 解析済みの DSL から HLSL を組み立て、DXC でコンパイルし、Reflection の結果を CPU Asset 用のデータに変換する。 |
| `DX12Backend` | PSO、GPU Buffer、Texture、Descriptor を所有する。Mesh と Material の組を解決し、Pass を描画する。 |

CPU Asset のアドレスは所有側から解放されるまで安定する。Material は Shader への非所有ポインタを持ち、Shader は参照中の Material の逆索引を持つ。Material の付け替えや破棄では旧 Shader の索引を解除し、Shader の破棄では参照先を Material 側から解除する。PSO は Backend が所有し、CPU Asset のポインタを GPU に渡さない。

## Material の値と Shader のレイアウト

- Property 名から `MakePropertyID` で ID を作り、型別の Setter／Getter でアクセスする。頻繁に使う名前は ID を再利用できる。
- Material の正本は型付きの値。CBuffer はその値を GPU 用に詰めたキャッシュとし、Texture は Handle で保持する。
- 共通 CBuffer の Offset、サイズ、行列の格納順、Texture の Slot は Reflection に従う。同じ Property を複数の Stage や Pass が使うときは、配置が一致することをロード時に確認する。
- Shader の付け替えでは同名・同型の値を引き継ぎ、それ以外は新しい Shader の既定値を使う。
- CPU の行列は行優先、変換は行ベクトル規約とする。HLSL の通常のコンパイルは `-Zpr` で行優先配置に揃え、式は `mul(v, M)` と書く。列優先が指定された場合は CBuffer へ詰める段階で Reflection に合わせる。

## .matsh の読み込み

1. Loader がファイルを読み、Parser に渡す。Parser は Property、共通 HLSL、Pass、固定描画設定とエントリーポイントを取り出す。
2. DXMaterialShaderCompiler が Property 宣言を生成し、共通 HLSL と Pass 本文から各 Stage をコンパイルする。HLSL 本文の解釈は DXC に委ね、DSL の Parser はブロック境界と独自 pragma を扱う。
3. Compiler が Reflection を統合し、Pass の Bytecode、描画設定、要求 Stream と、Shader 全体の Property 配置を返す。Loader は Texture 参照を解決して Asset を登録する。

現在の描画対象は Forward Pass。GBuffer の宣言は Parser で扱い、Compiler で未対応として通知する。

## Pass の Input Layout と SubMesh 描画

**PSO の Input Layout は Pass が要求する Stream から決まる。** 各 Stream のセマンティクス、フォーマット、スロット、オフセットは Graphics 側の固定規則とする。Pass は `VertexStream` の一覧を重複なしで保持する。

| VS の入力 | Pass が要求する Stream | PSO に追加する入力要素 |
| --- | --- | --- |
| `POSITION` | `Position` | `POSITION` |
| `NORMAL`、`TANGENT`、または両方 | `NormalTangent` | `NORMAL` と `TANGENT` |
| `TEXCOORD0` | `PrimaryTexCoord` | `TEXCOORD0` |
| `COLOR0` | `Color` | `COLOR0` |

スロットは Mesh と共通の番号を使う。例えば VS が `POSITION` と `NORMAL` を要求する場合、Input Layout には `POSITION`、`NORMAL`、`TANGENT` が入り、UV と Color の要素は入らない。描画する Mesh の Stream 構成は PSO の選択に影響しない。必要な Stream が Mesh にない場合は同じスロットへ共通の既定バッファをバインドする。法線と接線のうち片方だけを Mesh に設定する場合、同じ頂点構造体のもう片方には既定値を使う。

描画提出は Mesh と MaterialSlot 順の Material の非所有ポインタ、World行列、インスタンス色を渡す。SubMesh の範囲と Slot から Material を選び、Material が参照する Shader の Pass を描画する。SubMesh が未指定の Mesh は全 Index を Slot 0 に割り当てる。

## 描画順と GPU 資源

- Forward は Opaque → Masked → Transparent の順。Opaque・Maskedは描画状態でソートし、Transparentは提出順を保つ。深度・ブレンド・ラスタライザの状態にはShader Passの指定を反映する。 [RenderPassの設計](./RenderPass.md)
- Shader直下の `[InstancedStruct]` はインスタンス描画を指定する。初版のレイアウトはWorld行列と色。バッチはPSO、Material、Mesh、SubMeshを共有し、個別データをInstance Bufferに並べる。
- PSO は登録済み Shader の Handle と Pass 番号を基に再利用する。Mesh や個々の Material 値が変わっても同じ Pass の PSO を使う。
- Draw ごとに Material の定数と Texture をバインドする。フレームが参照中のアップロード領域と Descriptor は、フェンス完了後に再利用する。
- GBuffer 描画、LightingShader、透明物の距離ソート、個別アンロードと GPU Cache の連動は後続の設計対象。

設計の出典は [Material](../../Reference/HestiaDesign/DevelopNotes/Class/Material.md)、[MaterialShader](../../Reference/HestiaDesign/DevelopNotes/Class/MaterialShader.md)、[Mesh と VertexStream](../../Reference/HestiaDesign/DevelopNotes/Class/AssetType.md)、[MaterialShaderDSL](../../Reference/HestiaDesign/Graphics/MaterialShaderDSL.md)。ここで採用した Stream 単位の Input Layout は、参考資料の入力要素を個別に保存する案から更新している。
