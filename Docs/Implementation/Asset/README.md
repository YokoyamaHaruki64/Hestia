# Assetの処理・構造

[全体索引](../README.md) / [型](Types.md)

根拠: [AssetSystem.cpp](../../../Engine/Source/Asset/AssetSystem.cpp)、[AssetLoader.cpp](../../../Engine/Source/Asset/AssetLoader.cpp)、[AssetDB.cpp](../../../Engine/Source/Asset/AssetDB.cpp)、[AssetType.cpp](../../../Engine/Source/Asset/AssetType.cpp)

## 所有と名前解決

AssetSystemはAssetDB、Mesh/Texture別のLoaderとStorageを所有し、GraphicsSystemを借用する。Graphicsが未初期化でも非nullならCPU Cacheは利用できるが、GPU転送は成功しない。現在は単一instance、対応型はMeshとTexture。

初期DBはTextureTestと `../Assets/Textures/texture_test_2048_rgba.png` の対応をコードで登録する。DBファイルの走査や自動importは行わない。名前はASCII英字の大小を区別せず正規化する。Loadは型別Cache、DBの有無と型の照合、Loaderの順で進み、同名の再ロードでは既存Handleを使う。

## 登録と転送

MeshのRegisterは生ポインタの所有権を呼び出し時点で受け取り、unique_ptrで管理する。失敗時も所有権を消費する。空名、衝突、不正Meshなどを拒否し、CPU登録後にGPUへ転送する。GPU転送失敗でもCPU AssetとHandleを保持する。Textureの手動Registerは拒否する。MeshのファイルLoadは未対応で無効Handleを返す。

Texture LoaderはDirectXTexのWIC読込で単一2D画像をデコードする。配列・複数Mip等を拒否し、RGBA8 UNORMまたはsRGBへ変換する。CPU Textureを先に登録し、同じHandleでGraphicsへ画素を転送する。画素はCPU Assetに恒久保持せず、Loader内の一時画像からUpload Resourceへコピーする。

## 検証と寿命

Meshは位置とindexが必須。任意属性は空または位置と同数とし、index範囲と各Bufferのuint32サイズ内への収まりを検査する。三角形の幾何学的品質までは検査しない。TextureDescは正の寸法、1Mip、対応する二つのformatを条件とする。

Getの戻り値は非所有ポインタで、AssetSystem.Finalizeまで有効。個別unloadやslot再利用を公開する実装はない。FinalizeはCPU側のStorage/DB/Loaderを解放し、GraphicsのGPU StorageはGraphics自身が終了時に解放する。有効なCPU HandleだけではGPU登録を保証しない。
