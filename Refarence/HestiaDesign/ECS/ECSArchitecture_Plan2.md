# Game Code Generation / CScript Build 設計
## 目的
ゲームロジックでは以下を併用する。
- 大量Entityを高速に処理する **Archetype ECS**
- 開発速度を優先する **Sparse Script**

現在の採用方針はC++で進めるPlan2とする。大まかな構成は[[ECSArchitecture_Plan1]]と同一で、Script部分をC#からC++へ置き換える。C++の知識を使ってゲーム制作を進め、初期のReflection・Editor・ツール開発の範囲を絞る。

Sparse Scriptは通常のC++として実装するが、最終的には `.h` / `.cpp` の2ファイル管理を避けるため、独自拡張子 `.cscript` からコード生成する。
コード生成機能はゲーム固有ツールではなく、最終的に **Engine側のToolとしてGit管理する**。
現段階ではGeneratorを先行実装せず、まずEngine基盤・Sparse Script・Game.dll周辺を完成させる。

Editor UIはEditorプロファイルのexeへ組み込む。Applicationは独立libにせずexeを直接作る。5プロファイルとDefineは[[BuildProfilesSketch]]を参照する。Editorの規模は小さく保ち、ゲーム制作に必要な機能から始める。大規模な統合Editorを目標にせず、コード編集はVisual Studioを基本とする。

後述のUtility / GeneratedGame / .cscript構成はGenerator導入後の構成を示す。初期は通常の.h / .cppを通常のGame.dllプロジェクトでビルドし、このツール構成の完成をゲーム制作の開始条件にしない。

## 最終的なプロジェクト構成(例)
```
ProjectRoot/
├ Project.sln
│
├ Assets/
│  ├ Scripts/
│  │  ├Enemy/
│  │  │  ├ Enemy.cscript
│  │  │  ├ EnemyState.h
│  │  │  └ EnemySystem.cpp
│  │  │
│  │  └ Player/
│  │     ├ Player.cscript
│  │     └ PlayerSystem.cpp
│  │
│  └ ...
│
├ GameLogic/
│  ├ Game.vcxproj
│  ├ GameBuild.props
│  │
│  └ Generated/
│     ├ GeneratedGame.vcxproj
│     └ ...
│
└ Engine/
   ├ Runtime/
   ├ ECS/
   ├ Graphics/
   │
   ├ Build/
   │  ├ GameBuild.props / Template
   │  └ Templates/
   │     └ GeneratedGame.vcxproj
   │
   └ Tools/
      └ GameCodeGenerator/
```

`Assets/` 以下にはゲームDLLを構成するソースを自由に配置できる。
特定の
```
Scripts/
Components/
Systems/
```
などのフォルダ構成は要求しない。
## Game.vcxproj
`Game.vcxproj`はゲームソース一覧を保持し、ビルドを調停するUtility相当のプロジェクトとする。自身ではC++のCompile / Linkを行わない。

ゲームコードとして扱うファイルは、
`Assets/` に存在するファイルではなく、`Game.vcxproj` に登録されているファイル
を正とする。

そのため、`Assets/` 全体のrecursive scanは行わない。

例：
```
<ClCompile Include="..\Assets\Enemy\EnemySystem.cpp" />
<ClInclude Include="..\Assets\Enemy\EnemyState.h" />
<None Include="..\Assets\Enemy\Enemy.cscript" />
```
Generatorは `Game.vcxproj` に登録されたファイルを基準に処理する。

`Game.vcxproj`のBuildでは、GameCodeGeneratorを実行してから`GeneratedGame.vcxproj`のBuildを起動する。
```text
Game.vcxproj Build
├─ GameCodeGenerator
└─ GeneratedGame.vcxproj Build
```

### 通常のC++コード
Archetype ECS用のComponentやSystemは通常のC++として記述する。
例：
```
Assets/Enemy/
├ EnemyState.h
└ EnemySystem.cpp
```
これらはコード生成せず、そのまま `GeneratedGame.vcxproj` のコンパイル対象とする。
Sparse ScriptとArchetype Component/Systemは最終的に同一の `Game.dll` に含まれる。
そのためGame側内部ではABI境界を持たず、通常のC++型として相互利用できる。

### CScript
Sparse Scriptでは独自拡張子 `.cscript` を使用する。
目的は、C++のheader/source分離コンパイルを維持しながら、開発者が1ファイルだけ編集できるようにすること。

Assets内で作成した`.cscript`は、エクスプローラーからVisual StudioへDrag & Dropする通常の操作で`Game.vcxproj`へ登録する。Visual Studioのカスタム項目による新規作成は、必要になった場合に検討する。

例：
```
HEADER_BEGIN

class Enemy : public Script
{
public:
    void Update(float dt);

private:
    float timer_ = 0.0f;
};

HEADER_END

SOURCE_BEGIN

void Enemy::Update(float dt)
{
    timer_ += dt;
}

SOURCE_END
```

Generatorによって、
```
Enemy.cscript
↓
Enemy.generated.h
Enemy.generated.cpp
```
へ分離する。

