# Assetの型

[処理・構造](README.md)

| 型 | データ・役割 | 定義 |
| --- | --- | --- |
| `AssetHandle<T>` | 型、index、generationによる非所有識別子。IsValidは無効indexとの比較で、Storage内の実在確認ではない | [AssetHandle.h](../../../Engine/Include/Asset/AssetHandle.h) |
| `AssetGuid` / `AssetRef` | ファイルの永続GUIDと、GUID・ファイル内IDを組み合わせたAsset参照 | [AssetHandle.h](../../../Engine/Include/Asset/AssetHandle.h) |
| `AssetType` | DBの自動登録ではTexture／MaterialShader／Model／Prefabを拡張子から選ぶ | [AssetType.h](../../../Engine/Include/Asset/AssetType.h) |
| `Prefab` | Handle、名前、子を値として持つルートの `SerializedEntity` | 同上 |
| `PositionVertex` | float3の位置 | 同上 |
| `NormalTangentVertex` | float3法線とfloat4接線 | 同上 |
| `PrimaryTexCoordVertex` | float2の第一UV | 同上 |
| `ColorVertex` | float4頂点色 | 同上 |
| `VertexStream` | Position、NormalTangent、PrimaryTexCoord、ColorとCount。CPU/GPU双方のStream順 | 同上 |
| `Mesh` | 自身のHandle、各Stream、uint32のindex列、SubMeshを所有。IsValidで登録条件を確認 | 同上 |
| `SubMesh` | 共通index列内の開始位置・個数とMaterialSlot | 同上 |
| `TextureFormat` | RGBA8Unorm / RGBA8Srgb | 同上 |
| `TextureDesc` | width、height、mipLevels、format。既定寸法0のため既定値は無効 | 同上 |
| `Texture` | HandleとTextureDesc。画素とDX12 Resourceは保持しない | 同上 |
| `AssetInfo` | GUID・種別・相対パスと、モデル内のSubAsset ID割り当て | [AssetDB.h](../../../Engine/Include/Asset/AssetDB.h) |
| `AssetDB` | 元ファイルと `.meta` を走査し、GUIDと正規化した相対パスの索引を作る | 同上 |
| `AssetStorage<T>` | `AssetRef` と名前の索引を持ち、CPU実体を所有する。GetでIndexとGenerationを照合 | [AssetStorage.h](../../../Engine/Include/Asset/AssetStorage.h) |
| `AssetStorage<T>::Slot` | `unique_ptr`、Generation、保存用の `AssetRef` を保持する内部要素 | 同上 |
| `AssetLoader<Prefab>` | モデルからPrefab・個別Mesh・Materialを生成する。 `.prefab` の場合はYAMLからPrefabを構築する | [MeshLoader.cpp](../../../Engine/Source/Asset/MeshLoader.cpp) ・ [PrefabLoader.cpp](../../../Engine/Source/Serialize/PrefabLoader.cpp) |
| `AssetLoader<Texture>` | Graphicsを借用し、WICデコード・変換・CPU登録・GPU転送を担当 | [AssetLoader.cpp](../../../Engine/Source/Asset/AssetLoader.cpp) |
| `AssetLoader<MaterialShader>` | .matshを読み込み、Parser・Compilerを経てTextureを解決し登録 | [MaterialShaderLoader.cpp](../../../Engine/Source/Asset/MaterialShaderLoader.cpp) |
| `MaterialShader` / `Material` | Shaderが共通のProperty定義とPassを持ち、Materialが型付きの値を持つ | [MaterialShaderの実装仕様](../Graphics/MaterialShader.md) |
| `AssetSystem` | DB、Loader、Storageを所有する統括。static入口を現在instanceへ接続する | [AssetSystem.h](../../../Engine/Include/Asset/AssetSystem.h) |
| `AssetAPI` | AssetSystem の操作を Asset 型ごとの関数ポインタとして公開する DLL API。未接続時のポインタは `nullptr` | [AssetAPI.h](../../../Engine/Include/API/AssetAPI.h) |
| `HestiaGame::Assets` | 未接続なら `Load`／`Register` は無効 Handle、`Get` は `nullptr`。`Register` は失敗時も所有権を受け取る | [GameAssets.h](../../../Game/Include/GameAssets.h) |
