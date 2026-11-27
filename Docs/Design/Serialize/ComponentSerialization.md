# Componentのシリアライズ

PrefabはEntityの親子関係、各EntityのComponent構成、復元するフィールドの値をYAMLに保存する。Prefabの初版はComponentを対象とする。SceneはこのComponent保存の仕組みを共有し、複数ルート、Active状態、Scriptの型参照を扱う。 [ECSの設計](../ECS.md) ・ [Sceneの保存・読み込み](SceneSerialization.md)

## 保存フィールドの宣言

保存するフィールドはComponentの宣言内で指定する。各項目の先頭はYAMLのキーで、C++のメンバ名を変更しても維持できる。

```cpp
// Name の宣言内
SERIALIZE_FIELDS(
    ("value", &Name::m_value)
)

// Transform の宣言内
SERIALIZE_FIELDS(
    ("position", &TransformAPIImpl::GetLocalPosition, &TransformAPIImpl::SetLocalPosition)
    ("rotation", &TransformAPIImpl::GetLocalRotation, &TransformAPIImpl::SetLocalRotation)
    ("scale",    &TransformAPIImpl::GetLocalScale,    &TransformAPIImpl::SetLocalScale)
)
```

| 宣言                     | 読み書き                                              |
| ---------------------- | ------------------------------------------------- |
| `(キー, メンバポインタ)`        | 対象Componentを解決し、指定のメンバを読み書きする                     |
| `(キー, Getter, Setter)` | Getterに対象の `EntityHandle`、SetterにHandleと変換済みの値を渡す |

マクロはフィールド一覧を返す `static constexpr SerializeFields()` に展開する。各項目は一覧を組み立てる連続した `operator()` 呼び出しとし、Component登録時に `requires` で関数の有無を確認する。Registryはキー、型、読み書き操作をComponent型ごとに保持する。登録と呼び出しは2つの宣言形式で共通化する。

例えば `Transform` のSetterは既存のLocal TRS操作を通る。更新時は他のTRS要素を維持し、Local行列と子孫のWorld行列を無効化する。上の例で参照している `TransformAPIImpl` の関数は現状privateかつEngine内部にあるため、実装時はEngine内部での宣言とアクセス権を揃える。 [Transformの更新契約](../Transform.md) ・ [現行API](../../../Engine/Include/API/ECSProcess/TransformAPI.h)

フィールド情報は型単位で保持する。Componentには関数と宣言を置けるため、各インスタンスのデータサイズとChunk内の配列配置を維持できる。通常のProcessはChunkのComponent列を走査し、Registry経由のフィールド操作は保存・復元や将来のUIで使う。UIで編集できる範囲は別途決め、編集が許可されたフィールドには同じSetterを使う。

## メモリ上の保存データ

`ECSSystem::SerializeEntity(EntityHandle)` は、1 Entityの取得時点の値を `SerializedEntity` として返す。生成待ち、破棄予約済み、無効なHandle、構造変更の適用中は結果を返さない。非アクティブなEntityも取得できる。

- `SerializedEntity` は取得元のHandle、Componentの配列、子を値として所有する `m_children` を持つ。単体取得では `m_children` を空にする。取得元のHandleはライブEntityから取得する場合に設定する。
- `SerializedComponent` はRegistryの登録名と、フィールド名をキーとする `std::unordered_map<std::string, SerializedValue>` を持つ。Entityが持つ全Componentを記録し、保存対象がなければ空のMapにする。
- 表示やYAML出力ではRegistryのフィールド宣言順にキーを引く。スナップショットは名前と値を所有し、ChunkやRegistryへの参照を持たない。
- `Prefab` は `AssetHandle<Prefab>`、名前、ルートの `SerializedEntity` を持つ。YAMLから読み込んだPrefabと、モデルImporterが生成するPrefabは既存の `AssetStorage<Prefab>` に登録する。

`SerializedValue` は真偽値、符号付き・符号なし64bit整数、`double`、文字列、値の配列、`AssetRef` を保持する。フィールドの読み出し関数は、登録時に分かっているC++の型に応じて、メンバまたはGetterの値を直接変換する。

