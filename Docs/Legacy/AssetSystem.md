# Asset System

名前をキーにした `AssetDB.yaml` 方式の旧設計。採用した移行先は [AssetDBの設計](../Design/Asset/AssetDB.md)。

2026-11-19に採用したAsset名解決、CPU Asset所有、組み込みMesh生成の設計。現在の型と処理は[Assetの実装仕様](../Implementation/Asset/README.md)を参照する。

## 責務と所有

| 要素 | 責務 |
| --- | --- |
| `AssetDB` | YAMLからAsset名・型・Pathを読み、名前から登録情報を検索する |
| `AssetSystem` | Cache、DB、型別Loaderを通じてCPU Assetを登録・取得する |
| `AssetStorage<T>` | CPU Assetを所有し、名前と型付きHandleで検索する |
| `GraphicsSystem` | Mesh・TextureのGPU実体を所有側へ登録し、CPU Assetと同じHandleで対応させる |

Asset名はASCII英字の大小を区別しない。AssetDBと型別Storageを通じて名前を一意に保つ。Mesh・Texture・MaterialShader・MaterialのCPU実体はAssetSystemが所有し、GPU実体はGraphics側が所有する。

## AssetDBとロード

AssetDBはYAMLのAsset名をキーとし、各項目の`type`と`path`を保持する。ロード時にYAMLを検索用`unordered_map`へ読み込み、Assetの実体やLoaderは保持しない。Path文字列は登録値のまま扱う。

```yaml
AssetName:
  type: Texture
  path: ../Assets/Textures/image.png
```

`AssetSystem::Load<T>`は次の順に処理する。

1. 型別Cacheを検索し、登録済みなら同じHandleを返す。
2. Meshの予約名なら、対応する生成関数でCPU Meshを作ってCacheへ登録する。
3. それ以外はAssetDBで名前・型・Pathを解決し、型別Loaderへ渡す。

AssetDBに予約名が含まれる場合は初期化を失敗させる。手動`Register`でも予約名を拒否し、組み込み生成Meshだけがその名前を使う。外部Meshの形式と変換は[MeshLoaderの設計](Asset/MeshLoader.md)に定める。

## 組み込みMesh

組み込みMeshは外部ファイルやAssetDB項目を持たず、最初の`Load<Mesh>`時に生成する。成功後は通常のAssetStorageに保持し、以降のロードではCacheのHandleを返す。

| 名前 | 形状 |
| --- | --- |
| `Cube` | 各辺1 mの立方体 |
| `Plane` | 10 m四方、1 m間隔の11×11頂点 |
| `Quad` | 1 m四方、4頂点・2三角形 |
| `Sphere` | 半径1 mのUV Sphere、経度16・緯度8分割 |

生成関数は形状ごとに分ける。Meshは位置・Indexを必須とし、生成した法線・接線・UVを含めて通常のMesh登録検証へ通す。Mesh登録後はGraphicsへGPU Uploadを依頼する。Uploadに失敗してもCPU AssetとHandleは保持し、描画時にGPU Meshを解決できない要求はGraphics側で処理する。

## 初期化と拡張

AssetSystem初期化時にAssetDBを読み込む。現在のAssetSystemは`../Assets/AssetDB.yaml`を指定し、AssetDB単体の既定Pathは`Assets/AssetDB.yaml`とする。将来ProjectSettingsからPathを渡せるよう、AssetDBは初期化引数でPathを受け取る。

AssetDBの項目追加・Path変更はYAMLで行う。Assetの個別Unloadは後続作業とする。