利用側では生成物であることを意識せず、
```
#include CSCRIPT_INCLUDE(Enemy)
```
と記述できるようにする。

`CSCRIPT_INCLUDE(name)`はEngine側Component取得APIなどをまとめた共通Include Headerに定義する。
```cpp
#define CSCRIPT_INCLUDE(name) <name.generated.h>
```
このマクロは`#include`のファイル名部分を生成する。`#include` directive自体は利用側に記述する。

2ファイルの往復を減らすことが`.cscript`導入の主な目的となる。Visual Studio拡張による編集支援は、初期Generatorとは分けて扱い、必要性が出た場合に検討する。

## Sparse Script Storage
Scriptは型ごとにStorageを持ち、Scriptインスタンスを`std::vector<T>`へ値として格納する。型ごとの配列を走査することで、Script本体を連続したメモリ上で処理する。
```text
ScriptStorage<Enemy>
├─ DenseScripts: Enemy[Count]
└─ Sparse: EntityIndex → (Generation, DenseIndex)
```

EntityからScriptへのランダムアクセスは、Entity HandleのIndexとGenerationをSparse側で照合し、DenseIndexを解決して取得する。`vector`の再確保でScriptの実体アドレスが変化しても、DenseIndexの並びは変わらないため、Entity HandleやSparseの対応は維持できる。Scriptへの生ポインタや参照は、Storageの構造変更をまたいで保持しない。

Script削除時はSwap Removeで末尾のScriptを空いた位置へ移し、そのScriptを所有するEntityのSparse上のDenseIndexを更新する。多数Entity間のランダムアクセスが性能上問題になった場合は、ScriptRef等の仕組みを追加する前に、対象処理をArchetype ECSへ移すことを検討する。

### Generated Directory
`.cscript` の生成物は、
```
GameLogic/Generated/
```
以下へ出力する。

元の `Assets/` 以下の相対ディレクトリ構造を維持する。
例：
```
Assets/Scripts/Enemy/Boss/Boss.cscript
```
から、
```
GameLogic/Generated/Scripts/Enemy/Boss/
├ Boss.generated.h
└ Boss.generated.cpp
```
を生成する。
Assets以下の相対ディレクトリ構造を保ち、生成ファイルの配置を元の`.cscript`から追えるようにする。

生成Headerの拡張子は`.generated.h`とし、手書きの`Enemy.h`と区別する。
生成された`.generated.cpp`から同じ階層のHeaderを使う場合は`#include "Enemy.generated.h"`とする。`.generated.h`はGeneratorの出力専用とする。これはCSCRIPT_INCLUDE(Enemy)で吸収する
## GeneratedGame.vcxproj
`GeneratedGame.vcxproj` は実際に `Game.dll` を生成する裏側のC++プロジェクトとする。

コンパイル対象：
```
Game.vcxprojに登録された通常の.cpp
+
.cscriptから生成された.generated.cpp
```
最終的に、
```
MSVC
↓
Game.dll
```
を生成する。
`GeneratedGame.vcxproj` は通常開発者が操作する必要はなく、Solution Explorer上で目立たせる必要もない。なんなら表示すら必要ない。

`GeneratedGame.vcxproj`では、生成された`.generated.cpp`を`ClCompile`の実行直前にTarget内のワイルドカードで列挙し、コンパイル対象へ追加する。
```xml
<Target Name="AddGeneratedSources"
        BeforeTargets="ClCompile">
  <ItemGroup>
    <_GeneratedSource Include="$(ProjectDir)**\*.generated.cpp" />
    <ClCompile Include="@(_GeneratedSource)" />
  </ItemGroup>
</Target>
```

Generatorは`Game.vcxproj`のBuild中に先に完了するため、GeneratedGame側のTargetが動く時点では出力ファイルが存在する。Target内でワイルドカードを評価することで、そのBuildで新たに生成したファイルもコンパイル対象へ含める。
### GeneratedGame.vcxproj の生成
`GeneratedGame.vcxproj` を毎回動的生成しない。
初回のみ、
```
GameLogic/Generated/GeneratedGame.vcxproj
```
が存在するか確認する。

存在しない場合のみ、Engine側に保持したテンプレートからコピーする。
```
if (!exists(generatedProject))
{
    CopyTemplate();
}
```
既に存在する場合は変更しない。
これによりMSBuild / MSVCのIncremental Buildを不要に崩さない。
テンプレートは一度Visual Studioで通常のDLLプロジェクトとして作成し、必要な設定をGUI上で行ったものを使用する。
XMLをコードから一から組み立てる設計にはしない。
## GameBuild.props
`Game.vcxproj` と `GeneratedGame.vcxproj` で共通するコンパイル設定はProperty Sheetへ分離する。
例：
```
Game.vcxproj
       ┐
       ├── GameBuild.props
       │
GeneratedGame.vcxproj
       ┘
```
`Additional Include Directories`には、`.generated.h`を格納するGenerated側のScriptディレクトリを含める。`CSCRIPT_INCLUDE(Enemy)`は`Enemy.generated.h`という名前を生成し、共通のInclude設定からそのファイルを解決する。
共通化対象：
- Additional Include Directories
- Preprocessor Definitions
- C++ Language Standard
- Runtime Library
- Warning設定
- Engine Include Path
- Engine Library Path
- Assets Root
- その他Gameコード共通設定

