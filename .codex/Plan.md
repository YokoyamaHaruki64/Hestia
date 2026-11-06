# TransformProcessとDLL公開Facadeの実装計画

## 目的と実装範囲

必須TransformをECSに追加し、Local TRSを正本として、子孫へのdirty伝播・遅延更新・PreRender更新を実装する。Game向けFacade自身をEngine.dllから公開する方式をTransformで実証する。
今回追加するTransformの公開API・内部処理では、4×4行列をMatrix4x4_SIMDに統一する。
Rendererの作成、既存描画への接続、既存Facadeの移行、UI専用Transformは対象外とする。
この計画を.codex/Plan.mdに保存し、実装と検証を完了条件とするgoalを開始する。

## ComponentとECSへの組み込み

全Entityに既存のName／Tag／Layerと次の3 Componentを必須として付与する。

| Component | 保持するデータ |
| --- | --- |
| Hestia::Transform | Local位置・Quaternion・拡縮、SIMD版Local行列、LocalDirty／WorldDirty |
| Hestia::TransformCache | Matrix4x4_SIMDのWorld行列だけ |
| Hestia::TransformHierarchy | 親EntityHandleと子EntityHandleの一覧 |

- 初期値は位置0、回転Identity、拡縮1、行列Identity、親なし。初期Cacheは有効とする。
- 必須ComponentのID一覧を生成・取り外し保護で共用し、RemoveProcessでも残す。
- Transform独自のinactiveは設けず、Entityのactiveにかかわらずdirty伝播・更新を行う。
- 親の破棄要求で子孫全体を破棄予約し、実体はEndFrameで破棄する。生存する親の子一覧も整理する。
- 生成待ち、構造変更、Swap Remove、世代によるHandle無効化は既存契約を維持する。Componentの構築・破棄からECS操作は呼ばない。

## Facadeと公開契約

HestiaGame::TransformをEngine.dllから直接公開する。宣言をGameと共有し、実装はEngine側に置く。個別のTransformAPIやBind処理は追加しない。

- FacadeはEntityHandleだけを保持する非所有の値型とし、Handleから明示的に構築する。Componentポインタは保持しない。
- IsValidは生存状態を確認する。生成待ち・破棄予約済み・世代不一致・ECS未接続ではfalse。
- Local／WorldそれぞれのPosition・Rotation・ScaleのGet／Set、WorldMatrixのGet／Setを公開する。
- Getterは値を返す。公開数学型はVector3、Quaternion、Matrix4x4_SIMD。WorldMatrixのSetterもSIMD版を受け取る。
- GetParentは親を指すFacadeを返し、親なしは空のFacade。SetParent(parent, ParentMode)はKeepWorld／KeepLocalを指定でき、既定はKeepWorld。空の親指定は親解除。
- Setterはbool。無効な対象や不正な親指定はログとfalseで通知し、状態を変更しない。
- 無効FacadeのGetはログを記録してDebugでassertする。assert無効時は位置0・回転Identity・拡縮1・行列Identity・空の親を返す。IsValidはログもassertも行わない。
- 自分自身・子孫・破棄予約済みEntityへの親変更を拒否する。同じ親への再設定は何もしない。
- 内部Process向けにはComponent列の参照を渡せる操作入口を用意し、Facadeだけが操作ごとのHandle解決を行う。Localや階層の変更は共通操作を通してdirty通知と一体化する。

## 更新とWorld操作

- 行ベクトル、Local = S × R × T、World = Local × ParentWorld。4×4行列の合成・受け渡しにはSIMD版を使用する。
- Local変更では自身のLocalDirtyと自身・子孫のWorldDirty、親変更では自身・子孫のWorldDirtyを立てる。
- WorldDirtyが既に立ち、子孫への伝播も済んでいる枝は再走査を省く。
- World取得とPreRenderは共通内部処理を使う。dirtyな場合だけ親を先に更新し必要な行列を再計算する。cleanなWorld行列取得では親を辿らない。
- PreRenderはComponent列を走査しdirtyな行だけ再計算する。dirty Queueは初版では導入しない。
- WorldRotationは親子Quaternionの合成値。Setterは親のQuaternionからLocal回転を逆算する。
- WorldScaleは変換後の基底長を使う近似値。Local回転を親Worldで変換した基底と、対応するWorldQuaternion基底との向きで符号を決め、LocalScaleを掛ける。Setterも同じ係数で逆算し、係数0の成分は従来のLocalScaleを残す。
- WorldPositionのSetterは親の線形変換からLocal位置を求める。特異な親では元のLocalを基準に解ける部分だけ補正する最小二乗解を使い、失われた自由度のLocalを保持する。外部数学ライブラリは追加しない。
- KeepWorldは変更前のWorld位置・合成Quaternion・近似Scaleを取得し、新しい親の下で維持するようLocalを求める。
- WorldMatrixのSetterは有限なアフィン行列を対象としTRSへ近似して同じWorld操作へ渡す。回転抽出は基底の直交化を使い、潰れた基底では現在のWorld回転を補助にする。
- せん断の完全維持は保証しない。ゼロスケールによる潰れを許容し、逆算不能なLocal成分は元の値を残す。ゼロスケールだけを理由に失敗させない。
- Cacheは確定したLocal TRSから再計算し、分解前の行列を正本と異なる状態で残さない。

