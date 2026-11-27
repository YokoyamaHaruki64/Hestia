# AssetメタデータとMetaTool

元Assetに隣接する `.meta` を永続IDの管理元とする。メタデータの形式、GUIDの発行、読み書き、走査はC++で実装し、`Common.lib` に置く。開発用の `MetaTool.exe`、EngineのAssetDB、GameCodeGeneratorは同じ実装を使う。

[AssetDBの設計](AssetDB.md) ・ [C++ Scriptのコード生成](../GameRuntime/CppScript.md)

## ファイル形式

元ファイル名に `.meta` を付ける。たとえば `Assets/Textures/Icon.png` に対して `Assets/Textures/Icon.png.meta` を置く。元ファイルと `.meta` はセットでバージョン管理し、移動や改名でもセットを維持する。

```yaml
version: 1
type: Texture
guid: 2db48ec9-7a6f-49a2-8c79-73639d0a8572
```

| 項目 | 契約 |
| --- | --- |
| `version` | メタデータ形式のバージョン。新規作成時は `1` を書く。既存ファイルで省略されている場合は `1` として読み、未知のバージョンはエラーにする。 |
| `type` | 元ファイルの拡張子に対応するAsset型。既存ファイルの型も対応表と照合する。 |
| `guid` | 元ファイルに割り当てた128bitの永続ID。ハイフン付きの文字列で保存する。 |

モデルの `nextLocalId`、`subAssets`、`materials` も共通の読み書き処理が扱う。これらの値はモデル変換時にImporterが決める。既存のGUIDとSubAsset IDの割り当ては引き継ぐ。 [モデルのSubAsset ID](AssetDB.md#モデルのsubasset-id)

拡張子と型の対応表は `Common.lib` に一つ置く。照合時は拡張子を小文字に揃える。

| 拡張子 | 型 |
| --- | --- |
| `.png` | `Texture` |
| `.matsh` | `MaterialShader` |
| `.material` | `Material` |
| `.fbx`、`.obj`、`.gltf`、`.glb` | `Model` |
| `.prefab` | `Prefab` |
| `.scene` | `Scene` |
| `.cpp` | `Script` |

## 共通処理の責務

- `.meta` のUTF-8 YAMLと型付きデータを相互変換し、形式・バージョン・型・IDを検証する。YAMLの読み書きには既存のRapidYAMLを使う。
- `AssetGuid` の型、発行、文字列との相互変換を所有する。Prefab、Scene、Generatorからも同じGUID表記を扱えるようにする。
- Assetルートと拡張子の絞り込みを受け取り、走査・検証・不足分の作成を行う。成功時に元ファイルのルート相対パス、型、GUID、モデルのID割り当てを呼び出し側に返す。

メタデータ操作の宣言はEngineと開発ツールが使う内部ヘッダーに置く。呼び出し側は `Common.lib` を静的リンクし、型付きデータと診断を受け取る。AssetDBは診断をログに記録し、ツールは標準エラー出力に書く。

## 走査と作成の順序

1. Assetルートを再帰的に走査し、指定された拡張子と対応する `.meta` を集める。絞り込みが空なら、対応表の全拡張子を対象にする。
2. 元ファイルごとに `.meta` の有無を調べる。既存ファイルを読み、YAML、必須項目、型、GUID、およびモデル固有のIDを検証する。走査対象内でGUIDが重複した場合は両方のパスを報告する。
3. 既存ファイルの検証が成功したら、`.meta` が欠けている元ファイルにGUIDを発行し、隣接ファイルを作成する。既存ファイルのGUIDは再利用し、メタデータが不正ならパスと理由を報告して失敗とする。
4. 全件が揃ったら走査結果を返す。元ファイルが存在しない `.meta` は警告を出し、ファイルを保持したまま結果から除く。

走査または書き込みに失敗した場合、診断を返して処理を止める。途中で作成に成功した `.meta` は残し、次回の実行で同じGUIDを使う。

## 呼び出し経路

### MetaTool

リポジトリルートから実行し、`Assets/` を走査する。`--ext` は複数回指定でき、登録されていない拡張子の指定はエラーにする。成功時の終了コードは `0`、失敗時は非ゼロとする。

| コマンド | 対象 |
| --- | --- |
| `MetaTool.exe` | 対応表の全拡張子 |
| `MetaTool.exe --ext .cpp` | `Assets/` 配下の `.cpp` |
| `MetaTool.exe --ext .cpp --ext .scene` | `Assets/` 配下の `.cpp` と `.scene` |

GameビルドではGeneratorが `MetaTool.exe --ext .cpp` を実行してGUIDを確定し、プロジェクトに登録されたScriptの生成に進む。拡張子を絞った場合の重複検査はその走査対象に適用し、Asset全体での重複は全拡張子の走査で検出する。

### AssetDB

Engine起動時は、AssetDBが `Common.lib` の走査を直接呼び、返された一覧からパスとGUIDの索引を構築する。走査とメタデータの読み取りはこの呼び出しで完結する。

モデルを変換した後のSubAsset情報の更新には、共通のメタデータ書き込み処理を使う。Importerが決めた割り当てに変更があれば、元のGUIDを維持して保存する。