| C++の型                                    | 保存する値                          |
| ---------------------------------------- | ------------------------------ |
| `bool`、整数、`float`、`double`、`std::string` | 対応するスカラー                       |
| `Vector3`、`Quaternion`                   | `[x, y, z]`、`[x, y, z, w]` の配列 |
| `std::vector<T>`                         | 各要素を同じ規則で変換した配列。空なら `[]`       |
| `AssetRef`                               | `{guid, localId}`。未設定は `null`  |

未対応型の登録はコンパイル時にエラーにする。入れ子の構造体は、保存フィールドを宣言した型をキー付きのマップへ再帰変換する形で後から追加する。

World上の `TransformHierarchy` は実行中のEntityをHandleで結び付ける。Prefabの子は `m_children` に格納し、取得時点の階層を保持する。展開後にWorldのEntityや親子関係を変更しても、Prefabの内容には反映しない。RapidYAMLはYAML入出力の境界で使う。

## スナップショットの取得と保存

`ECSSystem::SaveEntity()` は、Worldの `TransformHierarchy` をたどって、ルートと子孫を `std::optional<SerializedEntity>` で返す。単体の取得処理を再利用して子孫を `m_children` に格納し、破棄予約済みの子とその配下は除く。取得やScript照会に失敗したら、ログを出して `std::nullopt` を返す。

| オーバーロード | Script参照の取得 |
| --- | --- |
| `SaveEntity(rootHandle)` | 保存対象のルートと子孫のHandleを集め、`SaveEntity()` の中でGameRuntimeへの一括照会から `EntityHandle → Script GUID列` のmapを取得する。 |
| `SaveEntity(rootHandle, scriptRefsByEntity)` | 呼び出し側が取得済みのmapを読み取り専用で借用する。Scene保存では全ルートで同じmapを使う。 |

どちらもルートと子孫の各ノードで、保存元Handleをキーにmapを引く。一致したGUIDを `{guid, localId: 0}` の `AssetRef` として `SerializedEntity` に格納する。mapに項目がないEntityのScript参照は空とする。

