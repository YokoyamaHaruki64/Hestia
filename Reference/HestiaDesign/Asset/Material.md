# MaterialShader / Material
Material系Assetの型、値の保持、描画ごとのOverride、Shader Reloadの扱いを整理する設計草案。今回の会話で合意した方針を記録する。コードは責務を示すAPIスケッチであり、型一覧・具体的な貸出方式・DLL境界を含む完成したHeaderではない。

[[AssetManagement]]・[[AssetType]] のAsset所有と貸出参照を前提にする。Shaderの記述形式は [[MaterialShaderDSL]]、描画要求の受け渡しは [[RenderingArchitecture]] を参照。

## 基本方針
- Materialの正本は型付きのProperty値とする。ShaderがGPUで使用していないPropertyも保持する。
- GPU Layoutの正本はコンパイル結果のReflectionとする。型付き値のメモリ配置や宣言順からGPU Offsetを計算しない。
- Materialは `AssetHandle<MaterialShader>` を持ち、Schemaを所有・保持しない。SchemaはMaterialShaderが保持する。
- AssetHandleは解決用の識別子。ゲームコードは貸出参照またはポインタからMaterialを直接操作でき、操作ごとにAssetDBへ問い合わせない。`shared_ptr` は使わない。
- Material・MaterialShader等のAssetは、それぞれ自身を識別する `AssetHandle<T>` も保持する。これは所有権を持つ参照ではなく、管理側への委譲や登録情報との対応に使う。
- Overrideは描画ごとの値として扱い、共有Materialの基準値を書き換えない。
- GPU Resource、Descriptor、PSO、GPU用キャッシュの管理はGraphics内部に置く。
- 操作の失敗はLogSystemへ記録する。呼び出し側が成否を必要とするAPIは `bool` 等で返し、`expected` は導入しない。

## Propertyの識別と型付き値
### PropertyID
`MaterialPropertyID` はProperty名から生成する安定したハッシュとする。同じ名前ならMaterialやShader、Reload前後にかかわらず同じIDになる。

```cpp
using MaterialPropertyID = uint64_t;

// ハッシュ方式は未確定。実行ごとに変わらない方式を使う。
MaterialPropertyID MakeMaterialPropertyID(std::string_view name);
```

従来の「上位bitにMaterialID、下位bitにPropertyID」という複合識別は採用しない。MaterialはHandleまたは貸出参照で別途指定する。

PropertyID、型付き値配列のindex、GPU Offsetは別の情報である。ShaderはPropertyIDから論理Propertyのindexを解決する情報を持つ。名前はEditor表示・Serialize・衝突検出のために残し、Shader読み込み時に異なる名前のハッシュ衝突を検出したらエラーとする。移行時にもハッシュ一致だけで異なる名前を同一Propertyと判断しない。

名前から生成したID自体はReloadで変わらない。一方、解決済みの配列indexやGPU Layoutは変わり得るため、これらのキャッシュはShader世代の変更時に再構築する。毎回の操作で名前をハッシュ化する必要はなく、ゲームコードがIDを保持して使用できる。

### 型付き値

`MaterialPropertyValue` は数値、Vector、Color、Matrix、Texture参照などを型付きで表す。内部表現は `variant` を想定するが、対応する型の全一覧は未確定。Texture値は `AssetHandle<Texture>` とする。

MaterialShaderのSchemaはProperty名・ID・型・既定値・編集用情報を保持する。MaterialはそのSchemaに対応した値配列を持つ。値配列の順序がGPU Layoutを意味することはない。

## MaterialShader
MaterialShaderは現在のShader世代として、次の情報をまとめて保持する。

| 情報 | 役割 |
|---|---|
| Schema | Property名・ID・型・既定値・編集用情報 |
| GPU Layout | ReflectionによるConstant Bufferのサイズ、Offset、Resource Binding |
| Pass列 | RenderType、Phase、描画状態、コンパイル済みShader等 |
| Revision | Shaderの内容が更新された世代を識別する |

RevisionはAssetHandleの無効化を検出する世代とは別の情報とする。AssetHandleを維持したままShaderの内容をReloadできる。
MaterialShaderはReload時の移行用に1世代前のSchemaも保持する。MaterialごとにSchemaを複製したり、共有所有したりしない。旧Schemaが不要になる条件は後述する。

