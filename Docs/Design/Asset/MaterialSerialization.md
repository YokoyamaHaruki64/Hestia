# Material の保存・読み込み

`.material` を独立した Asset として扱い、参照先の MaterialShader と、Material ごとの Property 値を YAML に保存する。Scene と Prefab は既存の `MeshRenderer.materials` から Material を参照する。

[AssetSystem](../AssetSystem.md) ・ [AssetDB](AssetDB.md) ・ [MaterialShader](../MaterialShader.md) ・ [Scene の保存・読み込み](../Serialize/SceneSerialization.md)

## 既存の Material との関係

モデルから生成した Material は、モデルの GUID と正の `localId` で参照できる。プログラムが名前で登録した Material は、単独のファイルに保存した時点で永続参照を作る。単独 Material の読み込みは既存の AssetSystem のロード経路に加える。 [モデルの Material](MeshLoader.md#materialの共有)

## ファイル形式

`.material` の隣に `.material.meta` を置き、型を `Material`、ファイル本体の `localId` を `0` とする。Material 自身の GUID は既存のメタデータ管理に従う。移動や改名では `.meta` も一緒に移す。 [Asset メタデータ](MetaTool.md)

```yaml
version: 1
shader: { guid: 1aab739d-1d62-4041-b04b-f208997c1df9, localId: 0 }
properties:
  _BaseColor: [0.65, 0.30, 0.12, 1.0]
  _Metallic: 0.2
  _Roughness: 0.45
```

| 項目 | 契約 |
| --- | --- |
| `version` | 必須。初版は `1` |
| `shader` | 必須。MaterialShader の `AssetRef`。空でない GUID と `localId: 0` を指定する |
| `properties` | Property 名をキーとするマップ。省略または空なら Shader の既定値を使う |

型は参照先の `MaterialPropertyDesc` から取得する。保存に使う識別子は Property の宣言名で、Shader 内の順序が変わっても同じ名前で解決する。ファイルに持つのは CPU 側の論理値と Asset 参照。CBuffer、GPU 配置、描画ごとの InstanceColor は対応する実行時データから作る。

| Property 型 | YAML の値 |
| --- | --- |
| `bool` / `int32_t` / `float` | `true`・`false`、32 bit 符号付き整数、有限の数値 |
| `Vector2` / `Vector3` / `Vector4` | 成分順の 2・3・4 要素の配列 |
| `Color` | `[r, g, b, a]`。Material が保持する成分をそのまま使う |
| `Matrix3x3` / `Matrix4x4` | CPU 側の行を順に並べた入れ子の配列。各行は 3・4 要素 |
| Texture | Texture の `{guid, localId: 0}`。明示的な未設定は `null` とし、既存の白 Texture フォールバックを使う |

## 読み込み

1. `AssetDB` と MetaTool の共通拡張子表に `.material` を追加する。相対パスと `AssetRef` の両方を既存の `AssetSystem::Load<Material>()` で受け、同じ `AssetStorage<Material>` を引く。
2. 単独 Material の参照は `AssetLoader<Material>` に渡す。YAML の版と構造を確認し、`AssetSystem::Load<MaterialShader>(ref)` で Shader を解決する。モデルの GUID と正の `localId` は既存のモデルインポート経路で解決する。
3. Shader の Property 定義を使って YAML を型付き値に変換し、Texture 参照も解決する。新しい Material に `SetShader()` で既定値を設定し、指定された値を既存の Setter と同じ経路で適用する。
4. 全項目が検証・適用できたら `RegisterFromLoader()` で登録する。Scene や Prefab から同じ参照を解決すると、同じ Handle と Material 実体を共有する。

未知の版・Property 名、キーの重複、型や要素数の不一致、解決できない Shader・Texture は読み込みエラーとする。ログには `.material` のパスと該当するキーまたは参照を出す。エラー時は無効 Handle を返し、途中の Material は登録しない。Scene 展開中の失敗は既存の復元エラー経路へ伝える。

## 書き出し

`AssetSystem::Save(materialHandle, path)` は Handle から Material を解決し、`Serializer<Material>` が型付きの値から YAML ツリーを作る。Shader は `GetHandle()` から永続参照を引き、Texture は Property の Handle から永続参照を引く。設定済みの Shader・Texture が永続参照を持たない場合は、出力前にエラーにする。書き込みと AssetDB への登録は共通処理に任せる。 [YAML Asset の保存](../AssetSystem.md#yaml-asset-の保存)

初版の推奨は **Shader 既定値との差分だけを書き出す**方式。Shader の宣言順に Property を調べ、既定値と異なる論理値を出力する。これなら現在の Material に上書き済みフラグを増やさず、書き出したファイルでも既定値の更新を継承できる。既定値と同じ値を明示して保存した場合も、再書き出し後は既定値の変更を継承する。Texture の未設定も、既定値に Texture がある場合は `null` として差分に残す。

指定先は、対象 Material 自身の既存ファイルか、新規パスとする。パスを省略した場合は単独の `.material` を AssetDB から解決して上書きする。名前登録した Material やモデルの SubAsset を保存先の指定なしでファイル化するときは、`Assets/` 直下に自動命名する。作成したファイルを以降の編集・上書きに使う場合は、確定したパスからロードし、その Handle を使用する。

## 起動デモへの適用

`Assets/Materials/DebugCube.material` を作り、既存の [Unlit_Inst.matsh](../../../Assets/Shaders/MatShaders/Unlit_Inst.matsh) を GUID で参照する。Property は空でよい。Material 本体と対応する `.meta` は Git 管理対象にする。

`DebugCubeGenerator` は既存の `Assets::LoadMaterial()` に `Materials/DebugCube.material` を渡し、取得した Handle を全 Cube で共有する。これで `Engine::Initialize()` のデモ専用 Material 登録を外せる。参照の失敗は生成 Script が報告し、Cube の生成を中断する。InstanceColor は引き続き Cube ごとのデータとして扱い、最終的な保持先は描画システム側の設計で決める。

## 実装時の確認項目

- パスと GUID の両方で同じ Material が得られ、複数の Renderer が同じ実体を共有する。既存のモデル由来 Material もロードできる。
- 既定値、数値・色・行列、Texture 参照、明示的な未設定が保存と復元で一致する。保存し直しても既存の `.meta` の GUID が維持される。
- 名前登録した Material をパスの指定なしで保存すると、重複しない相対パスが返る。
- 不正な版、未定義の Property、型不一致、Shader・Texture の参照切れで、不完全な Material が登録されない。
- 永続参照を持つ Mesh と単独 Material を使った Entity を保存・復元できる。起動デモでは既存のインスタンス描画用 Material が全 Cube で共有される。
