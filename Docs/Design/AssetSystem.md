# Asset System

AssetSystemはAssetDBと型別Loader・Storageを所有し、ファイル由来のAssetと実行時に生成したAssetを型付きHandleで扱う。

[AssetDBの設計](Asset/AssetDB.md) ・ [MeshLoaderの設計](Asset/MeshLoader.md) ・ [現行のAsset実装仕様](../Implementation/Asset/README.md)

## 責務と所有

| 要素 | 責務 |
| --- | --- |
| `AssetDB` | 元ファイルをGUIDやAssetルートからの相対パスで検索する |
| `AssetSystem` | 参照を解決してLoaderとStorageをつなぎ、Assetの取得・登録を受け付ける |
| `AssetStorage<T>` | CPU Assetを所有し、ロード済みの実体をIndexとGenerationで解決する |
| `GraphicsSystem` | Graphics側のGPU資源を管理し、CPU Assetと同じ型付きHandleで対応させる |

AssetSystemは初期化時にAssetルートをAssetDBへ渡す。ファイル由来のAssetは `Load<T>(path)` または `Load<T>(AssetRef)` で取得する。どちらも同じ `AssetRef` に解決し、キャッシュにあれば同じHandleを返す。キャッシュになければ元ファイルから読み込む。個別Unloadは後続作業とする。

## 組み込みMesh

組み込みMeshは最初の `Load<Mesh>` 時に生成する。生成後は型別Storageの名前索引から同じHandleを返す。次の名前は組み込みMesh用に予約し、手動登録では拒否する。

| 名前 | 形状 |
| --- | --- |
| `Cube` | 各辺1 mの立方体 |
| `Plane` | 10 m四方、1 m間隔の11×11頂点 |
| `Quad` | 1 m四方、4頂点・2三角形 |
| `Sphere` | 半径1 mのUV Sphere、経度16・緯度8分割 |

生成関数は形状ごとに分け、位置・Indexと任意Streamを通常のMesh登録検証へ通す。Mesh登録後はGraphicsへGPU Uploadを依頼する。Uploadが失敗した場合はログを出してCPU AssetとHandleを保持し、GPU Meshを解決できない描画要求はGraphics側で処理する。
