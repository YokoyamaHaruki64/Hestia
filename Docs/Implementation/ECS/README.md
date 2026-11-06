# ECSの処理・構造

[全体索引](../README.md) / [型](Types.md) / [Process実行](Process.md) / [開発手引き](DevelopmentGuide.md)

根拠: [ECSSystem.cpp](../../../Engine/Source/ECS/ECSSystem.cpp)、[ECSWorld.cpp](../../../Engine/Source/ECS/ECSWorld.cpp)、[Archetype.cpp](../../../Engine/Source/ECS/Archetype.cpp)、[Chunk.cpp](../../../Engine/Source/ECS/Chunk.cpp)

## 所有と初期化

EngineがECSSystemを所有し、ECSSystemがComponentRegistryと単一ECSWorldを所有する。WorldはRegistryを借用し、ArchetypeとProcessを所有する。Archetypeは共通LayoutとChunk群を所有する。LayoutのComponentInfoはRegistryへの非所有参照なので、終了はWorld → Registryの順になる。

既定Chunkサイズは16KiB。ECSSystem.InitializeはprivateのRegisterECSを呼び、Name、Tag、Layer、Transform、TransformCache、TransformHierarchyをUTF-8の型名で登録してからWorldを初期化する。登録関数を外部から渡す仕組みと、System／Worldを返すpublic Getterはない。通常のEntity操作はECSSystemのstatic APIからWorldへ委譲する。

Engine内のComponentとProcessの登録はRegisterECSへ追加する。Componentを先に登録し、m_world.RegisterProcess<Function>(phase)の結果を確認する。現在は上記6 Componentが必須で、TransformProcessをPreRenderに登録する。World単体では初回のEntity生成・Run・EndFrameまで登録を受け付けるが、ECSSystemの通常APIに動的Process登録口はない。 [Transformの実装仕様](../ECSProcess/Transform.md)

## 記憶構造

ArchetypeSignatureはComponentTypeIdを昇順に揃えた集合。ArchetypeはEntity列を先頭に置き、その後に型別SoA列を配置する。各列のalignmentとpaddingを含めてChunkに収まる最大行数を二分探索する。登録されていない型、重複型、0容量、1Entityも収まらない配置は構築失敗となる。

Chunk内ではEntity列と各Component列の同じrowが同じEntityを表す。Worldのslot表はHandleをArchetype/Chunk/rowへ解決する。移行・行詰め時にslotも更新するため、EntityとComponentの生ポインタを構造変更越しに保持しない。

## Entityの生成と破棄

CreateEntityはslotとHandleを予約し、必須6 Componentを持つArchetypeへ生成する。初期値はactiveがON、NameがEntity{index}、TagとLayerが0で、Transform系の初期値はTransformの仕様に従う。Process実行外では即時構築し、実行中はPendingCreateのHandleを返す。反映前のGetEntity/GetComponentはnullptr。生成待ちへの追加・削除はCreateの後に要求順で適用する。 [Transformの初期値](../ECSProcess/Transform.md#transform)

生成待ちへのDestroyは生成取消となる。FlushChangesは取消要求を先に処理し、Componentを構築せずslotを解放してgenerationを進める。これにより同じHandleのCreateと後続要求を無効化する。

生存中EntityへのDestroyは対象とTransformの子孫に破棄予約bitを設定する。activeの設定値を保持したままIsActiveはfalseを返し、実体とComponentはEndFrameまで残る。予約後の追加・削除と保留済みの構成変更は適用しない。EndFrameで親の子一覧から参照を外し、実体を破棄する。同じChunkの末尾行で穴を埋め、移動したEntityのslotを更新する。解放したslotのgenerationは0を飛ばして進める。古いHandleは世代照合で拒否し、全世代周回後の再一致は許容する。 [Transformの破棄](../ECSProcess/Transform.md#transformhierarchy)

## 構造変更

AddProcessは不足する要求Componentを追加し、既存値を維持する。EntityにProcess所属一覧は持たせず、Component構成だけで実行対象を決めるため、別Processの条件も揃えばそのProcessも対象になる。Processが要求しないComponentも保存する。

RemoveProcessは対象Processの要求から、必須Componentと現在一致する他Processの要求を除いて削除する。保護対象は全登録Processではなく、変更前のArchetypeと一致するものだけ。固有Componentがなければ構成が変わらず、削除対象Processも一致したままになる。

移行は次の順序で行う。

1. 移行先ArchetypeとChunkの空き行を用意し、Entityの識別子と状態bitを複製する。
2. 不足Componentを先に既定構築する。失敗したら構築済みの不足分と移行先Entityだけを片付ける。移行元とslotは変更せず、逆Moveは行わない。
3. 構築成功後、共通Componentをnoexcept Moveし、削除対象を含む移行元のComponentを破棄する。
4. 移行先行を確定してslotを更新し、元行をSwap Removeする。行順は保証しない。

Process実行中は生成・追加・削除をChangeRequestに積み、現在のProcessが全Chunkを処理し終えた後に反映する。Entity破棄の確定時点はそれとは別のEndFrame。構造変更中の借用取得や再入処理には状態による制限がある。並列ジョブ実行の仕組みは実装されていない。

EngineはTime更新後に各phaseを呼ぶ。FixedUpdateでは同名phase、FrameExecuteではFrameBegin → Update → LateUpdate → PreRender、描画後にEndFrameを実行する。終了時はECSをAsset／Graphics／Timeより先に解放し、Logは最後まで利用できる。

## エラーと未実装範囲

通常の不正操作はLoggerと戻り値で通知する。Componentの既定構築は例外を許容し、Move構築と破棄はnoexceptを要求する。Componentの構築・Move・破棄からECS操作を呼ばないことを前提とする。生成失敗では予約slotを解放し、構造変更失敗では元Entityを維持する。要求ごとに失敗を処理し、後続要求は継続する。

Process登録、ArchetypeのProcess接続、構造変更要求、Component構築、Process実行に例外境界がある。ECSSystem.Initialize自体はRegisterECSをtry/catchで囲まない。標準コンテナの確保例外を全APIで捕捉する設計ではなく、CreateEntityのslot予約やProcess実行外の破棄キュー確保からは例外が伝播し得る。

Game向けにはTransform操作のFacadeを公開している。汎用のGame向けECS API、OnAttach／OnStart、並列ジョブ、Script、Prefabと初期構成指定の生成APIは未実装。 [Transform Facade](../ECSProcess/Transform.md#hestiagametransform) ・ [生成APIとPrefabの将来案](../../OpenDecisions.md#将来案ecsの初期構成指定とprefab復元)

## 検証記録（2026-11-05）

| 対象 | 結果 |
| --- | --- |
| VerifyProject -Project Engine | Debug_Editor・x64で成功 |
| RunTests -Filter 'ECSFixture.*:ECSSystem.*:ComponentRegistry.*' | Test・x64で19件成功 |
| RunTests全体 | 80件中75件成功。Engine生成を前提とする4件がnull、LogSystemの並列出力件数1件が512対256で失敗。原因は未確定 |

構築失敗テストは、移行元の値と構成を維持し、Move回数が増えないことを確認した。GPU描画・Window操作・四構成ビルドの成立はこの記録に含めない。
