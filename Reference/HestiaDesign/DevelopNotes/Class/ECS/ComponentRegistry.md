# Component Registry
---
Engine内部のArchetype ECSで使用するComponentの型ID、配置情報、型操作を管理するクラス設計草案。
今回の会話で合意したComponent登録の方向を記録する。宣言は概念設計用のAPIスケッチであり、Productionコードではない。

## 基本方針
---
- Componentは通常のC++のデータ型とし、共通の仮想基底クラスを要求しない。
- 型名から決定的な64bitのComponentTypeIdを作り、登録情報をIDで管理する。
- 非自明な構築・破棄を許容し、Chunk内の移動には型ごとのMove構築を使用する。
- [ECSSystem](./ECSSystem.md#ecssystem)がRegistryと1つの[ECSWorld](./ECSWorld.md#ecsworld)を所有し、WorldはRegistryを非所有で参照する。
- 対象はEngine内部のComponent管理。Systemコールバックの登録方法、GameRuntimeとの接続、Reflectionの具体設計は別途検討する。

## ComponentRegistry
---
**役割**

C++型とComponentTypeIdを対応付け、Archetype／Chunkが使用するサイズ、アラインメント、型操作を保持する。
Componentインスタンスそのものの保存やEntityの管理は行わない。

```cpp
namespace Hestia
{
    class ComponentRegistry
    {
    public:
        template<class T>
        ComponentTypeId Register(std::string_view name);

        template<class T>
        ComponentTypeId GetId() const;

        const ComponentInfo& GetInfo(ComponentTypeId id) const;

    private:
        template<class T>
        ComponentTypeId Store(ComponentInfo info);

        std::unordered_map<ComponentTypeId, ComponentInfo> m_components;
        std::unordered_map<std::type_index, ComponentTypeId> m_typeIds;
    };
}
```

[ComponentTypeId](#componenttypeid)  
[ComponentInfo](#componentinfo)  
[Component](./Component.md)

**所有・参照**：ECSSystemがRegistryを所有する。Registryは型名とComponentInfoを所有し、C++型からIDへの検索表を保持する。Componentの実データはECSSystem管理下のChunkが保持する。Componentの破棄に型操作が必要なため、Registryはそれらの破棄が完了するまで保持する。

**APIとメンバ**：

- `Register<T>(name)`は型名からIDを生成し、`sizeof(T)`、`alignof(T)`、型操作をまとめて登録する。
- `Store<T>`はID・型名・C++型の対応を確認して、2つの表へ保存する内部処理。
- `GetId<T>()`は登録済みのC++型からIDを取得する。暗黙の新規登録は行わない。
- `GetInfo(id)`は登録情報を非所有のconst参照で返す。
- `m_typeIds`の`std::type_index`は実行中の型検索だけに使用する。保存用IDの生成には使用しない。

登録時に型操作を準備し、ChunkでComponentを処理するたびに型名のハッシュや登録処理を繰り返さない。

### ComponentTypeId
---
**役割**

Componentの型を識別する整数ID。個々のComponentインスタンスやEntityを識別するIDではない。

```cpp
namespace Hestia
{
    using ComponentTypeId = uint64_t;
}
```

[ComponentRegistry](#componentregistry)

名前空間を含む型名を明示的に登録する。同じ型名と同じハッシュ方式から、実行をまたいで同じIDを生成する。

```cpp
registry.Register<Hestia::Transform>("Hestia::Transform");
registry.Register<Hestia::MeshRenderer>("Hestia::MeshRenderer");
```

[Transform](./Component.md#transform)  
[MeshRenderer](./Component.md#meshrenderer)

**型名変更時の互換性は保証しない。** 保存データにIDを記録すれば、同じ登録名を使用する型へ対応付けられる。ただし、メンバ構成の変更やデータ形式の互換性までIDが保証するものではない。

ハッシュだけでは数学的な一意性を保証できないため、Registryは元の型名も保持する。異なる名前が同じIDになった場合は登録を失敗させ、同じ型として扱わない。

ハッシュIDは連番ではない。`m_components`ではIDを検索キーとし、IDの値をそのまま配列の添字にしない。

**未確定**：ハッシュのアルゴリズムと文字列の符号化。保存用IDを実装する前に固定する。`typeid(T).name()`や`hash_code()`を保存用IDとして使用しない。

### ComponentInfo
---
**役割**

Component型の配置情報と型操作を、型を消去した共通形式で保持する。Archetype／Chunkは具体的なC++型を知らなくても、この情報を使って保存領域と寿命を管理できる。

```cpp
namespace Hestia
{
    struct ComponentInfo
    {
        ComponentTypeId m_id;
        std::string m_name;

        size_t m_size;
        size_t m_alignment;

        void (*m_construct)(void* destination);
        void (*m_destroy)(void* object) noexcept;

        void (*m_moveConstruct)(
            void* destination, void* source) noexcept;

        void (*m_copyConstruct)(
            void* destination, const void* source);
    };
}
```

[ComponentTypeId](#componenttypeid)  
[ComponentRegistry](#componentregistry)  
[MeshRenderer](./Component.md#meshrenderer)

| 操作 | 契約 |
| --- | --- |
| `m_construct` | 未構築の領域へ引数なしでTを構築する |
| `m_destroy` | 構築済みのTを破棄する。Chunkの保存領域自体は解放しない |
| `m_moveConstruct` | 未構築の移動先へTをMove構築する。移動元の破棄は含めない |
| `m_copyConstruct` | 未構築のコピー先へTをCopy構築する。コピー可能な型にだけ登録する |

登録TemplateがTのコンストラクタ・デストラクタを呼ぶ関数を作り、これらの関数ポインタに設定する。ComponentごとにRegistry用のラッパーを手書きする必要はない。T独自の所有関係に専用のMove／Copyが必要なら、T側で通常のC++の特殊メンバ関数として定義する。

**今回のAPIスケッチの前提**：

- 引数なしで構築可能な型を対象とする。コンストラクタとデストラクタは非自明でもよい。
- Move構築と破棄は例外を投げない型を対象とする。Chunk移動の途中で一部のComponentだけが移動済みになる処理を複雑化させないため。
- Copy可能な型ではCopy構築を登録し、それ以外は`m_copyConstruct`を`nullptr`とする。
- `std::vector`などを含むComponentも、C++の型操作を通して扱う。バイト列の単純コピーで非自明な型を移動しない。

## 登録とChunkからの利用
---
登録時は次の順で情報を作る。

1. `Register<T>(name)`で型名を受け取る。
2. 型名からComponentTypeIdを生成する。
3. Tのサイズ・アラインメント・生成／破棄・Move／Copy操作を準備する。
4. 型・名前・IDの対応を確認し、Registryへ保持する。

ArchetypeはComponentTypeIdの集合をSignatureとして識別し、登録情報を使ってComponentの列をChunk内へ配置する方向とする。Queryは必要なIDに合うArchetypeを探す。具体的なSignature、Layout、Queryのクラス構成は後続で定める。

Componentを別のChunkへ移す場合は、移動先の未構築領域へ`m_moveConstruct`を呼び、その後で移動元に`m_destroy`を呼ぶ。既存オブジェクトへのMove代入とは分けて扱う。Swap Removeの対象行やEntity位置の更新順は、Chunk側の設計で定める。

## 未確定事項
---
- 同一型・同一名を再登録した場合の扱い。同一型を別名で登録する場合を含め、登録の整合確認と失敗の通知方法は未確定。
- 未登録型の`GetId<T>()`、未知のIDの`GetInfo()`に対する失敗の通知方法。
- 初期値を指定するComponent追加APIと、デフォルト構築できない型の扱い。
- 登録を行う具体的な初期化タイミング。
- 保存データの読み書き。型IDによる識別と、各メンバの保存・復元は別の処理として設計する。

## 関連資料
---
[ClassDesign](../../ClassDesign.md)の概念設計方針と[記述テンプレート](../_FormatTemplate.md)に従う。
既存の[ECSArchitecture_Plan1](../../../ECS/ECSArchitecture_Plan1.md)にも型登録と型操作の草案があるが、今回の会話で合意した範囲だけを本資料へ反映する。ScriptSystemやReflectionの詳細は引き継がない。
