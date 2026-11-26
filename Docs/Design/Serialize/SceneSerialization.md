# Sceneの保存・読み込み

Sceneは単一の `ECSWorld` にあるEntityの構成、親子関係、Active状態、Scriptの型参照を `.scene` に保存する。保存と読み込みは値のスナップショットを扱い、Worldの入れ替えは `SceneSystem` が行う。

[SceneSystemの設計](../SceneSystem.md) ・ [Componentのシリアライズ](ComponentSerialization.md) ・ [AssetDBの設計](../Asset/AssetDB.md)

## 保存データ

Sceneのスナップショットは `std::vector<SerializedEntity>` として複数のルートを所有する。各ノードは既存のComponent値、子ノード、保存元Handleに加え、EntityのActive状態とScript型の `std::vector<AssetRef>` を持つ。保存元HandleはライブWorldから値を集める間に使い、ファイルには書かない。

`.scene` はBOMなしUTF-8のYAMLとし、トップレベルに `version: 1` と `entities` を置く。Sceneを選ぶキーはAssetルートからの相対パスとする。ルートが0件のSceneは `entities: []` で表す。

| Entityの項目 | 契約 |
| --- | --- |
| `id` | Sceneファイル全体で一意の正の整数。保存時は全ルートを通して深さ優先に `1` から振る。 |
| `parent` | 同じファイル内の親Entityの `id`。親を持たないルートでは省略する。 |
| `active` | EntityのActive設定値。真偽値を保存する。 |
| `components` | 必須Componentを含む型と保存フィールド。型名、フィールド名、値の規則はPrefabと共有する。 |
| `scripts` | アタッチ済みScript型の `{guid, localId: 0}` の配列。省略時は空とする。各Scriptは型の既定状態から生成する。 |

```yaml
version: 1
entities:
  - id: 1
    active: false
    components:
      "Hestia::Name": { value: Spinner }
      "Hestia::Tag": {}
      "Hestia::Layer": {}
      "Hestia::Transform": {}
      "Hestia::TransformCache": {}
      "Hestia::TransformHierarchy": {}
    scripts:
      - { guid: 2db48ec9-7a6f-49a2-8c79-73639d0a8572, localId: 0 }
```

ファイル内の `id` は親子関係を再構築するために使う。展開時に新しい `EntityHandle` を発行する。MeshやMaterialなどのAsset参照は、従来どおり元Assetの `{guid, localId}` で記録する。

## Worldからの保存

1. GameRuntimeとECSのフレーム末尾処理が終わったWorldを対象にする。[Archetypeクエリ](../ECS.md#archetypeクエリ)に `TransformHierarchy` を渡す。
2. 一致したArchetypeごとに `TransformHierarchy` の列を引き、各Chunkの `Entity` 列と対応する行を順に読む。全EntityのHandleを照会用に集め、親Handleが空のEntityをルート一覧にも入れる。非ActiveのEntityも含める。
3. 照会用のHandle一覧を `QueryScriptAttachments` に一度渡し、`EntityHandle → Script GUID列` のmapを取得する。 [Script参照の取得](../GameRuntime/Script.md#scene保存用の逆引き)
4. ルートごとに `SaveEntity(root, scriptRefsByEntity)` を呼び、同じmapを渡す。`SaveEntity()` は各ノードのComponent値とActive設定を取り込み、ルートと子孫それぞれのHandleでmapを引いてScript参照も `SerializedEntity` に格納する。 [SaveEntityのオーバーロード](ComponentSerialization.md#スナップショットの取得と保存)
5. 全ルート分を取得できたら、`SceneSystem` がスナップショットと保存先を `AssetSystem::Save` に渡す。Scene 用 Serializer がツリーを組み立て、共通の `YamlWriter` で出力する。既存のSceneを上書きするときはGUIDを引き継ぐ。新規保存では `.meta` を作成してAssetDBの索引に登録し、確定した相対パスを返す。 [YAML Asset の保存](../AssetSystem.md#yaml-asset-の保存)

ルート収集とScript照会は、反映と破棄が済んだ状態を読む。スナップショットの取得、書き込み、メタデータの登録のいずれかが失敗した場合は、保存先と理由を報告する。

## ファイルからの読み戻し

`.scene` の `.meta` は `type: Scene` とする。AssetDBが相対パスから `.scene` の情報を解決し、読み込み処理がそのたびにファイルを開く。AssetDBへの登録は型、パス、GUIDの索引に限り、スナップショットの寿命は読み込みから展開完了までとする。新規保存したSceneも、索引への登録後は同じ起動中にロードできる。

読み込み時はComponentの型名、保存フィールドと値をRegistryに照合する。IDの重複、存在しない親、循環、対応外のバージョン、形式の不一致は読み込み失敗とする。`id` と `parent` からツリーを組み、すべてのルートを一時データとして返す。

`SceneSystem` は親から順に全Entityを生成し、Component値、`KeepLocal` での親子関係、Active設定を反映する。その後、GameRuntimeが各ScriptのGUIDを解決してアタッチする。対応する型が未登録のGUIDはSceneパス・対象Entity・GUIDをエラーログに出し、そのScriptだけを省いて復元を続ける。 [切り替えの手順](../SceneSystem.md#切り替え)
