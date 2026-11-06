# Asset Management
アセット名から型付きAssetをロードする仕組みの草案。登録情報の形式や編集・検証ルールは [[AssetDB]]、Mesh / Textureのメモリ上の形は [[AssetType]] を参照する。


## Public Facadeと内部Subsystem
Game.dllはAssetsの型付きC++ APIを使い、その実装をGameAPI.libへリンクする。Engine::AssetDBは登録Map / Cache / Loader接続を持つSubsystem実体であり、中継Serviceではない。Engine内部はその実体を直接使う。Public AssetはSTLを含んでもよく、所有入力の限定変換だけをAPI lib内へ隠す。GetはEngine所有の借用Pointer、Register(T&&)は入力を消費しEngineが登録結果を所有する。具体的な内部View / コピー案とLifetimeは[[FacadeBoundarySketch]]・[[AssetSceneSketch]]を参照する。

## 基本方針
- 利用側はファイルパスではなく、`Assets::Load<T>(AssetName)` で名前を指定する。
- 戻り値は`AssetHandle<T>`とする。要求した型をHandleにも保持する。
- AssetDBが共通の名前解決・型確認・Cache検索を行い、未ロードの場合だけ型別Loaderへ処理を渡す。
- LoaderはMesh、Textureなど型ごとに特殊化し、その形式の読み込みとAsset生成を担当する。
- 手動作成したAssetは `Register<T>(AssetName, Asset)` でCacheへ登録し、Loaderを経ずに同じGPUアップロード経路へ渡す。詳細は [[AssetType]] を参照する。
- 登録情報とロード済みAssetは別に管理する。起動時に全Asset本体をロードしない。

## 全体像
`Assets::Load<T>` の処理は次の順序を想定する。

~~~text
AssetName
→ 名前を正規化
→ T用のCache Mapを検索
→ Cache Hitなら既存のHandleを返す
→ Cache MissならT用の登録Mapでパス・登録Typeを確認
→ 管理レコードを作り、AssetLoader<T>でロード
→ 利用側へHandleを返す
~~~

登録MapとCache Mapは型ごとに分ける。たとえばMeshの要求では、Mesh用の登録情報とMesh用のCacheだけを調べる。

~~~text
登録情報                     ロード済みAsset
Mesh名    → ファイルパス     Mesh名    → Mesh
Texture名 → ファイルパス     Texture名 → Texture
Material名→ ファイルパス     Material名→ Material
~~~

名前の正規化、全体での重複検査、手動Reload後のCacheの扱いは [[AssetDB]] にまとめる。

## ModelとMesh / Prefab
`Model`はAssetDBのTypeとして設けない。`Mesh`は描画用の形状データ、`Prefab`はEntityの親子関係とComponentの生成情報を担う。モデルのノード階層はPrefabのEntity階層へ展開する。

~~~text
Character（Prefabのルート）
├─ Body：Mesh HandleとMaterial Handleを持つ描画Component
├─ Head：別のMesh HandleとMaterial Handleを持つ描画Component
└─ Armature
   └─ Spine → Neck → HeadBone：Boneとして使うTransform
~~~

BodyとHeadがスキニングされる場合、各描画Componentは同じArmature内のBoneを参照する。描画EntityとBone EntityはHierarchy上で親子になっている必要はない。Boneごとに専用Componentを付けるか、Skin情報をどのAssetへ保持するかは未確定。

## 利用側のAPI
呼び出し側には、要求する型とAsset名を渡す形を想定する。

~~~cpp
auto mesh = Assets::Load<Mesh>("PlayerMesh");
AssetHandle<Material> material = Assets::Load<Material>("PlayerFaceMaterial");
~~~

`auto mesh`の型は`AssetHandle<Mesh>`になる。`T`は呼び出し時に指定する。ファイルを使わない `Register<T>` もHandleを返し、登録済みの名前は `Load<T>` で同じHandleへ解決する。ロード失敗の通知方法は未確定。

## 型別Loader
名前解決やCache検索は共通の `Load<T>` に置き、ファイル形式ごとに異なる読み込み処理は `AssetLoader<T>` の特殊化へ分ける。

~~~cpp
// 概念スケッチ。型、引数、戻り値は未確定。
template<>
struct AssetLoader<Mesh>
{
    static auto Load(/* 登録されたパス */);
};
~~~

MeshLoaderはMeshのファイルを読み、Mesh Assetを作る。TextureやMaterialも同様に、それぞれの形式に応じたLoaderが生成を担当する。通常のロード経路でAsset本体を `void*` にして扱う必要はない。

## Materialが参照するAsset
MaterialはTexture本体を直接保持せず、`AssetHandle<Texture>`を保持する。Materialのロード時に記録されたTexture名で`Assets::Load<Texture>`を呼び、そのHandleをMaterialへ保存する。

~~~text
MaterialをLoad
→ Material内のTexture名からTextureをLoad
→ 返ったTexture HandleをMaterialが保持
→ 描画時にGraphics内部でHandleからGPU上のTextureを解決
~~~

`Get`の戻り値や、描画中の参照寿命は未確定。Handleからの解決に毎回名前検索を使う必要はない。

## 非同期ロードのState
非同期ロードを導入する場合、`Load<T>`はロード完了前にHandleを返す。同じAssetへの再要求では、処理を重複して起動せず既存のHandleを返す。

StateはAsset本体ではなく、Handleに対応する管理レコードへ置く。ロード中はAsset本体がまだ存在しないため。初期の候補は`Loading`、`Ready`、`Failed`で、未要求のAssetはCacheにレコードがない状態とする。`Get`で利用できるのは`Ready`のAssetに限る。

Material自身の`Ready`をTextureの`Ready`まで待たせるか、Textureが未完了の間は描画側で代替Textureを使うかは未確定。

## CacheとReload
CacheはHandleに対応する管理レコードとロード済みAssetの保持場所で、AssetDBの登録Mapとは別にする。DBの手動ReloadではCacheを維持する。既にCacheにある名前は既存Handleを返し、Cache Missになった名前はReload後の登録パスからロードする。

Assetの破棄時期、Cacheからの退避、Handleの寿命は未確定。詳細は必要になった段階で決める。

## 未確定事項
- 共通の `Load<T>` から型別の登録Map / Cache Mapを選ぶ具体的な仕組み。
- AssetsはPublic static Facade、Engine::AssetDBはCacheを持つSubsystem。既知型のtemplate明示実体化とexport配置はHeaderレビューで決める。
- `AssetHandle<T>`が参照する実体の所有者と、Handleの無効化・破棄条件。
- Loaderの失敗結果とログ・エラー伝達方法。
- 初期実装を同期 / 非同期のどちらにするか。非同期にする場合のジョブ実行と完了通知。
- LoaderからGraphicsへのアップロード要求の具体的なAPIと、NativeRenderスレッドとの同期方法。
- Materialなどが別Assetをロードする際の循環参照検出と失敗の伝播。
- SkinのJoint対応・Inverse Bind Matrixの保持場所と、Prefab生成時のBone参照の設定方法。
- `Failed`になったHandleの再試行条件と、DBの手動Reload後の扱い。
- Renderスレッドからの`Get`と、描画中のAsset寿命の保証。

## 関連資料
- [[AssetDB]]
- [[AssetType]]
- [[HestiaArchitecture]]
