# Assetの処理・構造

[全体索引](../README.md) / [型](Types.md) / [Assetの設計](../../Design/AssetSystem.md) / [MaterialShader と Forward 描画](../Graphics/MaterialShader.md)

[AssetDBの設計](../../Design/Asset/AssetDB.md) ・ [Prefabの保存と展開](../ECS/README.md#prefabの保存と展開)

[AssetSystem.cpp](../../../Engine/Source/Asset/AssetSystem.cpp) ・ [AssetLoader.cpp](../../../Engine/Source/Asset/AssetLoader.cpp) ・ [AssetDB.cpp](../../../Engine/Source/Asset/AssetDB.cpp) ・ [AssetType.h](../../../Engine/Include/Asset/AssetType.h)

## 所有とAsset参照の解決

AssetSystemはAssetDBと型別のLoader・Storageを所有し、GraphicsSystemを借用する。単一インスタンスでMesh、Texture、MaterialShader、Material、Prefabを保持する。Graphicsが未初期化の場合もCPU側は利用できる。

AssetDBは初期化時にAssetルートを再帰的に走査し、対応する元ファイルと隣の `.meta` を突き合わせる。 `.meta` のない元ファイルには型と新しいGUIDを書き出す。対応拡張子は `.png`、`.matsh`、`.prefab`、`.fbx`、`.obj`、`.gltf`、`.glb`。型の不一致やGUIDの重複は初期化エラーになり、本体のない `.meta` は警告を出して索引から外す。変更は再起動時に取り込む。

ファイルのパスはAssetルートからの相対パスで指定する。たとえば `Load<Texture>("Textures/Icon.png")`。パス指定はDBで主Assetの `AssetRef{guid, 0}` に解決する。 `Load<T>(AssetRef)` も同じ型別キャッシュを使い、ロード済みなら同じHandleを返す。ロード後は型付きHandleのIndexとGenerationでCPU・GPU資源を引く。手動登録と組み込みMeshには名前の索引を使う。

組み込みMeshは`Cube`、`Plane`、`Quad`、`Sphere`。形状ごとの生成関数は初回Load時に呼び、通常のMesh登録・GPU Upload経路を通す。寸法・分割数は[Asset設計](../../Design/AssetSystem.md#組み込みmesh)を参照。

## モデルの読み込み

モデルは `type: Model` として登録する。 `Load<Prefab>(path)` はAssimp 6.0.5で読み込み、ノード階層とLocal TRSをPrefabに変換する。Meshを持つノードでは、そのノードが参照する複数の `aiMesh` を1つのMeshにまとめ、SubMeshにMaterialSlotを記録する。同じ `aiMaterial` から生成したDefaultPBRのMaterialを共有し、PrefabのMeshRendererにはMeshと各SlotのMaterialを `AssetRef` として保存する。個別のMeshとMaterialは型付きの `Load<T>(AssetRef)` で取得できる。 [MeshLoader.cpp](../../../Engine/Source/Asset/MeshLoader.cpp)

Importerは `.meta` のノードパスとMaterialキーから既存の `localId` を引き継ぎ、新規SubAssetには共通の `nextLocalId` から発行する。割り当てが変わったときだけ `.meta` を更新する。読み込み・変換に失敗した場合はパスと理由をログに出す。変換後のCPU Meshは通常のGPU Upload経路へ渡す。Assimp DLLとライセンスはEngineのビルド時に実行先へコピーされる。

### モデル由来MaterialとDefaultPBR

Importerは、使用された `mMaterialIndex` ごとに [DefaultPBR.matsh](../../../Assets/Shaders/MatShaders/DefaultPBR.matsh) からMaterialを一つ作る。複数ノードが同じ番号を参照する場合は同じMaterialを借用する。各ノードでは使用する元Materialの番号順でSlotを詰め、Prefabの `MeshRenderer.materials` にSlot順で `AssetRef` を格納する。保存済みPrefabの展開時も、同じ参照は同じロード済みMaterialに解決される。 [MeshLoader.cpp](../../../Engine/Source/Asset/MeshLoader.cpp) ・ [MeshRendererAPI.cpp](../../../Engine/Source/API/ECSProcess/MeshRendererAPI.cpp)

| Property | 値の決め方 |
| --- | --- |
| `_BaseColor` | AssimpのBase Colorを優先し、省略時はDiffuse Colorを使う。両方とも取得できない場合はシェーダーの既定値 |
| `_Metallic`・`_Roughness` | 元データに値があれば使用し、省略時はシェーダーの既定値 |
| `_BaseMap` | シェーダーの既定値を使い、描画時に白Textureに解決する |

DefaultPBRは不透明Forward用で、固定平行光と簡易環境光を使う。既定シェーダーをロードできない場合は警告を出し、PrefabとMeshを登録する。この場合はMaterialSlotの設定が描画前に必要になる。

MaterialのIDは `.meta` の `materials` に記録する。一意な名前は `name:<名前>`、無名・重複名は `index:<Assimpの番号>` で照合する。番号を使うMaterialは、元データの並び順が変わると照合先が変わることがある。

## 登録と転送

MeshのRegisterは生ポインタの所有権を呼び出し時点で受け取り、unique_ptrで管理する。失敗時も所有権を消費する。空名、衝突、不正Meshなどを拒否し、CPU登録後にGPUへ転送する。GPU転送失敗でもCPU AssetとHandleを保持する。Textureの手動Registerは拒否する。

Texture LoaderはDirectXTexのWIC読込で単一2D画像をデコードする。配列・複数Mip等を拒否し、RGBA8 UNORMまたはsRGBへ変換する。CPU Textureを先に登録し、同じHandleでGraphicsへ画素を転送する。画素はCPU Assetに恒久保持せず、Loader内の一時画像からUpload Resourceへコピーする。

MaterialShaderは `.matsh` を読み込んでCPU Assetとして登録する。Materialは、モデルから生成されるか、Shaderを設定して手動登録される。Propertyの配置や両者の寿命については[MaterialShaderの実装仕様](../Graphics/MaterialShader.md)を参照。

## 検証と寿命

Meshは位置とindexが必須。任意属性は空または位置と同数とし、index範囲と各Bufferのuint32サイズ内への収まりを検査する。SubMeshの範囲は共通index列に収まる必要があり、未指定時は全indexをMaterialSlot 0として扱う。TextureDescは正の寸法、1Mip、対応する二つのformatを条件とする。

Getの戻り値は非所有ポインタで、AssetSystem.Finalizeまで有効。個別unloadやslot再利用を公開する実装はない。FinalizeはCPU側のStorage/DB/Loaderを解放し、GraphicsのGPU StorageはGraphics自身が終了時に解放する。有効なCPU HandleだけではGPU登録を保証しない。

### 検証記録

- 2026-11-20にEngine `Debug_Editor|x64` ビルドとAssetDB・AssetStorage・Prefabのテスト6件が成功。 [AssetTests.cpp](../../../Test/Hestia_Tests/Source/AssetTests.cpp)
- 2026-11-25に `Test|x64` で、相対パスによるAssetロードを使う描画統合テスト2件が成功。 [AssetGraphicsTests.cpp](../../../Test/Hestia_Tests/Source/AssetGraphicsTests.cpp)
- 2026-11-25にMaterial SubAsset追加後のEngine `Debug_Editor|x64` ビルドと、AssetDB・Prefabの既存テスト3件が成功した。一時OBJでは、2ノードのMaterial共有と、PrefabをYAMLに保存してAssetSystemを再初期化した後の参照復元を確認した。
- 同日の `Y-Bot.fbx` 検証では182ノード、Mesh 2件、Material 2件を展開した。隠しウィンドウでDX12に55,320三角形を提出し、ピクセルシェーダーの実行を確認した。Debug Layerのエラーは0件。見た目の確認は未実施。
