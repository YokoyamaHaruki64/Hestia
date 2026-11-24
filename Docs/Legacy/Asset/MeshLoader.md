# MeshLoaderの設計

[AssetSystem](../AssetSystem.md) ・ [Assetの実装仕様](../../Implementation/Asset/README.md)

ファイルを単一のMeshへ変換する旧設計。階層付きモデルへの移行先は [MeshLoaderの設計](../../Design/Asset/MeshLoader.md)。

## 入口と対象

AssetDBに `type: Mesh` と `path` を登録し、`Assets::Load<Mesh>(name)` で静的Meshを取得する。初版の形式はFBX、OBJ、glTF 2.0（`.gltf`、`.glb`）。相対Pathは既存のAssetと同じく実行時の作業ディレクトリを起点とし、OBJのMTLやglTFの外部BufferはAssimpのファイル読込に委ねる。

```yaml
SampleMesh:
  type: Mesh
  path: ../Assets/Meshes/Sample.glb
```

Assimp 6.0.5のC APIをLoader内部で使い、変換後のCPU MeshをAssetSystemへ渡す。登録後は既存のGPU Uploadと名前Cacheを使う。MaterialやTextureの生成、スキニング、アニメーション、モーフは後続とする。

## 座標と頂点属性

Assimpで三角形化し、左手系・面の向き・UVの原点を描画規約に合わせる。FBXはAssimpのファイル単位変換でメートルに揃え、glTFはメートル、OBJは1単位を1 mとする。ノードの累積変換を頂点へ適用し、複数ノードから参照されるMeshも各配置を反映する。 [AssimpのFBX単位処理](https://github.com/assimp/assimp/blob/v6.0.5/code/AssetLib/FBX/FBXImporter.cpp) ・ [ノード変換処理](https://github.com/assimp/assimp/blob/v6.0.5/code/PostProcessing/PretransformVertices.cpp)

位置を必須とし、法線、接線、UV0、頂点色0があれば対応するStreamへ格納する。全体に存在しない任意Streamは空にする。一部のメッシュにだけ存在する場合は欠損頂点を既存の既定値で埋める。接線の第4成分には法線・接線・従法線の向きから求めた符号を入れる。

## SubMeshとMaterialSlot

1ファイルを1つのMeshとして登録する。変換後の各メッシュのIndexに頂点オフセットを加え、共通Index列の連続範囲をSubMeshにする。使用する元Materialを番号順にSlot 0から詰め、同じ元Materialを参照するSubMeshは同じSlotを共有する。描画時は各Slotに既存のMaterialを設定する。

## エラー処理

Assimpの戻り値で読込結果を判定し、失敗時はエラー文字列を取得する。対応外の拡張子、不完全または空のScene、三角形化後に残る三角形以外の面、不正なIndex、Buffer上限の超過は、Asset名・Path・理由をエラーログに出して無効Handleを返す。CPU登録は全データの変換を終えてから行う。GPU Uploadだけが失敗した場合は、既存契約どおりログを出してCPU MeshとHandleを保持する。
