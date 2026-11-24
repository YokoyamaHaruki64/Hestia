# MeshLoaderの設計

Model Importerはモデルのノード階層をPrefabとして保持し、Meshを参照するノードごとに1つのMeshを生成する。ファイルの識別とSubAsset IDの割り当ては [AssetDBの設計](AssetDB.md) に定める。

[AssetSystem](../AssetSystem.md) ・ [現行のAsset実装仕様](../../Implementation/Asset/README.md) ・ [単一Meshに変換する旧設計](../../Legacy/Asset/MeshLoader.md)

## 入力と生成物

FBX、OBJ、glTF 2.0（`.gltf`、`.glb`）をAssimp 6.0.5のC APIで読む。OBJのMTLやglTFの外部Bufferの読み込みはAssimpに委ねる。

- 主Assetは `localId: 0` のPrefabとする。元の親子関係とLocal Transformを保持する。
- ノードが複数の `aiMesh` を参照する場合は、頂点オフセットを加えて1つのMeshへまとめ、各 `aiMesh` の連続したIndex範囲をSubMeshとする。同じ元MaterialのSubMeshは同じSlotを使い、使用する元Materialを元の番号順にSlotへ割り当てる。
- 生成したMeshごとに `localId` と内部名を持つ。内部名はノードパスの `/` を `_` にした名前とする。ノード名が空なら参照する `aiMesh` の名前を候補にする。再インポートの照合は変換前のノードパスを使う。同じ内部名を生成する別パスも、別々のIDで扱う。

主Assetは `Assets::Load<Prefab>("Models/Chair.fbx")`、個別Meshは `Assets::Load<Mesh>(AssetRef{guid, localId})` で取得する。Material・Textureの自動生成と、スキニング・アニメーション・モーフの再生は後続作業とする。

## 座標と頂点属性

三角形化し、左手系・面の向き・UVの原点を既存の描画規約に合わせる。FBXは単位情報に従ってメートルに揃え、glTFはメートル、OBJは1単位を1 mとする。Meshの頂点はノードのローカル座標で保持し、配置はPrefabのLocal Transformで表す。

位置を必須とし、法線、接線、UV0、頂点色0があれば対応するStreamへ格納する。Mesh全体にない任意Streamは空にし、一部の `aiMesh` で欠ける場合は既定値で補う。接線の第4成分には法線・接線・従法線の向きから求めた符号を入れる。

## 失敗時の扱い

読込失敗、対応外形式、不完全または空のScene、不正な面やIndex、Buffer上限超過はファイルパスと理由をエラーログに出す。ノードパスが重複してIDの対応を一意に決められない場合も、対象パスをログに出してインポートを失敗させる。変換が完了した生成物を登録し、SubAssetの対応に差分がある場合は成功後に `.meta` を更新する。GPU Uploadだけが失敗した場合はCPU MeshとHandleを保持する。