## 検証と完了条件

既存GoogleTest基盤に中程度の契約テストを追加する。汎用テスト基盤は新設しない。

- 必須3 Componentの生成・初期値・取り外し保護、構造変更後の値保持。
- Local変更、祖先変更、複数階層へのdirty伝播、Get時更新とPreRender更新の一致。
- inactiveを含む更新、Chunkをまたぐ親子、行移動後も有効なFacade。
- Local／World TRSとSIMD行列のGet／Set、Quaternion合成、非一様・負・ゼロスケール、せん断の近似。
- KeepLocal／KeepWorld、親解除、循環指定の拒否、親子孫の破棄予約とEndFrame後の無効化。
- ゼロスケール中の操作でNaN／Infを生じず、逆算不能なLocalを保持し、復帰後もその値が残ること。
- World行列だけを要求する読み取りProcessでCache列を直接利用できること。
- 既存ECSテストの登録型・期待構成・小Chunk容量を必要な範囲で修正。

最終検証:
1. Test/RunTests.ps1で全既存・追加テストを実行。実行フォルダーのSummary.mdを先に確認する。
2. VerifyProjectでEngineのDebug_EditorとRelease_Editorをビルド。assertとフォールバックを両構成で確認。
3. 外部consumerをEngine import libraryへリンクしSIMD行列を含むFacadeのDLL公開を確認。Releaseでは無効Facadeの既定値復帰も実行。
4. VerifyDocsを.codex/Plan.mdに限定して実行。

実装・テスト・DLLリンクの結果を記録し、描画・GPU・実際のGame.dllからの利用は未検証として区別する。既存設計文書の更新、コミット、pushは含めない。
## 実装・検証結果（2026-11-05）

### 実装済み

- 必須Transform／TransformCache／TransformHierarchyを登録し、生成・RemoveProcessの保護へ接続。
- 子孫dirty伝播、共通の遅延更新とPreRender更新、親子孫の破棄予約・EndFrameでの切り離し。
- HestiaGame::TransformをEngine.dllから直接公開。4×4行列は公開APIを含めMatrix4x4_SIMD。
- Local／World TRS、World行列、親操作を実装。せん断をTRSへ近似し、特異な親ではLocalの失われた自由度を保持。
- FacadeはHandleだけを保持。内部Process向けには列参照によるTRS変更入口を用意。
- Renderer／既存描画への接続、既存Facade移行、既存設計文書の変更は実施していない。

### 検証結果

- [全GoogleTestのSummary](../Test/Results/20261105-230616-9d33d156/Summary.md): ビルド成功、103件実行、98件成功、5件失敗、スキップ0件。
- 今回追加した23契約テストはすべて成功。Facade、親子更新、ゼロScale、Archetype移行、Cacheだけを要求する登録済みProcessのChunk横断読み取りを含む。
- [Engine Debug_Editor検証ログ](../Build/Verification/20261105-230422-188-Project-925abd48/): ビルド成功。
- [Engine Release_Editor検証ログ](../Build/Verification/20261105-230428-012-Project-831aaf65/): ビルド成功。
- [外部DLL consumer結果](../Test/Results/FacadeDll-20261105-230434-411b193b/results.json): 両構成でコンパイル・リンク・実行成功。Debugの無効Getはassertによる終了コード3、Releaseの無効GetはSIMD行列を含む既定値へ復帰。
- XMLプロジェクトとフィルターの構造を確認。
- 文書検証対象はこのファイルのみ。コマンド: Tools/Verify/VerifyDocs.cmd -Paths ".codex/Plan.md"。

### 範囲外の既存テスト失敗

[Failures.md](../Test/Results/20261105-230616-9d33d156/Failures.md)に根拠を保存。

- EngineAPI.CreatedEngineAcceptsCallbacksAndCanBeDestroyed
- EngineAPI.EngineCanBeRecreatedAfterDestruction
- TimeIntegration.EngineCallbacksUpdateOwnedClock
- LogSystemTest.EngineConnectsLoggerAndDestroyFlushesBeforeDisconnecting

上記4件は空のApplicationAPIから渡されるWindow Handleがnullptrであり、GraphicsSystem.InitializeがECS初期化より前で拒否する。Transformを使用する経路へ到達していない。

- LogSystemTest.FinalizeAfterConcurrentProducersSavesEveryAcceptedRecord

未変更のLogProcessはヘッダーと本文の2行を出力するため256記録で512改行になる。既存テストは256改行を期待している。
代表2件を単独実行して同じ失敗を確認した（Test/Results/20261105-225558-b8d84d9b）。今回の範囲外として修正していない。変更前バイナリでの実行比較は行っていない。

### 未検証・未実施

- 描画、GPU、実際のGame.dllからの利用は未検証。
- 実行中Game.dllによる有効EntityのFacade利用は、Static構成の機能テストと外部consumerのDLL境界検証を分けて確認した段階。
- コミット・pushは行っていない。