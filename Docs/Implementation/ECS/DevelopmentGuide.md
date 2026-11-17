# ECS ComponentとProcessの開発手引き

[処理・構造](README.md) / [型](Types.md) / [Process実行](Process.md)

Engine 内部の ECS に Component と Process を追加する手順をまとめる。Game から操作する Process は、同じ手順の中で API と Facade を接続する。型・寿命・反映時点の詳細は上記の実装仕様、書式は [コーディング規約](../../CodingStandard.md) を参照する。

## 1. Processの目的から必要なデータを決める

1. 実現する処理、入力、更新結果を決める。
2. 実行するphaseと、前後のProcessとの順序を決める。
3. 読むデータと書き換えるデータを列挙し、既存Componentを利用できるか確認する。
4. 新しいデータが必要なら、意味・寿命・共有範囲に応じてComponentを定義する。

Processの要求を設計の出発点とする。1つのProcessが複数Componentを組み合わせ、1つのComponentを複数Processが共有してよい。Process固有の設定や状態も、Entityごとに保持する必要があればComponentとして表す。

処理対象はComponent構成から自動的に決まる。EntityにProcess所属一覧はなく、別Processの要求が揃った場合もそのProcessの対象になることを前提にする。

## 2. Componentを定義する

- 状態・設定・参照先を明確にし、デフォルト値を決める。生成後の初期値設定を前提にする場合も、設定前にどのProcessと一致するか確認する。
- デフォルト構築可能で、Move構築と破棄がnoexceptの型にする。非自明型やMove-only型も扱える。所有資源がある場合は、Move後の所有権と破棄を型側で正しく管理する。
- Componentの構築・Move・破棄からECS操作を呼ばない。構造変更中の再入やEntity取得を前提にした初期化は行わない。
- 外部資源への参照は、所有者と有効期間を決める。Entityへの長期参照にはEntityHandleを使い、構造変更で移動するEntity／Componentの生ポインタを保持しない。
- Name／Tag／LayerとTransform／TransformCache／TransformHierarchyは全Entityに存在する必須Component。TransformのLocal値と階層を変更するときは、dirty伝播や親子参照を更新する専用操作を使う。 [Transformの操作入口](../ECSProcess/Transform.md)

Componentのデータを独自にバイトコピーして移行しない。構築・Move・破棄とSoAへの配置はRegistry／World／Chunkへ任せる。

## 3. Process関数を実装する

- free関数またはstatic関数を使い、引数を `void(size_t count, const Entity* entities, Components*...)` の形式にする。
- 読み取りだけのComponent引数はconstポインタ、更新するものは非constポインタにする。constはRead、それ以外はWriteとして記録される。
- 要求Componentはすべて揃っている前提で処理する。同じ型の重複要求はconst違いも含めて拒否される。要求していないComponentが存在していても実行対象になる。
- 1回の呼び出しは1つのChunkを扱い、Entity配列と各Component配列の同じ添字が同じEntityを表す。渡されたcountの範囲を処理する。
- 配列には非active・破棄予約済みの行も含まれる。activeだけを対象とする処理では、行ごとにIsActiveを確認する。
- ChunkやEntityの行順を処理結果の前提にしない。Entity／Componentへの借用ポインタは構造変更をまたいで保持しない。

現在は逐次実行で、Read／Writeから実行順や並列ジョブが自動生成される仕組みはない。

## 4. 登録と実行順を接続する

新しいHeader／Sourceは既存のEngine配置とinclude規則に従い、Engineプロジェクトとフィルターへ登録する。Componentの型定義とProcess関数の宣言をECSSystem.cppから参照できるようにする。

1. `ECSSystem::RegisterECS` で、Component を名前空間付きの UTF-8 名とともに `REGISTER_COMPONENT(Type, TypeName)` で登録する。
2. Component 登録後に、Engine 内で使う Process を `REGISTER_PROCESS(Func, Phase)` で登録する。Game に公開する Process は、以下の手順で `REGISTER_GAME_PROCESS(API, Func, Phase)` を使う。

各マクロは登録に失敗した場合にログを記録し、`RegisterECS` から `false` を返す。 [ECS の登録箇所](../../../Engine/Source/ECS/ECSSystem.cpp)

同じ関数を別phaseへ重複登録することはできない。同じphase内の依存順は登録順で表す。登録名はComponent IDの生成元になるため、登録名を変更した場合の保存互換性は保証されない。

| phase | Engineからの呼び出し位置 |
| --- | --- |
| FixedUpdate | 固定時間の更新後。Applicationの蓄積時間に応じて通常フレームの前に0回以上 |
| FrameBegin | 通常フレームのTime更新後、最初のphase |
| Update | FrameBeginの後 |
| LateUpdate | Updateの後 |
| PreRender | LateUpdateの後、既存描画の前 |

Engine 内からの通常操作は `ECSSystem` の static API を使う。

### 公開ProcessのAPIとFacade

1. `Engine/Include/API/ECSProcess/` に `XxxAPI.h` を置き、Game に公開する個別操作を `Hestia::XxxAPI` の関数ポインタとして宣言する。`Engine/Source/API/ECSProcess/XxxAPI.cpp` の `XxxAPIImpl::CreateAPI()` で Engine 内の関数を割り当てる。個別操作は `EntityHandle` から Component を解決する。
2. `Engine/Include/Game/` に Facade を置く。クラス内の `using API = Hestia::XxxAPI;` で対応する型を指定し、`World` から受け取った `EntityHandle` と API ポインタを保持する。Facade のメソッドは保持した Handle を渡して関数ポインタを呼ぶ。
3. `ECSSystem::RegisterECS()` に `REGISTER_GAME_PROCESS(XxxAPI, &XxxProcess::Run, Phase)` を追加する。`ECSSystem` は ECS の登録が成功した時点で `ECSProcessBinding<XxxAPI>` に `ProcessHandle` を設定する。同じ公開 API への再登録は拒否する。