### Passの連続配置と公開情報
Passは `std::vector<MaterialShaderPass>` に連続して保持し、順序はDSLの宣言順とする。実際の描画実行順はPipelineがRenderType・Phase等に従って決定する。
```cpp
// 公開情報のスケッチ。描画状態の具体的な型は未確定。
struct MaterialShaderPass
{
    std::string name;
    RenderType renderType;
    RenderPhase phase;
    RenderState renderState;
};
```

RenderType、Phase、Blend・Depth・Cull等の状態は読み取り専用で公開する。D3D12のPSOやDescriptorをゲームコードへ公開する意味ではない。コンパイル済みShader等の内部情報と公開情報を同じ構造体に置くか、その具体的な配置は未確定。

Pass列は読み取り専用View等で参照する。ポインタ・参照・Viewの有効性はReloadをまたいで保証しない。通常のPass参照に世代チェックを追加する前提にはしない。リリース版ではReloadを行わず、Editorでは参照を使用している処理が終わった境界でReloadを実施する。

## Material
MaterialはShaderへの参照と基準値を保持し、値操作の検証と更新番号の管理を行う。

```cpp
enum class MaterialState
{
    Ready,
    Error,
};

class Material
{
public:
    /// 基準値を変更する。存在しないPropertyや型不一致では失敗する。
    bool SetProperty(MaterialPropertyID id, const MaterialPropertyValue& value);

    /// 基準値を取得する。Error状態では通常の値アクセスを許可しない。
    bool GetProperty(MaterialPropertyID id, MaterialPropertyValue& out) const;

    /// AssetDB側の管理処理へ委譲し、値の移行と逆引き登録を更新する。
    bool SetShader(AssetHandle<MaterialShader> shader);

    AssetHandle<Material> GetHandle() const noexcept;
    AssetHandle<MaterialShader> GetShader() const noexcept;
    MaterialState GetState() const noexcept;

private:
    AssetHandle<Material> m_handle;
    AssetHandle<MaterialShader> m_shader;
    std::vector<MaterialPropertyValue> m_values;
    uint64_t m_shaderRevision = 0;
    uint64_t m_valueRevision = 0;
    MaterialState m_state = MaterialState::Error;
};
```

この例は主要状態と操作を示すものであり、PropertyID検索の補助情報や各型の便利なGet/Setを省略している。値が実際に変わった場合にだけValue Revisionを増やす。GPU未使用のPropertyでも、Setした値を保持してGet・保存できる。

ゲームコードはAssetDBから一度得た貸出参照等を使って、MaterialのGet/Setを直接呼べる。Assetの所有権はCache側に残る。貸出期間、解放・Unloadとの関係、Shader参照の解決方法は共通Assetの貸出方式に合わせて詰める。
Shaderの変更は `Material::SetShader` として公開する。この関数は自身のMaterial Handleと変更先Shader Handleを使い、内部でAssetDB側のMaterial管理処理へ委譲するラッパとする。ゲームコードが別途AssetDBへ問い合わせる必要はない。

委譲先は変更先Shaderの解決・検証、Property値の移行、ShaderからMaterialへの逆引き登録更新、Shader RevisionとGPU用キャッシュの更新・無効化をまとめて扱う。`SetShader` がShader Handleだけを直接書き換える実装にはしない。AssetDB側の具体的な窓口名と、Materialからその窓口へアクセスする方法は未確定。内部で管理側から再び公開 `SetShader` を呼ぶと再帰するため、管理側は公開ラッパとは別の内部適用処理を使う。

自身のHandleはAsset管理側が登録時に設定する。MaterialShaderも自身の `AssetHandle<MaterialShader>` を持つ。Assetを複製する場合は新しい登録で新しいHandleを与え、元のHandleを複製先の識別として使わない。自身のHandleを持つことによってAssetの寿命が延びるわけではない。

## 描画ごとのOverride
OverrideはProperty名と型付き値の対応で保持する。名前を使うことで、Editor表示・Serialize・Shaderの宣言順変更に対応しやすくする。

```cpp
// 保持形式の候補。unordered_mapの採用は未確定。
struct MaterialOverrides
{
    std::unordered_map<std::string, MaterialPropertyValue> values;
    uint64_t revision = 0;
};
```