引数1つの呼び出しで使う照会はEngineが接続し、GameRuntimeの破棄前に切断する。GameRuntime未接続時はScript参照を空としてスナップショットを作る。Scene保存は、Scriptの追加・削除を反映した後の状態を使う。 [Script参照の取得](../GameRuntime/Script.md#scene保存用の逆引き) ・ [Scene保存の手順](SceneSerialization.md#worldからの保存)

Prefabの `AssetSystem::Save` は取得済みのルート、Prefab名、任意の保存先を受け取る。Editorなどが元のPrefabへ上書きする場合は、保存先を指定する。保存先を省略した新規保存では共通の規則で `Assets/` 直下に自動命名し、ファイル名として使える場合はPrefab名を使う。Scene上のインスタンスと元Prefabの関係を持つ `PrefabInstance` は後続作業とする。 [YAML Asset の保存](../AssetSystem.md#yaml-asset-の保存)

Prefab用Serializerはルートから `m_children` の順に深さ優先でたどり、`id: 1` から採番する。子の `parent` には直近の親のIDを出力する。子がいなければルート1件を出力する。この階層変換はScene用Serializerとも共有する。

Componentはスナップショットの配列順に、フィールドはRegistryの宣言順に出力する。保存対象がないComponentは `{}`、空の配列値は `[]` とする。

RegistryにないComponentやフィールドはログに出し、ツリーの生成に失敗したことを返す。完成したツリーは共通の `YamlWriter` へ渡す。

## PrefabのYAML

最上層の `prefabName` は呼び出し側が指定するUTF-8の名前とする。出力先を変えても同じ名前を指定できる。空名は出力先を開く前にログを出して拒否する。Entityの一覧は `entities` に格納する。

`components` には必須Componentも含め、各Entityが持つ全Componentを記録する。キーはRegistryの登録名とする。保存フィールドがないComponentは空のマップで表す。

```yaml
prefabName: "Character"
entities:
  - id: 1
    components:
      "Hestia::Name":
        value: Character
      "Hestia::Tag": {}
      "Hestia::Layer": {}
      "Hestia::Transform":
        position: [0, 0, 0]
      "Hestia::TransformCache": {}
      "Hestia::TransformHierarchy": {}
  - id: 2
    parent: 1
    components:
      "Hestia::Name":
        value: Head
      "Hestia::Tag": {}
      "Hestia::Layer": {}
      "Hestia::Transform":
        position: [0, 1.6, 0]
      "Hestia::TransformCache": {}
      "Hestia::TransformHierarchy": {}
```

Prefab内の `id` は配列の添字と独立した、一意な正の整数とする。子Entityの `parent` は同じPrefab内の `id` を参照し、唯一のルートは `parent` を省略する。保存する値はNameやLocal TRSなどの初期値で、行列キャッシュ、dirtyフラグ、実行時Handleやポインタは生成時に組み直す。

Asset参照は `{guid, localId}` で保存し、復元時に型付きHandleへ解決する。ここでの `localId` は元Assetファイル内のIDを表す。 [AssetDBの設計](../Asset/AssetDB.md#asset参照とクラスの責務)

## 読み込み

`.prefab` はAssetDBに `type: Prefab` として登録し、`Assets::Load<Prefab>(path)` または `AssetRef` で取得する。LoaderはYAMLの `entities` の各行を `SerializedEntity` に変換し、一時的に `id → SerializedEntity` と `親ID → 子IDの一覧` を作る。

ルートから子IDをたどり、各 `SerializedEntity` を親の `m_children` に移して `Prefab` を構築する。ファイル内IDはこの組み立てに使い、完成したツリーの親子関係は `m_children` で表す。この処理はWorldのEntity生成から独立している。YAMLや親子関係が不正な場合、またはComponent名・フィールド名・値がRegistryの宣言と合わない場合は、ファイルパスと理由をログに出し、無効Handleを返す。

## 復元

`ECSSystem::Instantiate` は次のオーバーロードを持つ。成功時は生成したルートの `EntityHandle`、失敗時は無効Handleを返す。

```cpp
static EntityHandle Instantiate(const Prefab& prefab);
static EntityHandle Instantiate(AssetHandle<Prefab> handle);
static EntityHandle Instantiate(const Prefab& prefab,
    const Vector3& position, const Quaternion& rotation, const Vector3& scale);
static EntityHandle Instantiate(AssetHandle<Prefab> handle,
    const Vector3& position, const Quaternion& rotation, const Vector3& scale);
```

Handle版は `AssetSystem::Get(handle)` でPrefabを取得して対応する参照版へ委譲する。位置・回転・スケールを指定した場合は、生成したルートのLocal TRSだけを上書きする。指定を省略した場合はルートも保存値を使う。子は保存済みのLocal TRSを使う。

Worldへの展開では、Prefabのルートから親を先に作る。各 `SerializedEntity` のComponent名をRegistryで型IDに変換し、必須型を含む一覧を作成する。この時出来る一覧は`ComponentID`の配列であり、`ArchetypeSignature` と等価になる。よって、`CreateEntity(componentTypes)` に渡すことで正常にEntityを作成できる。登録済みの書き込み操作で保存値を適用し、省略したフィールドにはComponentの既定値を使う。 [Entityの生成契約](../ECS.md)

子の展開には作成済みの親の `EntityHandle` を渡し、子のHandleが得られたら `KeepLocal` で接続する。親子関係のために実行時のID対応表を作る必要はない。Setterや親子の設定が失敗した場合は対象のEntityと処理をログに出し、生成失敗として扱う。初期値と親子関係の設定を終えてから展開結果を返す。

親子付きEntityを保存し、`.prefab` を読み込んで名前・Local TRS・Mesh参照・親子関係を復元できることを完了条件とする。同じPrefabを2回展開し、それぞれが独立した階層になることも確認する。