現行の `PreRender` では、以下の順序で Transform の更新と描画提出を登録している。 [ECS の登録箇所](../../../Engine/Source/ECS/ECSSystem.cpp)

```cpp
REGISTER_GAME_PROCESS(TransformAPI, &TransformProcess::Run, PreRender);
REGISTER_GAME_PROCESS(MeshRendererAPI, &MeshRendererProcess::Run, PreRender);
```

`Common.vcxproj` のビルド中、PowerShell の生成器が `Engine/Include/API/ECSProcess/` 直下の `*API.h` をファイル名順に処理する。`ECSProcessBinding<XxxAPI>` の集約メンバ、型ごとの `Member` オーバーロード、`XxxAPIImpl::CreateAPI()` を呼ぶ初期化コードを `API/Generated/` に生成する。内容が同じ場合は既存ファイルを再利用する。 [生成スクリプト](../../../Tools/APIGenerator/GenerateECSProcessAPI.ps1) ・ [ビルドの生成ターゲット](../../../Common/Common.vcxproj) ・ [集約の入口](../../../Engine/Include/API/ECSProcessAPI.h)

`ProcessHandle` は Engine の登録済み Process 配列の添字。EntityAPI は追加・検索時に範囲を確認し、解決した Process の呼び出し口を ECS に渡す。 [型付き登録](../../../Engine/Include/ECS/ECSSystem.h) ・ [EntityAPI の実装](../../../Engine/Source/API/EntityAPI.cpp)

## 5. Entityへ適用する

- CreateEntityは必須6 Componentで生成する。AddProcessで不足する要求Componentを追加し、通常ComponentはGetComponentで値を設定・編集する。追加時に既存Componentの値は維持される。 [必須Component](README.md#所有と初期化)
- Process実行外の生成・追加・削除は即時反映される。Process実行中は、現在のProcessの全Chunk処理が終わってから、次のProcessの前に要求順で反映される。
- 生成待ちHandleへのGetEntity／GetComponentはnullptrになる。生成直後に追加要求は出せるが、反映前に値へアクセスできないことを踏まえて初期値設定の手順を決める。
- RemoveProcessは対象Processの要求から、必須Componentと現在一致する他Processの要求を除いて削除する。固有Componentがなければ対象Processも一致したままになるため、常にそのProcessを停止するAPIとしては扱わない。
- 生存中EntityへのDestroyEntityは破棄予約を設定し、IsActiveはその時点でfalseになる。activeの設定値は変えず、実体はEndFrameで破棄する。生成待ちなら構築せず取消となる。
- EntityHandle::IsValidは形式の判定であり、Entityの生存を保証しない。取得結果を確認し、長期保持したHandleは使う時点で解決する。

初期 Component 列／Process 列を指定する生成 API と Prefab は ECS の将来案にある。Script の `OnStart` は GameRuntime が Script の追加時に呼ぶ。 [ECS の将来案](../../OpenDecisions.md#将来案ecsの初期構成指定とprefab復元) ・ [Script の実行](../GameRuntime/README.md)

### Gameから公開Processを操作する

`World` は Facade の `API` 型に対応する集約メンバをコンパイル時に選ぶ。同じ登録から取得・追加・検索に必要な情報を得る。 [World の公開 API](../../../Game/Include/World.h)

| World の操作 | 呼び出し経路 |
| --- | --- |
| `GetProcess<T>(entity)` | 登録の API ポインタと `EntityHandle` から Facade を値で作る。未接続や未登録の場合は無効な Facade を返す。操作時に Engine が Component を解決するため、保持した Facade は Archetype の移動をまたいで使える |
| `AddProcess<T>(entity)` | `ProcessHandle` を EntityAPI に渡し、要求 Component を追加する。既存 Component の値は維持する |
| `FindEntitiesWithProcess<T>()` | 同じ `ProcessHandle` で、要求 Component の構成に一致する Entity を検索する |

個別操作の使い方は [Transform](../ECSProcess/Transform.md) と [MeshRenderer](../ECSProcess/MeshRenderer.md) を参照する。

## 6. エラーと完了条件を確認する

通常の不正操作や登録失敗はLoggerと戻り値で通知する。自らthrowする処理を通常の制御フローに持ち込まず、標準コンテナごとのtry/catchを追加しない。現在の例外境界と確保例外の制約は[エラーと未実装範囲](README.md#エラーと未実装範囲)を参照する。

Component構築に失敗した構造変更は、移行元を変更せず拒否される。Process関数の例外は記録後に保留要求を反映して現在のRunを中断するが、既に編集したComponent値は戻らない。noexcept関数からの例外はこの境界では回復しない。

実装した機能に応じて、次の点を確認する。

- 必要なComponentだけを要求し、共有データのRead／Writeとphase・登録順が意図に合っている。
- デフォルト値と初期値設定の手順が、Processの一致・生成保留の契約と矛盾しない。
- 公開APIで観測できる更新結果、非active時の扱い、追加・削除時の値保持、必要な寿命・失敗経路を検証する。
- [既存のGoogleTest基盤](../../../Test/README.md)で関係する範囲を確認し、VerifyProjectでEngineをビルドする。検証した条件と未確認範囲を記録する。

基盤APIの変更が必要になった場合は、利用側で迂回処理を増やす前に、必要な操作・所有・反映時点を整理してECS本体の変更として検討する。
