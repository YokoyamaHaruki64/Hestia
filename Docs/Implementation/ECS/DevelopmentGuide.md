# ECS ComponentとProcessの開発手引き

[処理・構造](README.md) / [型](Types.md) / [Process実行](Process.md)

Engine内部ECSを利用して、具体的なComponentとProcessを追加する際の手順と判断基準をまとめる。型・寿命・反映時点の詳細は上記の実装仕様、書式は[コーディング規約](../../CodingStandard.md)を参照する。

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

1. privateのECSSystem::RegisterECSで、Componentを名前空間付きのUTF-8名とともにRegisterComponentへ登録する。
2. Component登録が成功した後、m_world.RegisterProcess<Function>(phase)でProcessを登録する。
3. Component登録のoptionalとProcess登録のboolを確認し、失敗したらRegisterECSからfalseを返す。

同じ関数を別phaseへ重複登録することはできない。同じphase内の依存順は登録順で表す。登録名はComponent IDの生成元になるため、登録名を変更した場合の保存互換性は保証されない。

| phase | Engineからの呼び出し位置 |
| --- | --- |
| FixedUpdate | 固定時間の更新後。Applicationの蓄積時間に応じて通常フレームの前に0回以上 |
| FrameBegin | 通常フレームのTime更新後、最初のphase |
| Update | FrameBeginの後 |
| LateUpdate | Updateの後 |
| PreRender | LateUpdateの後、既存描画の前 |

通常操作はECSSystemのstatic APIを使う。WorldのGetterや外部登録コールバックを追加する必要はない。

## 5. Entityへ適用する

- CreateEntityは必須6 Componentで生成する。AddProcessで不足する要求Componentを追加し、通常ComponentはGetComponentで値を設定・編集する。追加時に既存Componentの値は維持される。 [必須Component](README.md#所有と初期化)
- Process実行外の生成・追加・削除は即時反映される。Process実行中は、現在のProcessの全Chunk処理が終わってから、次のProcessの前に要求順で反映される。
- 生成待ちHandleへのGetEntity／GetComponentはnullptrになる。生成直後に追加要求は出せるが、反映前に値へアクセスできないことを踏まえて初期値設定の手順を決める。
- RemoveProcessは対象Processの要求から、必須Componentと現在一致する他Processの要求を除いて削除する。固有Componentがなければ対象Processも一致したままになるため、常にそのProcessを停止するAPIとしては扱わない。
- 生存中EntityへのDestroyEntityは破棄予約を設定し、IsActiveはその時点でfalseになる。activeの設定値は変えず、実体はEndFrameで破棄する。生成待ちなら構築せず取消となる。
- EntityHandle::IsValidは形式の判定であり、Entityの生存を保証しない。取得結果を確認し、長期保持したHandleは使う時点で解決する。

初期Component列／Process列を指定する生成API、Prefab、OnAttach／OnStartは未実装。[将来案](../../OpenDecisions.md#将来案ecsの初期構成指定とprefab復元)を実装済みの入口として使用しない。

## 6. エラーと完了条件を確認する

通常の不正操作や登録失敗はLoggerと戻り値で通知する。自らthrowする処理を通常の制御フローに持ち込まず、標準コンテナごとのtry/catchを追加しない。現在の例外境界と確保例外の制約は[エラーと未実装範囲](README.md#エラーと未実装範囲)を参照する。

Component構築に失敗した構造変更は、移行元を変更せず拒否される。Process関数の例外は記録後に保留要求を反映して現在のRunを中断するが、既に編集したComponent値は戻らない。noexcept関数からの例外はこの境界では回復しない。

実装した機能に応じて、次の点を確認する。

- 必要なComponentだけを要求し、共有データのRead／Writeとphase・登録順が意図に合っている。
- デフォルト値と初期値設定の手順が、Processの一致・生成保留の契約と矛盾しない。
- 公開APIで観測できる更新結果、非active時の扱い、追加・削除時の値保持、必要な寿命・失敗経路を検証する。
- [既存のGoogleTest基盤](../../../Test/README.md)で関係する範囲を確認し、VerifyProjectでEngineをビルドする。検証した条件と未確認範囲を記録する。

基盤APIの変更が必要になった場合は、利用側で迂回処理を増やす前に、必要な操作・所有・反映時点を整理してECS本体の変更として検討する。