これにより、
```
Game.vcxprojではincludeできる
GeneratedGame.vcxprojではincludeできない
```
といった設定差異を防ぐ。
## GameCodeGenerator
Generatorは最終的にEngine側Toolとして実装する。
```
Engine/
└ Tools/
   └ GameCodeGenerator/
```
Generator自身はC++製CLI Toolを想定する。

役割：
```
Game.vcxproj読み込み
↓
登録された.cscript取得
↓
.cscriptの生成対象型・Field・Property・CUSTOM宣言を解析
↓
通常のC++コード、値の読み書き、Descriptor、一括登録コードを生成
↓
必要ならGeneratedGame.vcxproj初回生成
```
独自C++コンパイラは実装しない。
生成後のC++コンパイルは通常のMSVCに任せる。

Reflection登録コードはGame.dll側に生成し、Editor起動時やDLL Reload後に型ごとに一度登録する。`FIELD`マクロ自体はメンバ宣言だけを行い、登録処理は持たない。

## Incremental Build
Generatorは生成ファイルを毎回上書きしてはならない。
生成内容を既存ファイルと比較し、
```
if (oldContent == newContent)
{
    // 書き換えない
}
```
とする。

例えば `Enemy.cscript` のSOURCE部分だけ変更された場合、
```
#include "Enemy.generated.h"
→ 変更なし

Enemy.generated.cpp
→ 更新
```
となる。

その結果、
```
Enemy.generated.cpp
↓
Enemy.generated.objのみ再コンパイル
↓
Game.dll再リンク
```
となる。

HEADER部分が変更された場合は通常のC++ Header変更と同様、そのHeaderに依存するcppも再コンパイルされる。

## \#line
生成された `.generated.cpp` のエラー位置が生成ファイルではなく、元の `.cscript` を指すようにする。
例：
```
#include "Enemy.generated.h"

#line 18 "Assets/Scripts/Enemy/Enemy.cscript"

void Enemy::Update(float dt)
{
    ...
}
```

これによりVisual Studio上では、
```
Enemy.cscript(20): error ...
```
のように表示できる。
デバッグ・エラー確認時に生成コードを意識しなくて済むようにする。

## Reflection
Reflection情報はScript / Componentの型ごとに一度登録する。各インスタンスにメタデータを保持・登録させない。

GeneratorがFIELD一覧から、値の読み書きとInspector用のDescriptorを生成する。Game側の値は基本値・文字列・配列・Mapの中間表現へ変換し、Engine / Editorが利用する。独自型のC++定義をEngine側へ公開する必要はない。

### 初期対応と拡張
初期の自動Inspector・共通変換は、基本型、文字列、必要な数学型、それらの一次元配列に絞る。独自型の自動分解、独自型配列、入れ子配列は後回しにし、必要な独自型はCUSTOM Serializerと専用Inspectorで扱う。

Writer / Readerは保存・復元とEditorとの値の受け渡しを担当する。初期のInspector取得は表示対象のScript / Game側Component単位で行い、編集後も対象全体を再取得する。Field単位の再取得や複雑な差分更新は後回しにする。

Generator完成前は通常の.h / .cppに、手書きのDescriptor・登録Callbackと共通テンプレートを使う。FIELD等の宣言だけで自動登録されるわけではない。Generatorはこの定型処理を置き換えるために後から導入する。

以下には、Generator導入後の生成例と独自型の再帰対応を含む拡張例も残す。独自型の自動生成や再帰Descriptorの完成を初期実装の必須条件にしない。

以下のコードは設計を説明するスケッチ。マクロ名、API名、Descriptorの共有C++ Headerは詳細設計時に確定する。

### FIELDと生成対象
Generator導入後は基本型のFIELD一覧・読み書き・Descriptor・登録コードから生成を始める。以下はその後、明示的にReflection対象とした独自型とその配列にも拡張した場合の例。特殊な型はCUSTOM SerializerとカスタムInspectorで補う。

ScriptはSCRIPT_BODY、独自型はREFLECT_BODYで生成対象であることを示す。初期Generatorは、プロジェクトに登録された.cscript内の対象型を解析する。外部Headerの任意の型まで自動探索する構成にはせず、必要な型には手書きの登録・変換処理かCUSTOMを使う。

```cpp
struct Hoge
{
    REFLECT_BODY(Hoge);

    int a = 0;  // FIELDではないので自動処理の対象外

    FIELD(float, b, Serialize);
    FIELD(std::vector<float>, c, Serialize);
};

class Enemy : public Script
{
    SCRIPT_BODY(Enemy);

private:
    FIELD(float, speed, Serialize);
    FIELD(int, health, Serialize);
    FIELD(bool, enabled, Serialize);
    FIELD(std::string, displayName, Serialize);
    FIELD(Hoge, hoge, Serialize);
    FIELD(std::vector<Hoge>, patterns, Serialize);
    FIELD(float, internalValue, Serialize, HideInspector);
    FIELD(float, debugValue, NonSerialized);
};
```

