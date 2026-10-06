# Class Design Template
このファイルは `HestiaDesign/Class/` 配下で使用するクラス設計Markdownのテンプレート。
マスタークラスを先に記述し、そのクラスに付随するAPI・補助型を下位見出しとしてまとめる。
コード内に他の設計対象型が登場する場合は、コードブロック直下に参照リンクを置く。

## MasterClass
**役割**

マスタークラスの主な責務を1〜3行程度で記述する。

```cpp
class MasterClass
{
public:
    void Function();

private:
    RelatedAPI m_relatedAPI;
    ExternalType* m_externalType = nullptr;
};
```

[RelatedAPI](#relatedapi)  
[ExternalType](./Other.md#externaltype)

必要な設計上の補足があればここに記述する。

### RelatedAPI
**役割**

マスタークラスに付随するAPI・補助型の責務を記述する。

```cpp
struct RelatedAPI
{
    void Function() const;

private:
    void* m_context = nullptr;
};
```

[MasterClass](#masterclass)

必要な設計上の補足があればここに記述する。

### OtherSubClass
**役割**

マスタークラスに付随する別のAPI・補助型の責務を記述する。

```cpp
struct OtherSubClass
{
    RelatedAPI m_api;
};
```

[MasterClass](#masterclass)  
[RelatedAPI](#relatedapi)

必要な設計上の補足があればここに記述する。


## フォーマットルール
- `#` はファイル全体の設計単位に使用する。
- ファイル先頭には、このファイル全体の概要を2〜3行程度で記述する。
- `##` はマスタークラスに使用する。
- `###` はマスタークラスに付随するAPI・補助型に使用する。
- 補助型でも独立した設計単位として扱う場合は `##` に昇格させる。
- 各クラス・型は `**役割**` → 説明 → コード → 参照リンク → 補足の順で記述する。
- コード内に他の設計対象型が登場する場合は、コードブロック直下に参照リンクを置く。
- 同一ファイル内の参照は `[Type](#type)` とする。
- 別ファイルへの参照は `[Type](./File.md#type)` とする。
- `HWND`、`HMODULE` など標準API・外部ライブラリ由来の型には原則として参照リンクを付けない。
- C++識別子は [NamingConvention](../NamingConvention.md) に従う。
- 設計上必要のない説明や実装詳細は書きすぎない。
