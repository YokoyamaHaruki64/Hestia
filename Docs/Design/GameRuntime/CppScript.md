# C++ Script のコード生成

[Script の実行と寿命](Script.md) ・ [GameCore と Game の分担](GameDLL.md#gamecore-と-game-の分担)

## 入力形式

`.cppsc` は、ゲームロジックの Script を1ファイルで編集するための形式。宣言と実装をそれぞれ `HEADER_BEGIN` / `HEADER_END` と `SOURCE_BEGIN` / `SOURCE_END` で囲み、内側に通常の C++ を書く。Script は具象型として定義し、GameRuntime が対応するコールバックを検出する。

```text
HEADER_BEGIN
#include "Common/Include/EntityHandle.h"

class Enemy
{
public:
    void Update(Hestia::EntityHandle self);

private:
    unsigned int m_updateCount = 0;
};
HEADER_END

SOURCE_BEGIN
void Enemy::Update(Hestia::EntityHandle)
{
    ++m_updateCount;
}
SOURCE_END
```

## 生成物

GameCodeGenerator は `Game.vcxproj` に登録された `.cppsc` を入力として扱う。Script ごとのヘッダー・ソースと、ゲーム全体で使う Setup を同じ生成処理で出力する。

| 生成物 | 内容 |
| --- | --- |
| `Enemy.generated.h` | `Enemy.cppsc` の宣言部分 |
| `Enemy.generated.cpp` | 対応する生成ヘッダーの include と実装部分 |
| `GameSetup.generated.cpp` | `HestiaGame::Setup::RegisterScripts()` と `SetupScene()` の実装 |

`RegisterScripts()` は生成された Script のヘッダーを参照し、登録対象の各型について `World::RegisterScript<T>()` を呼ぶ。 `SetupScene()` はゲームの初期配置データに従って対象 Entity を決め、`World::AddScript<T>()` で関連付ける。配置対象の指定方法とデータ形式は未決定。GameCore は既存の Setup の契約に従い、型登録、初期配置、保留中の Script の反映を順に進める。 [Setup の宣言](../../../Game/Include/GameSetup.h)

## ビルドと診断

- 生成した `.cpp` は Game プロジェクトで通常の C++ としてコンパイルする。 `GameSetup.generated.cpp` が手書きの `GameSetup.cpp` を置き換え、GameCore とリンクして `Game.dll` を作る。
- 生成先では入力の相対ディレクトリ構成を維持する。出力内容に変更がなければ書き換えを省き、増分ビルドで再コンパイルする範囲を抑える。
- 生成したヘッダーとソースの Script 部分に `#line` を付け、コンパイル診断が元の `.cppsc` のファイル名と行を指すようにする。