名前をキーにする方針と、コンテナとしてハッシュマップを使うかどうかは分ける。少数のOverrideなら `{名前, 値}` の連続配列も候補になる。最適な保持形式は未検証であり、Public APIへ具体的なコンテナを固定しない。
Object側がOverrideを継続して保持することはできるが、GraphicsへSubmitするときは、その描画のデータとしてコピーする。Overrideが指定されていないPropertyにはMaterialの基準値を使う。
名前からPropertyIDやindexへ解決した結果は、Overrideの変更時またはShader世代の変更時に更新する案とする。解決済みindexを別Shaderへ使い回さない。GPU Layoutへの対応はReflectionから解決する。

Shaderに定義されているがGPU未使用のPropertyも保持できる。Shaderに存在しない名前、型不一致等のOverrideをどう扱うかは未確定。Submitを失敗させてログを出す案を検討しているが、Overrideの不正によって共有Material自体をErrorにする方針ではない。

## GPU用データの構築
Materialの基準値から、Reflectionに従ってGPU用のバイト列とResource Bindingを構築する。GPU未使用のPropertyは正本の値として残し、GPUへの書き込み対象から外す。

基準値の変更またはShader世代・Layoutの変更時にキャッシュを再構築する。通常の描画ではキャッシュを使う。Overrideがある描画では、描画専用の領域へ基準データをコピーし、対応する値やTexture Bindingを上書きする。共有MaterialのGPU用キャッシュは変更しない。

バイト列の再構築とGPU Bufferへの配置は別の処理とする。GPUが使用中の領域を上書きせず、GPU Resource・Descriptorの寿命はFence完了までGraphics側で維持する。RenderThreadへゲーム側Materialの可変状態をそのまま参照させず、フレームへの受け渡し方法は [[RenderingArchitecture]] と整合させる。

## Shader ReloadとMaterial移行
Material管理側は `AssetHandle<MaterialShader>` から参照中のMaterialを逆引きできるようにする。登録は非所有の識別子で保持し、Material生成・Shader変更・破棄時に更新する。

1. 新しいShader候補をコンパイルし、Schema・Reflection・Passを検証する。候補が失敗した場合は現在のShaderを維持し、Material移行を行わない。
2. 新世代を適用する処理では旧Schemaを保持し、参照するMaterialをまとめて移行する。
3. 全Materialが新世代へ移行済み、または旧Schemaを参照しないError状態になったら、旧Schemaを破棄する。

通常の移行では、同名・同型の値を引き継ぐ。追加Propertyは既定値、削除Propertyは削除、型変更は新しい型の既定値とする。宣言順・GPU Offsetの変更やGPU未使用化だけを理由にMaterialをErrorにはしない。
移行不能の例は、Materialが利用可能な旧Schemaより古い世代のまま残っている、値配列と旧Schemaの件数・型が整合しない、Shader参照が解決できない等。これらはLogSystemへ記録し、該当MaterialをErrorにする。
Error化はフラグを立てるだけでは足りない。通常のGet/Setと描画から旧Schema・旧Layoutを使う経路を止め、旧GPUキャッシュを無効化する。描画はError用の代替Materialへ切り替える。これにより、移行に失敗したMaterialも「旧Schemaを必要としない状態」として扱える。復旧時に既定値から再初期化する操作の詳細は未確定。

[[AssetDB]] の登録情報Reloadは、ここで扱うShader再コンパイル・Material移行とは別の操作である。

## API確定前に残す事項
- Property型の全一覧、型別Get/Set、編集用メタデータの具体型。
- Property名ハッシュの方式と、名前の比較規則。
- Asset貸出参照の寿命契約と、Unload・Editor Reloadの実施境界。
- Pass公開情報とコンパイル済みデータの配置、PassごとのGPU Layoutの保持方式。
- Overrideの具体的なコンテナ、不正なOverrideの処理、解決キャッシュの所有者。
- GameLogic DLLからMaterialを直接操作するAPIとABI。直接操作する方針は維持しつつ、内部の `vector` / `variant` 等をそのままDLL境界へ渡す契約にはしない。
- Material更新とFramePacketの受け渡し、GPU用キャッシュの具体的な所有者。

Graphics全体のAPI確定はこの資料だけでは完了しない。Camera・Light・RenderItem等の提出APIと合わせて別途整理する。
