# SceneSystemの設計

Engineは `SceneSystem` を所有する。`SceneSystem` は現在のSceneと保存・切り替え要求をAssetルートからの相対パスで管理する。実行中のEntityは既存の単一 `ECSWorld` に置き、Sceneの切り替えでは全Entityを入れ替える。

[Sceneの保存・読み込み](Serialize/SceneSerialization.md) ・ [ECSの設計](ECS.md) ・ [Scriptの設計](GameRuntime/Script.md)

## パスと要求

| 操作・状態 | 契約 |
| --- | --- |
| `currentScenePath` | 最後に復元が完了したSceneのパス。初回の復元までは空とする。 |
| `SaveScene(path)` | Engine側の開発機能が指定した保存先を保持し、保存要求のフラグを立てる。フラグが立っている間に繰り返し呼んでも、次のフレーム末尾に保存するのは1回とする。 |
| `LoadScene(path)` | `.scene` の相対パスで `nextScenePath` を更新する。処理前に複数回呼ばれた場合は、最後に指定されたパスを使う。 |

保存とロードはそれぞれの処理を始める時点でフラグやパスを取り出す。`nextScenePath` が空の場合は切り替えを省き、パスが現在と同じ場合もファイルを読み直してWorldを再構築する。

## フレーム末尾での処理

描画後にEngineが次の順に呼ぶ。

1. `GameRuntime::EndFrame()` で破棄対象のScriptを除去し、保留中のScript変更と保存用の逆引き索引を反映する。
2. `ECSSystem::EndFrame()` で構造変更とEntityの破棄を確定する。
3. `SceneSystem::SerializeScene()` は保存要求のフラグを確認する。フラグが立っていれば解除し、確定済みWorldのスナップショットを保存する。フラグが立っていなければ関数から戻る。
4. `SceneSystem::CheckScene()` は `nextScenePath` の有無を確認し、要求があれば切り替える。

## 切り替え

1. `CheckScene()` が対象の `.scene` を読み込み、Sceneの構造とComponentデータを検証する。
2. `GameRuntime::ClearSceneScripts()` が、旧Entityの実体がある間にScriptの `OnDestroy` を呼ぶ。Script実体と `EntityHandle → Storageインデックス列` を空にし、型の登録とGUID索引を保持する。この終了処理の間は旧SceneへのEntity生成とScript追加を受け付けない。
3. `ECSSystem::ClearSceneEntities()` が全EntityとComponentの実体を破棄する。旧Handleを無効にするため、使用中だったSlotの世代を進める。ComponentとProcessの登録を保持したWorldを、新Sceneの展開に使う。
4. 全ルートを展開し、親子関係とActive設定を整えてからScriptを復元する。追加の反映を済ませたら `currentScenePath` を更新し、読み込み用のスナップショットを解放する。新Sceneの更新は次のフレームから始まる。

全体の破棄は切り替え専用の操作とする。旧Sceneの全Entityが対象で、切り替えをまたいで残るEntityは初版の保存・復元契約に含めない。

`.scene` の読み込みやEntity展開、登録済みScriptの生成・反映に失敗した場合は、Sceneのパスと失敗理由をログに出し、`ApplicationAPI::RequestQuit()` で終了を要求する。終了時は通常のEngine終了処理で残っている実体を解放する。GUIDに対応するScript型が未登録の場合は個別のScript参照のエラーとして扱い、そのScriptだけを省く。

## 起動Scene

初版の起動パスは `Engine::Initialize()` 内に `Assets/` 基準の相対パスとして記述する。Asset・ECSを初期化し、GameRuntimeの生成とScript型の登録を済ませてから、`SceneSystem` で最初のSceneを復元する。起動時は最初の更新前に完了させ、失敗は `Engine::Initialize()` の失敗として返す。[Engineの起動実装](../../Engine/Source/Engine.cpp)

起動用 `.scene` から初期配置とScriptを復元できるようになった時点で、手書きの `CreateDebugScene()` と `SetupScene()` を外す。現在 `CreateDebugScene()` が行う描画のView・Projection設定は、起動Sceneの表示に必要な描画初期化として扱う。将来的には起動パスの取得元を `ProjectSettings` に移す。

## 初版の確認項目

- 複数ルート、子階層、非ActiveのEntity、GUID付きScriptが保存と復元で一致する。未登録GUIDが混ざっている場合は、当該Scriptのエラーを記録したうえで残りを復元する。
- 新規保存したSceneを同じ起動中に読み込める。現在のSceneと同じパスを指定しても、ファイルを読み直して再構築する。
- 保存を繰り返し要求してもフレーム末尾の書き出しは1回とする。ロードを繰り返し要求した場合は最後に指定されたパスを読み込む。
- 切り替えではScriptの `OnDestroy` が旧Entityの解放より先に呼ばれ、旧Handleが無効になる。保存済みSceneの構造が不正な場合は、エラーを記録して終了処理へ進む。