SCRIPT_BODY / REFLECT_BODYは、生成するReflectionAccessへのfriend宣言になる。FIELDは通常のメンバ変数を宣言するだけで、登録処理を持たない。

```cpp
namespace Reflection // Public Headerの補助型。Subsystemではない。
{
    template<class T>
    struct Access;
}

#define SCRIPT_BODY(Type) friend struct Reflection::Access<Type>
#define REFLECT_BODY(Type) friend struct Reflection::Access<Type>
#define FIELD(Type, Name, ...) Type Name{}
```

Generatorが集めるのは型名、FIELD名、属性など。Fieldの型はdecltypeと共通テンプレートで判定できるため、Generator自身でC++の型を完全に解決する必要はない。

Serialize / NonSerializedはScene保存、HideInspectorはInspector表示の選別に使う。NonSerializedでもInspectorには表示できる。Generatorは属性を保持し、共通の読み書き処理が利用目的に応じて選別する。

### 生成する読み書き処理
HogeとEnemyから、次のような処理をGame.dll内に生成する。FIELDでないHoge::aには触れない。

```cpp
template<>
struct Reflection::Access<Hoge>
{
    static bool WriteFields(ValueWriter& writer, const Hoge& value)
    {
        return WriteField(writer, "b", value.b, Serialize)
            && WriteField(writer, "c", value.c, Serialize);
    }

    static bool ReadFields(ValueReader& reader, Hoge& value)
    {
        return ReadField(reader, "b", value.b, Serialize)
            && ReadField(reader, "c", value.c, Serialize);
    }

    static const TypeDesc* Describe();
};
```

```cpp
template<>
struct Reflection::Access<Enemy>
{
    static bool WriteFields(ValueWriter& writer, const Enemy& value)
    {
        return WriteField(writer, "speed", value.speed, Serialize)
            && WriteField(writer, "health", value.health, Serialize)
            && WriteField(writer, "enabled", value.enabled, Serialize)
            && WriteField(writer, "displayName", value.displayName, Serialize)
            && WriteField(writer, "hoge", value.hoge, Serialize)
            && WriteField(writer, "patterns", value.patterns, Serialize)
            && WriteField(writer, "internalValue", value.internalValue,
                Serialize | HideInspector)
            && WriteField(writer, "debugValue", value.debugValue, NonSerialized);
    }

    static bool ReadFields(ValueReader& reader, Enemy& value)
    {
        return ReadField(reader, "speed", value.speed, Serialize)
            && ReadField(reader, "health", value.health, Serialize)
            && ReadField(reader, "enabled", value.enabled, Serialize)
            && ReadField(reader, "displayName", value.displayName, Serialize)
            && ReadField(reader, "hoge", value.hoge, Serialize)
            && ReadField(reader, "patterns", value.patterns, Serialize)
            && ReadField(reader, "internalValue", value.internalValue,
                Serialize | HideInspector)
            && ReadField(reader, "debugValue", value.debugValue, NonSerialized);
    }

    static const TypeDesc* Describe();
    static void Register(); // Scripts FacadeへDescriptorとCallbackを渡す
};
```

WriteField / ReadFieldは属性による選別と名前の処理を担当し、値の変換を共通テンプレートへ渡す。選別対象外のFieldは処理せず成功として扱う。

示したReadFieldsはField単位の変換を説明する断片。正式なScene復元 / EditValue Callbackでは実体へ逐次書き込まず、反映対象Fieldの作業値をすべてDecode・検証してからまとめて適用する。FIELD外のGame状態を丸ごとコピー / 初期化しない。実体へ直接ReadFieldsを呼ぶだけの実装では、この失敗時契約を満たせない。

名前と値をWriterへ渡す部分は、たとえば次のように共通化する。

~~~cpp
template<class T>
bool WriteField(
    ValueWriter& writer,
    const char* name,
    const T& value,
    FieldFlags flags)
{
    if (!writer.ShouldProcess(flags))
        return true;

    return writer.Key(name)
        && WriteValue(writer, value);
}
~~~

Writer / ReaderにはScene保存・復元かInspectorでの取得・編集かを示す利用目的を持たせる。Inspectorからは変更対象を含む値ツリーを渡し、入力に存在しないFieldの値は維持する。Scene復元でも既存値へ読み込むため、FIELDでないメンバを丸ごと初期化しない。

### 共通テンプレートによる再帰変換
基本型、配列、独自型の処理は共通化する。WriterのValueは対応する基本型のオーバーロード、IsBasicValue / IsVector等は共通の型判定ヘルパーを想定する。

