# Assetの型

[処理・構造](README.md)

| 型 | データ・役割 | 定義 |
| --- | --- | --- |
| `AssetHandle<T>` | 型、index、generationによる非所有識別子。IsValidは無効indexとの比較で、Storage内の実在確認ではない | [AssetHandle.h](../../../Engine/Include/Asset/AssetHandle.h) |
| `AssetType` | Mesh / Texture / MaterialShaderのDB分類 | [AssetType.h](../../../Engine/Include/Asset/AssetType.h) |
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
| `AssetInfo` | DBに登録する種別とファイルpath | [AssetDB.h](../../../Engine/Include/Asset/AssetDB.h) |
| `AssetDB` / `EntryMap` | 正規化した名前とAssetInfoの索引。空名・空path・未対応型・正規化後衝突を拒否 | 同上 |
| `AssetStorage<T>` | 名前索引とCPU実体の所有者。Getでindexとgenerationを照合 | [AssetStorage.h](../../../Engine/Include/Asset/AssetStorage.h) |
| `AssetStorage<T>::Slot` | unique_ptrとgenerationを組にして保持する内部要素 | 同上 |
| `AssetLoader<Mesh>` | ファイル読込の境界。現在は未対応をログ出力する | [AssetLoader.h](../../../Engine/Include/Asset/AssetLoader.h) |
| `AssetLoader<Texture>` | Graphicsを借用し、WICデコード・変換・CPU登録・GPU転送を担当 | 同上 |
| `AssetLoader<MaterialShader>` | .matshを読み込み、Parser・Compilerを経てTextureを解決し登録 | [MaterialShaderLoader.cpp](../../../Engine/Source/Asset/MaterialShaderLoader.cpp) |
| `MaterialShader` / `Material` | Shaderが共通のProperty定義とPassを持ち、Materialが型付きの値を持つ | [MaterialShaderの実装仕様](../Graphics/MaterialShader.md) |
| `AssetSystem` | DB、Loader、Storageを所有する統括。static入口を現在instanceへ接続する | [AssetSystem.h](../../../Engine/Include/Asset/AssetSystem.h) |
| `AssetAPI` | AssetSystem の操作を Asset 型ごとの関数ポインタとして公開する DLL API。未接続時のポインタは `nullptr` | [AssetAPI.h](../../../Engine/Include/API/AssetAPI.h) |
| `HestiaGame::Assets` | 未接続なら `Load`／`Register` は無効 Handle、`Get` は `nullptr`。`Register` は失敗時も所有権を受け取る | [GameAssets.h](../../../Game/Include/GameAssets.h) |
