# Asset System

AssetSystemはAssetDBと型別Loader・Storageを所有し、ファイル由来のAssetと実行時に生成したAssetを型付きHandleで扱う。

[AssetDBの設計](Asset/AssetDB.md) ・ [MeshLoaderの設計](Asset/MeshLoader.md) ・ [現行のAsset実装仕様](../Implementation/Asset/README.md)

## 責務と所有

| 要素 | 責務 |
| --- | --- |
| `AssetDB` | 元ファイルをGUIDやAssetルートからの相対パスで検索する |
| `AssetSystem` | 参照を解決してLoaderとStorageをつなぎ、Assetの取得・登録・保存を受け付ける |
| `AssetStorage<T>` | CPU Assetを所有し、ロード済みの実体をIndexとGenerationで解決する |
| `GraphicsSystem` | Graphics側のGPU資源を管理し、CPU Assetと同じ型付きHandleで対応させる |

AssetSystemは初期化時にAssetルートをAssetDBへ渡す。ファイル由来のAssetは `Load<T>(path)` または `Load<T>(AssetRef)` で取得する。どちらも同じ `AssetRef` に解決し、キャッシュにあれば同じHandleを返す。キャッシュになければ元ファイルから読み込む。個別Unloadは後続作業とする。

## YAML Asset の保存

Material、Scene、Prefab の保存は `AssetSystem::Save` に揃える。保存対象の型に応じたオーバーロードで受け、Scene や Prefab では呼び出し側が取得したスナップショットを渡す。以後追加する YAML Asset も `Save` のオーバーロードと `Serializer<T>` の特殊化を用意し、書き出しと登録を共用する。

| 対象 | `Save` に渡すデータ | 元ファイルのパス |
| --- | --- | --- |
| [Material](Asset/MaterialSerialization.md#書き出し) | `AssetHandle<Material>` と任意の保存先 | Handle の `AssetRef` が単独の `.material` を指す場合に AssetDB で解決する |
| [Scene](Serialize/SceneSerialization.md#worldからの保存) | 複数ルートの `SerializedEntity` と任意の保存先 | 現在の Scene に上書きする場合は `SceneSystem` が解決して渡す |
| [Prefab](Serialize/ComponentSerialization.md#スナップショットの取得と保存) | ルートの `SerializedEntity`、Prefab 名、任意の保存先 | 元の Prefab を編集する呼び出し側が解決して渡す |

1. `AssetSystem::Save` がデータと保存先を確定する。Asset ルートからの相対パスを使い、対象型の拡張子と照合する。
2. `Save` のオーバーロードが対応する `Serializer<T>` の特殊化を呼ぶ。Serializer は型付きの入力を検証し、ファイル形式に沿った `std::optional<ryml::Tree>` を返す。Scene と Prefab は Entity 階層をツリーに変換する処理も共有する。
3. `YamlWriter` が完成したツリーを BOM なし UTF-8 で一時ファイルに書き、書き込み完了後に保存先へ反映する。
4. `AssetSystem` が `AssetDB::RegisterFile()` を呼び、既存の `.meta` を引き継ぐか、新規に用意する。新規保存は索引への登録後に同じ起動中から参照できる。戻り値は `std::optional<std::string>` とし、成功時は確定した相対パス、失敗時はログに理由を出して `std::nullopt` を返す。ファイル反映後の登録失敗では、ファイルが書き出されたことも報告する。

保存先は次の順で選ぶ。

| 条件 | 保存先 |
| --- | --- |
| パスが指定された | 指定先に保存する。対象型の上書き条件を満たす既存ファイルでは、保存先の GUID を維持する |
| パスは空で、元ファイルを解決できる | 元ファイルへ上書きし、GUID を維持する |
| パスは空で、元ファイルを持たない | `Assets/` 直下に新規作成し、新しい GUID を発行する |

自動命名では、ファイル名に使える名前があればそれを使う。使える名前がなければ `NewMaterial.material`、`NewScene.scene`、`NewPrefab.prefab` のような型別の名前を使う。既存のファイルや隣接する `.meta` と重なる候補には連番を付ける。呼び出し側は返された相対パスを保持し、以降の保存先に使える。

## 組み込みMesh

組み込みMeshは、それぞれに一度発行してコードに固定したGUIDと `localId: 0` を持つ。名前指定の `Load<Mesh>` は名前を固定の `AssetRef` に解決する。永続参照からのロードと同じ経路を使い、初回に生成して型別Storageへ登録する。以後はどちらの指定でも同じHandleを返す。次の名前は組み込みMesh用に予約し、手動登録では拒否する。

| 名前 | 形状 |
| --- | --- |
| `Cube` | 各辺1 mの立方体 |
| `Plane` | 10 m四方、1 m間隔の11×11頂点 |
| `Quad` | 1 m四方、4頂点・2三角形 |
| `Sphere` | 半径1 mのUV Sphere、経度16・緯度8分割 |

生成関数は形状ごとに分け、位置・Indexと任意Streamを通常のMesh登録検証へ通す。Mesh登録後はGraphicsへGPU Uploadを依頼する。Uploadが失敗した場合はログを出してCPU AssetとHandleを保持し、GPU Meshを解決できない描画要求はGraphics側で処理する。

Scene・Prefabの `MeshRenderer` は既存の `AssetRef` 形式で組み込みMeshを保存する。GUIDは形状の修正後も維持する。名前、GUID、生成関数はAssetSystem内の同じテーブルに置く。 [組み込みMeshの定義](../../Engine/Source/Asset/AssetSystem.cpp)