```cpp
template<class T>
bool WriteValue(ValueWriter& writer, const T& value)
{
    if constexpr (IsBasicValue<T>)
    {
        return writer.Value(value);
    }
    else if constexpr (IsVector<T>)
    {
        if (!writer.BeginArray(value.size()))
            return false;

        for (const auto& element : value)
        {
            if (!WriteValue(writer, element))
                return false;
        }

        return writer.EndArray();
    }
    else if constexpr (HasGeneratedFields<T>)
    {
        return writer.BeginMap()
	            && Reflection::Access<T>::WriteFields(writer, value) // 独自型の再帰 
            && writer.EndMap();
    }
    else
    {
        // CUSTOMの型別変換が登録されている場合
        return CustomSerializer<T>::Save(writer, value);
    }
}
```

ReadValueも同じ分類を使う。配列は入力の要素数に合わせてResizeして各要素を読み、独自型はReadFieldsへ委譲する。対応する生成情報もCUSTOM変換もない型は、コンパイル時に未対応として検出する。
たとえばEnemyのScene保存結果は次のようになる。Hoge::aとdebugValueは含まれず、HideInspectorのinternalValueは保存される。

```yaml
speed: 3.5
health: 100
enabled: true
displayName: EnemyA
hoge:
  b: 1.5
  c: [2.0, 3.0]
patterns:
  - b: 4.0
    c: [5.0, 6.0]
  - b: 7.0
    c: []
internalValue: 0.5
```

### FieldDescと型の構造
同じFIELD一覧からInspector用のDescriptorも生成する。FieldDescは名前・属性・型を持ち、TypeDescが独自型の子Fieldや配列の要素型を表す。

```cpp
struct TypeDesc;

struct FieldDesc
{
    StringView name;  // 登録中に借りるUTF-8 view
    FieldFlags flags;
    const TypeDesc* type;
};

struct TypeDesc
{
    ValueKind kind;  // Float、Int、Bool、String、Object、Array等

    // Objectの子Field
    const FieldDesc* fields;
    uint32_t fieldCount;

    // Arrayの要素型
    const TypeDesc* elementType;
};
```

HogeのDescriptor生成例：

```cpp
const TypeDesc* Reflection::Access<Hoge>::Describe()
{
    static const FieldDesc fields[] = {
        { "b", Serialize, DescribeType<decltype(Hoge::b)>() },
        { "c", Serialize, DescribeType<decltype(Hoge::c)>() }
    };

    static const TypeDesc type = {
        .kind = ValueKind::Object,
        .fields = fields,
        .fieldCount = 2,
        .elementType = nullptr
    };

    return &type;
}

template<class T>
const TypeDesc* DescribeType()
{
    if constexpr (IsBasicValue<T>)
        return BasicTypeDesc<T>();
    else if constexpr (IsVector<T>)
        return ArrayTypeDesc<T>(DescribeType<typename T::value_type>());
    else
        return Reflection::Access<T>::Describe();
}
```

CUSTOM専用型はCustomとして扱い、自動生成できない表示はカスタムInspectorで補う。上のStringViewの初期化等は説明を簡略化しており、共有型の配置と所有契約はHeaderレビューで揃える。

Enemyの構造は次のようにつながる。
```text
Enemy : Object
├ speed       : Float
├ health      : Int
├ enabled     : Bool
├ displayName : String
├ hoge        : Object
│ ├ b         : Float
│ └ c         : Array → Float
├ patterns    : Array
│ └ elementType : Object (Hoge)
│   ├ b       : Float
│   └ c       : Array → Float
├ internalValue : Float (HideInspector)
└ debugValue    : Float (NonSerialized)
```
配列のDescriptorには実際のindexや要素数を固定しない。要素の構造だけ登録し、現在の要素数と値は値ツリーから取得する。多重配列もelementTypeをたどって扱う。循環参照やオブジェクトグラフの自動保存は初期対応に含めない。

### Writer / ReaderとInternal C++ DLL Boundary
exe / Engine.dll / Game.dllは同一VS / MSVC Toolset / Runtime Library / Build Configurationで一緒にビルドする。Gameの登録・Entity操作はScripts / Entities等のPublic Facadeを使い、実装はAPI libに置く。Engine内部はSubsystem実体を使う。ReflectionのValueWriter / ValueReader等のPublic C++型、STLを含むValueTreeやDescriptorを維持する。所有やABIへの配慮が必要な入力だけAPI lib内部で変換し、Gameへ境界用型を公開しない。全APIのC化やFunction Tableは必須にしない。

```cpp
static bool SaveEnemy(const Enemy& instance, ValueWriter& writer)
{
    return Reflection::Access<Enemy>::WriteFields(writer, instance);
}

static bool LoadEnemy(Enemy& instance, ValueReader& reader)
{
    // 疑似コード：FIELD対象の作業値をDecodeし、検証後に一括適用する。
    auto staged = CaptureReflectedFields(instance);
    if (!ReadAndValidateReflectedFields(reader, staged))
        return false;
    ApplyReflectedFields(instance, std::move(staged));
    return true;
}
```

Writer / Readerを使うのはGame固有型をEditorが知らずに値を表示・編集・保存するため。DLL境界専用Serializerではない。Game内のC++型連携やEngine既知型の直接利用を制限しない。

