# AssetDBの設計

Assetの永続参照に使うGUIDを元ファイルの隣の `.meta` に保存する。AssetDBは起動時に `Assets/` から検索用の索引を組み立てる。コードからは相対パスでロードでき、PrefabやSceneはファイルのGUIDとファイル内IDでAssetを参照する。

[AssetSystemの設計](../AssetSystem.md) ・ [MeshLoaderの設計](MeshLoader.md) ・ [現行のAsset実装仕様](../../Implementation/Asset/README.md) ・ [旧AssetDB方式](../../Legacy/AssetSystem.md)

## .metaの形式

元ファイル `Assets/Textures/Icon.png` に対応するメタデータは `Assets/Textures/Icon.png.meta` に置く。元ファイルのパスは `.meta` の位置から末尾の `.meta` を外して求める。元ファイルと `.meta` はセットでバージョン管理する。
以下は最小構成。

```yaml
type: Texture
guid: 2db48ec9-7a6f-49a2-8c79-73639d0a8572
```

| 項目     | 意味                                          |
| ------ | ------------------------------------------- |
| `type` | 元ファイルの型。対応拡張子の表から決める                        |
| `guid` | 新規登録時に発行する128bitの永続ID。YAMLではハイフン付きの文字列で保存する |

対応拡張子は `AssetDB.cpp` に `static const std::unordered_map<std::string_view, AssetType>` で定義する。小文字に揃えた拡張子を、生成時の型決定と既存ファイルの型検証で共用する。`.png` は `Texture`、`.matsh` は `MaterialShader`、`.prefab` は `Prefab` とする。`.fbx`、`.obj`、`.gltf`、`.glb` は `Model` として扱い、Importerが主Assetの `Prefab` を生成する。 [Prefabの読み込み](../Serialize/ComponentSerialization.md#読み込み)

## 起動時の索引作成

`AssetDB::Initialize(assetsRoot)` は指定されたAssetルートを再帰的に走査する。対応拡張子の元ファイルに `.meta` がなければGUIDを発行し、書き込み成功後に登録する。拡張子表にないファイルは自動登録の対象外とする。

| 状態                                       | 処理                                     |
| ---------------------------------------- | -------------------------------------- |
| 既存の `.meta` のYAML、必須項目、GUID形式、拡張子と型の組が不正 | パスと理由をエラーログに出し、初期化を失敗させる。既存ファイルを保持する   |
| 登録するGUIDが重複                              | 両方のパスをエラーログに出し、初期化を失敗させる               |
| 元ファイルがない `.meta`                         | 想定される元ファイルのパスを警告に出し、索引から外す。`.meta` は残す |
| 走査や `.meta` の書き込みに失敗                     | パスと理由をエラーログに出し、初期化を失敗させる               |

`AssetInfo` は `AssetGuid`、`AssetType`、Assetルート起点の相対パスを持つ。DBは次の2つの索引をメモリ上に作る。モデルファイルも1ファイルにつき1件とする。

- `GUID → AssetInfo` を `FindByGuid(guid)` で検索する。
- `正規化した相対パス → GUID` を `FindByPath(path)` で検索し、同じ `AssetInfo` を返す。

初版の変更反映は再起動で行う。起動時に索引を再構築し、各Assetは初回ロード時に元ファイルから読み込む。ディスク上に索引を置く場合は、元ファイルと `.meta` から再生成できるキャッシュとして扱う。

## Asset参照とクラスの責務

| 要素                         | 責務                                                                                            |
| -------------------------- | --------------------------------------------------------------------------------------------- |
| `AssetGuid`                | GUIDを保持する値型。YAMLの入出力時に文字列へ変換する                                                                |
| `AssetRef`                 | `{guid, localId}` を持つ永続参照。主Assetは `localId: 0`、SubAssetは正の整数で指定する                             |
| `AssetSystem` / `AssetAPI` | `Load<T>(path)` と `Load<T>(AssetRef)` を提供する。パス指定は主Assetの `AssetRef` に解決し、同じロード経路で型付きHandleを返す |
| 型別Loader / Model Importer  | DBで解決した元ファイルを読み、生成したCPU Assetを `AssetRef` で登録する。Model ImporterはPrefab・個別Mesh・Materialを生成する |
| `AssetStorage<T>`          | ファイル由来のAssetは `AssetRef → AssetHandle<T>` でキャッシュする。組み込みMeshと手動登録には名前の索引を使う                    |
| Prefab / Scene             | 保存時はロード済みHandleに対応する `AssetRef` を記録し、復元時に型付きHandleへ解決する。保存用の対応はStorageのスロットから取得する             |
| Graphics                   | `AssetHandle<T>` のIndexを添字としてGPU資源を取得し、Generationを照合する                                        |

コードからのロード例は `Assets::Load<Texture>("Textures/Icon.png")`。PrefabやSceneのAsset参照は `{ guid: 2db48ec9-7a6f-49a2-8c79-73639d0a8572, localId: 0 }` のように保存する。組み込みMeshを永続参照するためのID割り当ては、Prefab・Sceneの保存で扱う段階に決める。

## モデルのSubAsset ID

モデルは主AssetとしてPrefabを持ち、個別Meshと使用するMaterialをSubAssetとして保持する。Importerは変換成功時に、各SubAssetのキーとIDの対応、および次に発行するIDを `.meta` に保存する。初回の割り当ては `1` から始め、同じモデル内の各型で採番を共有する。
以下はモデルファイルを読みこんだ際の例。

```yaml
type: Model
guid: 1c6df791-0fd5-4ddf-9c55-384c61557352
nextLocalId: 4
subAssets:
  "Chair/Left/Leg": 1
  "Chair/Right/Leg": 2
materials:
  "name:Wood": 3
```

- 同じノードパスは既存の `localId` を引き継ぐ。Materialは一意な名前を `name:` キーで引き継ぎ、名前が空または重複している場合は `index:` キーでAssimpのMaterial番号を使う。この場合、番号が変わる再出力では対応が変わり得る。新しいキーには `nextLocalId` を割り当て、次の値へ進める。
- 削除したノードの対応は取り除き、発行済みIDは再利用しない。既存のメタデータでは各IDの一意性・正値と、`nextLocalId` が割り当て済みIDより大きいことを検証する。
- MeshとMaterialの対応が同じ場合は `.meta` を書き直さない。頂点やTransformだけの変更、ノードの並べ替えはMeshのIDに影響しない。
- モデルの初回ロードで元ファイルから変換し、同じ起動中の再ロードではメモリのキャッシュを使う。将来のファイルキャッシュは `Cache/Assets/<guid>/<localId>.mesh` のような名前にできる。インポート入力が変わったら同じキーでキャッシュの内容を再生成する。

## 移動・改名・更新・置き換え

| 操作            | 手順と結果                                                                                      |
| ------------- | ------------------------------------------------------------------------------------------ |
| 元ファイルを移動・改名する | 対応する `.meta` も一緒に移動・改名する。再起動後は既存GUIDが新しいパスに解決される。コードに指定した相対パスも新しいパスに揃える                    |
| 元ファイルだけを移動する  | 移動先に `.meta` がなければ再起動時に新規GUIDが発行され、旧GUIDは未解決になる。復旧するには移動先の `.meta` を元の `.meta` で上書きし、再起動する |
| 同じモデルを更新する    | 元ファイルを上書きし、`.meta` を引き継ぐ。ノードの改名や親の変更でも以前のIDを使う場合は、再インポート前に `subAssets` の対応先パスを更新する         |
| 別モデルに置き換える    | 元の `.meta` を退避してから元ファイルを差し替える。再起動で新しい `.meta` とGUIDを生成し、参照を付け直す                            |

同じパスやノード名を持つ別モデルもあり得る。更新と置き換えの区別は、作業者が `.meta` を引き継ぐかどうかで指定する。
