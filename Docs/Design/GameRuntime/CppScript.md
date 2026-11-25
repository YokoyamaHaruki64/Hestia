# C++ Script のコード生成

GameCodeGenerator は `Assets/` 内の Script 用 `.cpp` と隣接する `.cpp.meta` から、Game のコンパイル用 C++ コードと Script の型登録を生成する。`.cpp.meta` の GUID は Scene に保存する永続参照に使う。

GameCodeGenerator は MSBuild からの単発生成と、CLI からの保存監視で利用する。両方の経路で生成先と差分キャッシュを共用する。

[Script の実行と寿命](Script.md) ・ [GameCore と Game の分担](GameDLL.md#gamecore-と-game-の分担) ・ [AssetメタデータとMetaTool](../Asset/MetaTool.md)

## 入力形式

Script 用 `.cpp` は `Assets/` 配下の任意のフォルダに置き、`Game.vcxproj` に登録する。登録した `.cpp` は IntelliSense の解析対象にし、C++ コンパイルは生成側の `.cpp` に対して行う。型名の既定値は拡張子を除いたファイル名で、`HestiaGame` 内の型を指す。別名やネストした名前空間の指定には `SCRIPT_TYPE(Type)` を使う。宣言と実装をそれぞれ `HEADER_BEGIN` / `HEADER_END` と `SOURCE_BEGIN` / `SOURCE_END` で囲む。 [Script の入力設定](../../../Props/GameScripts.props)

Generatorは独立した行に書かれたディレクティブとブロックの構造を解析する。各ブロックのC++はそのまま出力し、型名とソースの整合はC++コンパイル時に確認する。実装をヘッダーに含める場合は、実装ブロックを空にできる。

```text
HEADER_BEGIN
#include "Common/Include/EntityHandle.h"

namespace HestiaGame
{
    class Enemy
    {
    public:
        void Update(Hestia::EntityHandle self);

    private:
        unsigned int m_updateCount = 0;
    };
}
HEADER_END

SOURCE_BEGIN
void HestiaGame::Enemy::Update(Hestia::EntityHandle)
{
    ++m_updateCount;
}
SOURCE_END
```

## ビルド時のデータフロー

1. MSBuild は `Game.vcxproj` の `ClCompile` から、`Assets/` 配下の `.cpp` を `GameScripts.props` の `_GameCppScript` で選ぶ。`Assets/` 起点の相対パスをプロジェクトの登録順で入力一覧に出し、GameCodeGenerator の `--project <input-list>` に渡す。
2. ビルド前は `MetaTool.exe --ext .cpp` でメタデータの検証と補完を行い、GameCodeGenerator で生成物を揃える。監視と共通の生成処理を使い、入力の照合から生成物とキャッシュの確定まで、同じプロジェクトへの書き込みを直列化する。`.meta` の補完もこの書き込み順序に含める。
3. Game プロジェクトは生成完了後、現在の登録一覧に対応する生成ソースと `GameSetup.generated.cpp` を `ClCompile` でコンパイルし、GameCore とリンクする。DLL 構成では `Game.dll`、`Test` 構成では `Game.lib` に組み込む。

MetaTool と GameCodeGenerator は、各ツールのフォルダに置いた `Release|x64` の exe を直接起動する。いずれかが見つからなければ MSBuild はビルドエラーを報告する。Game の Debug・Release と Editor・Game の四構成、および既存の `Test` 構成は維持する。

## 保存監視

1. Visual Studio の Developer PowerShell でリポジトリルートから `Tools/GameCodeGenerator/GameCodeGenerator.exe --watch` を起動する。MSBuild の `-getItem:_GameCppScript` でビルド時と同じ対象一覧を取得し、初回生成する。監視で一覧を取得する際の構成は `Debug_Editor|x64` とする。`Game.vcxproj` や `GameScripts.props` の保存後は一覧を取り直す。 [MSBuild の項目取得](https://learn.microsoft.com/en-us/visualstudio/msbuild/evaluate-items-and-properties)
2. `ReadDirectoryChangesW` で `Assets/` 配下と、プロジェクト・プロパティファイルがあるディレクトリを監視する。登録対象の `.cpp` と対応する `.cpp.meta` の保存、一時ファイルからのリネームを拾い、連続する通知は最後の通知から約 200 ms を目安にまとめる。`.cpp.meta` の通知は末尾の `.meta` を外し、対応する Script のパスに結び付ける。ディレクトリの移動や通知の取りこぼしがあれば、現在の登録一覧とファイルの状態を照合し直す。 [Windows のディレクトリ変更通知](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw)
3. 登録済み Script のうち `.meta` が欠けたものがあれば、MetaTool で補完してから同じ生成処理を呼ぶ。変更を検出した Script はキャッシュの更新時刻とサイズが一致しても読み直す。生成中に受けた変更通知は次回の生成に回す。

Script の追加・削除・移動を生成対象に反映するのは、`Game.vcxproj` の登録が更新されたとき。移動先のファイルが存在し、プロジェクトが新パスを指す状態で新しい出力を確定する。登録が旧パスを指す間は入力不足として報告する。`.meta` も一緒に移動すれば既存の GUID を引き継ぐ。

監視は `Ctrl+C` で終了する。入力エラー時は理由を表示し、次の保存で再試行する。開発用 `Init` からの起動はホットリロードの導入時に接続する。初版の監視が行うのは生成コードの更新まで。

## 生成物とScript登録

| 生成物 | 内容 |
| --- | --- |
| `Enemy.generated.h` | `Enemy.cpp` の宣言部分 |
| `Enemy.generated.cpp` | 対応する生成ヘッダーの include と実装部分 |
| `GameSetup.generated.cpp` | `HestiaGame::Setup::RegisterScripts()` の実装 |

`RegisterScripts()` は生成ヘッダーを参照し、入力一覧の順に `World::RegisterScript<T>(guid)` を呼ぶ。GameRuntimeのStorage実行順が型登録順に従うため、登録順は `Game.vcxproj` の入力順で決める。アタッチ可能なScript型の登録にはGUIDを必須とする。 [Setup の宣言](../../../Game/Include/GameSetup.h) ・ [GameRuntimeの実行順](../../../Game/Source/GameRuntime.cpp)

Scene保存では、Entityに付いているScriptの登録GUIDを `{guid, localId: 0}` で記録する。復元時はGameRuntimeがGUIDに対応する生成関数を引き、Entityの展開後にScriptを追加する。初版で保存するのは型参照であり、インスタンスは引数なしで生成して既定状態から始める。 [Sceneの保存・読み込み](../Serialize/SceneSerialization.md)

リポジトリ直下の `GeneratedScript/` を、監視・MSBuild・IntelliSense の共通出力先とする。Script ごとの生成物は `GeneratedScript/Scripts/` の下に置き、`Assets/` からの相対ディレクトリ構成を保つ。`GameSetup.generated.cpp` は `GeneratedScript/` の直下に置く。他の Script を参照する場合は `#include "GeneratedScript/Scripts/Gameplay/Enemy.generated.h"` のように記述する。

`GeneratedScript/` と差分キャッシュは Git の管理対象から外す。オブジェクトファイルとビルド追跡情報は構成ごとの `$(IntDir)` に置く。初版の同期範囲は生成処理の完了までとし、コンパイル中にさらに保存した場合の入力固定は今後の課題とする。

## 差分キャッシュ

`Cache/GameCodeGenerator/ScriptInputs.cache` にプロジェクト共通のキャッシュを永続化し、監視とビルド前の単発生成から利用する。キーは正規化した `Assets/` 起点の Script の相対パスとする。各項目に `.cpp` と `.cpp.meta` それぞれの更新時刻とサイズ、および解析済みの型名と GUID を記録する。キャッシュ全体に形式の版、Generator の識別情報、Asset ルートを記録する。

| 入力や生成物の状態 | 処理 |
| --- | --- |
| 同じパスで、両入力の時刻とサイズが一致し、生成した `.h` と `.cpp` が揃っている | 保存済みの型名と GUID を使う。元の `.cpp` と `.meta` の再読込・解析を省く |
| 新規パス、変更通知あり、入力情報の差分あり、または生成物が欠けている | `.cpp` と `.meta` を読み、型名・GUID・生成コードを作り直す |
| 登録から外れたパス | 生成成功後に、対応する生成物とキャッシュ項目を取り除く |
| 登録順だけ変わった | 個別のキャッシュ項目を再利用し、`GameSetup.generated.cpp` を登録順に更新する |
| キャッシュの欠損・破損、または形式・Generator・Asset ルートの変更 | 全入力を読み、キャッシュを作り直す |

新しい登録一覧を取得しても、同じパスの有効な項目は再利用する。移動・リネームは旧パスの削除と新パスの追加として扱う。型名・GUID の重複はキャッシュから採用した項目も含めて検証し、`GameSetup.generated.cpp` は現在の登録順から組み立てる。

## 出力の確定と診断

- 入力全体の検証が成功してから出力を反映する。`WriteChanged` の内容比較を維持し、変更箇所だけ一時ファイルから置き換える。不要分の整理は `GeneratedScript/Scripts/` 内の生成物に限り、キャッシュがない場合も現在の登録一覧から判断する。生成と整理が完了してからキャッシュを保存する。出力処理が途中で失敗した場合は、キャッシュを無効にし、次回に全件を照合する。
- 生成したヘッダーとソースの Script 部分には `#line` を付け、コンパイル診断が元の `.cpp` のパスと行を指すようにする。
- 入力の不足や不正、型名や GUID の重複、出力失敗は、パス、取得できる場合は行番号、理由を出力する。単発生成は非ゼロで終了し、MSBuild は C++ コンパイルを開始しない。監視は生成エラーを報告し、次回の通知を待つ。

## 実装時の確認項目

- 保存直後に対象の生成物が更新され、IntelliSense と通常ビルドの両方で同じヘッダーを解決できる。
- 無変更の再実行では Script 本文を読み直さず、生成ファイルの更新時刻も維持する。実装だけの編集では、変更のあった `.generated.cpp` だけを更新する。
- 追加・削除・移動・登録順の変更を反映できる。フォルダ移動後にプロジェクトを更新すると、新パスで生成されて旧出力が整理される。
- `.meta` の変更や重複、キャッシュ破損、生成ファイルの欠損を扱える。監視と MSBuild が同時に生成を要求しても、出力とキャッシュを順に確定できる。