Writer / Readerは処理中だけ借り、保存する値を所有側へコピーする。Engineは登録Descriptorの名前・子構造を必要な寿命へ所有コピーし、Game Callbackを使う間はDLLを保持する。Editorは値ツリーの作業コピーを編集し、Component / Scriptの実体書込Pointerを保持しない。これはValidation・Dirty / Physics同期のための責務境界。

External Stable Plugin ABIは別の後続問題。異なるCompiler / Runtime / Languageや独立配布・長期互換が必要になった時にC ABIやAdapterを検討する。現在のReflectionへ予約fieldや外部版互換を先行追加しない。詳細は[[SketchContracts]]と[[EditorCommandDesign]]。

### 自動InspectorとPROPERTY
EditorはDescriptorと現在の値ツリーをたどる。ObjectはField名で子の値を探し、Arrayは各値をelementTypeで表示し、基本型は対応する入力UIを表示する。
初期の編集では表示対象Script / Componentの値ツリーをReader経由で反映する。入力は自動Inspectorの対応型に限り、対象全体の取得・反映から始める。変更Fieldだけの入力や配列要素別の部分更新Callbackは、必要になった段階で検討する。
PROPERTYは直接メンバへ代入せず、開発者が用意したGetter / Setterを生成コードから呼ぶ。取得値をWriterへ渡し、Readerから変換した値をSetterへ渡す。ValidationやDirty通知が必要な操作もSetterを通す。

### Componentへの適用
Game側のArchetype Componentにも、FieldDesc / TypeDesc、Writer / Reader、CUSTOM Serializer、自動Inspectorを共通利用する。EditorがGame固有のC++型を知らなくても、値の表示・編集とScene保存・復元を扱える。

Scriptとの違いはインスタンスの解決方法となる。
~~~text
Entity Handle + ComponentTypeID
→ Worldから現在のComponent位置を解決
→ Game側の型消去済みCallbackへ渡す
→ Writerで値を取得 / Readerで変更を反映
~~~

Archetype MigrationやSwap RemoveでComponentの実体アドレスは変わるため、Editorは生ポインタを保持しない。操作ごとにEntityのGenerationとComponentの存在を確認して再取得する。Callback実行中は構造変更と対象データへの競合アクセスを避ける。

ComponentのDescriptorと値操作Callbackは型ごとに登録し、Construct / Move / Destroy等の型操作情報と同じGame.dllの寿命に従う。Reload前に旧Componentを破棄し、Descriptor・値操作Callbackも登録解除する。
Game側Component / Systemを通常の.h / .cppで記述する方針は維持する。ReflectionAccess相当のField一覧・読み書き処理は手書きでも提供できる。通常HeaderをGeneratorの対象に指定する方法は詳細設計事項とし、初期から任意のHeaderの再帰解析を必須にしない。

Engine Componentも今回の初月案では[[EditorCommandDesign]]のGetEditorValue / EditValueを使う。EditorはValueTreeへ操作を記録し、全Window描画後にEngine::Runtimeへ渡す。Engine内部SetterがTransform / Camera / PhysicsのValidationと副作用を担当する。専用のTransformEditCommandや直接SetTransformするUI経路を基本案にしない。

### 値ツリーの生成とキャッシュ
型Descriptorは型ごとに登録して共有する。Edit Modeの自動InspectorはSceneデータの値を基本とし、表示更新のためにScriptから値ツリーを毎フレーム取得しない。

以下の値ツリー取得・編集・寿命の方針は、自動Inspectorを利用するGame側Componentにも適用する。Play中は表示対象EntityのScript / Game側Componentだけ毎フレーム取得する。Engine Componentも同じ値取得・汎用EditValue経路を使う。

EditorのPlay中は、Inspectorで表示しているEntityのScriptから毎フレーム値ツリーを取得する。そのフレームの取得結果をInspector描画で使い回し、World内の全Enemy / Scriptの値ツリーを生成・保持しない。更新は対象Scriptへの書き込みと競合しないタイミングで行う。

Play中のInspector編集は対象の値ツリーをReaderでRuntime Scriptへ反映し、次フレームの取得で実際の値へ合わせる。PROPERTYのValidationやCUSTOM処理による変化もこの取得で反映する。全ScriptのDirty通知や変更追跡を必須にせず、選択変更、Entity / Script破棄、Play / Stop、World再生成、Game.dll Reload時は取得結果を破棄する。

Edit Modeでの変更はSceneデータへ反映し、必要なプレビュー更新をReader / PROPERTYのSetter経由でWorldへ反映する。保存対象外Fieldの表示値や、OnDrawInspectorが直接変更した値のSceneへの同期方法は詳細設計事項とする。

Scene保存・ロード等でScriptとの変換が必要な場合は順に処理し、変換用ツリーは処理後に解放する。保存済みSceneデータとは別に、全Runtime値のミラーを維持しない。配布ゲームにもSceneロード等の変換コストは発生するが、通常のScript更新ではInspector用の値ツリーを生成しない。

値ツリーには値のコピー、名前、配列・Mapの管理情報等のメモリが必要となる。保持対象を表示中のScript等に限定し、負荷が問題になった場合は割り当ての再利用や部分更新を詳細設計・実装時に検討する。

### CUSTOM Serializer
特殊な型や保存形式は手書きの変換処理を指定する。以下の構文は仮。

```cpp
struct HogeSerializer
{
    static bool Save(ValueWriter& writer, const Hoge& value)
    {
        return writer.BeginMap()
            && WriteField(writer, "b", value.b, Serialize)
            && WriteField(writer, "c", value.c, Serialize)
            && writer.EndMap();
    }

    static bool Load(ValueReader& reader, Hoge& value)
    {
        return ReadField(reader, "b", value.b, Serialize)
            && ReadField(reader, "c", value.c, Serialize);
    }
};

// Scriptのメンバ宣言
CUSTOM(Hoge, customHoge, HogeSerializer, Serialize);

// Generatorが生成する処理
HogeSerializer::Save(writer, enemy.customHoge);
HogeSerializer::Load(reader, enemy.customHoge);
```

この型をCUSTOMで扱う場合、FIELD一覧による自動変換に代えて指定Serializerを呼ぶ。保存・復元用のC++処理はGame.dll内で動き、Engineは他のFieldと同じWriter / Readerの中間表現を扱う。

### OnDrawInspector
自動Inspectorで足りない表示・操作はScriptのOnDrawInspectorで書く。

```cpp
void Enemy::OnDrawInspector(InspectorUI& ui)
{
    ui.DrawDefaultInspector();
    ui.DragFloat("Custom B", customHoge.b);

    if (ui.Button("Reset"))
        ResetSettings();
}
```

以下のCUSTOM Inspectorは後続の制作機能候補。初月はDescriptorからUIを生成し汎用EditValueへ集め、Game Callbackが実体を直接書く経路を導入しない。

Editorが登録されたGame側のInspector Callbackを呼び、ScriptがInspectorUIを通してUIを要求する。InspectorUIは共通HeaderのC++型としてEditor実装へ直接委譲できる。Engine / Editor定義型への参照やSTL値を許容し、POD + 関数テーブルを必須形式にしない。実際のImGui呼び出しはEditor側で行う。

CUSTOM Serializerは保存・復元、OnDrawInspectorは表示・編集を担当する。初期ラッパーはDragFloat、Checkbox、InputText、Button等、必要な操作から用意する。Editor用のCallbackとAPIはEditor構成に限り、GameプロファイルへEditor実装依存を持ち込まない。

### 一括登録
各generated.hのReflectionAccessを呼ぶため、GeneratorはGameReflection.generated.cppも生成する。対象HeaderのincludeとScriptごとの登録呼び出しをまとめる。

```cpp
#include "Enemy.generated.h"
#include "Player.generated.h"

void RegisterGameReflection()
{
    Reflection::Access<Enemy>::Register();
    Reflection::Access<Player>::Register();
}
```

Game.dllの登録入口から呼び、Editor起動時とReload後にDescriptorとCallbackを再登録する。独自型の構造はScriptのTypeDescから参照でき、独自型をScriptインスタンスとして登録する必要はない。

Game側Archetype ComponentのDescriptorと値操作Callbackも、Component登録時に同じ登録APIへ渡す。登録コードを手書きする場合も、Game.dllの登録入口から呼ぶ。

## Internal C++ DLL BoundaryとExternal Stable Plugin ABI
exe / Engine.dll / Game.dllの間は同一ビルドのC++境界。GameはPublic Facadeのstatic / template APIとPublic型のPointer / Referenceを使い、API libがEngine Subsystemを直接呼ぶ。STL所有入力に必要な境界変換は内部へ隠す。生成・破棄Entry Pointだけextern "C"にすることもできる。

Assets::Load<T> / Get<T> / Register<T>は自然なstatic template APIとして利用する。GetはEngine所有Pointerの借用、Registerは所有入力を消費してEngineが登録データを所有する。API lib内の限定変換は[[FacadeBoundarySketch]]。AssetをGame側でdeleteせず、将来の破棄 / ReloadでPointerが失効し得ることを明示する。共有Runtime・Build Configurationの一致が前提。

Game固有型は必要に応じてScriptStorage内で直接連携する。Engine側Native ECSへ任意非自明Game Componentを置く型操作登録は後続とし、初月はEngine既知trivial型とSparse Scriptに絞る。これは初月のECS実装範囲の判断であり、同一ビルドのC++データ受渡しを禁止するものではない。

External Stable Plugin ABIは外部SDK・異言語・異なるCompiler / Runtime・独立配布の要件が出た時に別設計する。現在のGame / Editorに長期互換や必須Function Tableを課さない。

### Game.dllのHot Reload
この節は後続の制作機能。初月は起動時Load・終了時Unloadを基本とし、実行中差替えや状態移行を完成条件にしない。
Editor起動中はEdit Modeでも`Game.dll`をロードし、Game側Component / System / ScriptとReflection情報を登録する。これによりScene表示やInspectorがGame側の型情報を使える。
初期Editorには明示的な`Build Game`操作を用意する。ファイル保存監視による自動Buildは初期実装に含めず、将来File Watcherを追加する場合も同じ`BuildGame()`処理を呼ぶ。
```text
Build Game
↓
必要ならScene Save
↓
Game.vcxproj Build
  ├─ .cscript Generator
  ├─ Incremental Compile
  └─ Game.dll生成
```

このBuild内訳はGenerator導入後のもの。初期は通常のGame.dllプロジェクトをIncremental Compile / Linkする。Build成功後のみReloadする方針は共通とする。
Buildに失敗した場合は、現在ロード中の`Game.dll`、Scene、Reflection Registryを維持する。Build成功後にのみReloadへ進む。
初期Hot ReloadではRuntime State Migrationを行わない。Scene / World内のGame側状態を破棄し、新DLLから登録した型情報でSceneを再生成する。
```text
Game Build成功
↓
Game.dll由来のJob完了を待つ
↓
Scene / World内のGame状態を破棄
↓
Component操作情報 / System / Script / Reflectionを登録解除
↓
旧Game.dll unload
↓
新Game.dll load
↓
Component / System / Script / Reflection再登録
↓
SceneからWorldを再生成
```

DLLをUnloadする前に、Game.dll由来のFunction Pointerや型操作CallbackをRegistry等からすべて解除する。Component LayoutやScript Fieldの変更も旧状態をMigrationせず、Scene再生成で反映する。
WindowsのDLLファイルロックを避けるため、MSVCのBuild出力`Game.dll`を直接Loadすることには依存しない。必要になった場合はShadow Copyを使える構造にする。
```text
Game.dll
↓
Game_HotReload_XXX.dllへコピー
↓
LoadLibrary
```
Shadow Copyの具体的な方式はReload実装時に決める。

### Editorの境界
EditorのコードはEditorプロファイルのexeだけに含める。DefineだけでなくEditor / ImGui / backendソースと依存libもGameプロファイルから除外する。初期Editorはゲーム制作に必要な範囲に留め、コード編集はVisual Studioで行う。
配布する`Game.exe`は完成したゲームを実行し、`Editor.exe`は制作環境を起動する。Editor内のPlayはEditorプロセス上でRuntime Worldを実行する形とし、次のPlay / Stop方針に従う。

Edit / PlayでWorldを二つ常駐させず、基本的に単一Worldを使う。
```text
Play:
Scene Save
↓
World内Entityを破棄
↓
Sceneから再生成
↓
Game実行開始

Stop:
Game実行停止
↓
World内Entityを破棄
↓
保存済みSceneから再生成
↓
Edit Modeへ戻る
```

exe内EditorはPlay中も維持する。Play中もScene View / Game View / Inspector / Debug Window等からRuntime状態を確認できる。Play中のScene構造編集やUndo / Redoは初期機能の必須要件にしない。

## 実装順序
GameCodeGeneratorを先行実装しない。

通常のC++と限定した対応型の手書き登録からゲーム制作を始める。基本型の自動Inspector、CUSTOM、必要な専用Inspectorを先に使い、.cscriptや独自型の再帰自動生成は後から導入する。

まず以下を完成させる。
```
Entity / Component Registry
↓
Archetype / Chunk / Query
↓
System
↓
Sparse Script
↓
Script Storage / Lifecycle
↓
Engine.dll / Game.dllの共有C++ Header
↓
Game側Component / System / Reflection登録
↓
Gameプロジェクト構成確定
↓
GameCodeGenerator
```
Sparse ScriptはGenerator完成前は普通の、
```
Enemy.h
Enemy.cpp
```
として実装してよい。

Sparse ScriptとReflectionの必要な設計が固まってから、既存の`.h/.cpp`を`.cscript`から生成する形へ置き換える。GeneratorはEngine基盤完成後に実装し、未確定のEngine設計を先行して埋め込まない。
## 方針まとめ
```
Game.vcxproj (Utility)
├─ Source一覧
├─ GameCodeGenerator
└─ GeneratedGame.vcxproj Build
      ↓
GeneratedGame BeforeTargets="ClCompile"
├─ 通常の.cpp
└─ Generated/**/*.generated.cpp
      ↓
    MSVC
      ↓
   Game.dll
```

- ゲームコードは `Assets/` 以下へ自由配置
- `Game.vcxproj` をゲームソース一覧の正とする
- Sparse Scriptのみ `.cscript` からコード生成
- Archetype Component/Systemは普通のC++
- GeneratedGameは裏側のビルド専用プロジェクト
- 共通コンパイル設定は `GameBuild.props`
- Incremental Buildを維持する
- GeneratorはEngine ToolとしてGit管理
- Generator実装はEngine / ECS / Sparse Script基盤完成後
- Edit ModeでもGame.dllとReflection Registryを利用
- Build成功後のみGame.dllをReloadし、SceneからWorldを再生成
