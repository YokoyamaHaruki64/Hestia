# SceneSystemの設計

`SceneSystem` は単一 `ECSWorld` の現在の Scene と保留要求を静的に管理し、パスは Asset ルートからの相対パスで保持する。Scene の切り替えでは全 Entity を入れ替える。Engine の終了時は GameRuntime を破棄してから Scene の状態をリセットする。

[Sceneの保存・読み込み](Serialize/SceneSerialization.md) ・ [ECSの設計](ECS.md) ・ [Scriptの設計](GameRuntime/Script.md)

## パスと要求

| 操作・状態 | 契約 |
| --- | --- |
| `currentScenePath` | 最後に復元が完了したSceneのパス。初回の復元までは空とする。 |
| `SaveScene(path)` | Engine側の開発機能が指定した保存先を保持し、保存要求のフラグを立てる。パス省略時は `currentScenePath`、最後に保存が成功したパス、自動命名の順に使う。保存要求が複数あっても、次のフレーム末尾に保存するのは1回とする。 |
| `LoadScene(path)` | `.scene` の相対パスで `nextScenePath` を更新する。処理前に複数回呼ばれた場合は、最後に指定されたパスを使う。 |

保存とロードはそれぞれの処理を始める時点でフラグやパスを取り出す。`nextScenePath` が空の場合は切り替えを省き、パスが現在と同じ場合もファイルを読み直してWorldを再構築する。

Editor の `File/SaveScene` は現在の Scene パスに保存する。パスが空の場合と `File/SaveSceneAs...` では保存ダイアログを開き、選択した `.scene` の Asset ルート相対パスで保存を予約する。ダイアログをキャンセルした場合は要求を残さない。新規保存・別名保存後も現在の Scene パスはロード済みの Scene を指す。

`File/OpenScene...` は読み込みダイアログで選んだ `.scene` の Asset ルート相対パスを `LoadScene` に渡す。現在の Scene はフレーム末尾の復元が成功した時点で更新する。

## Game への公開

Game Script は `HestiaGame::Scene` の3つの操作を使う。`SceneAPI` は対応する `SceneSystem` の静的関数を GameRuntime に渡す。Scene の保存要求は Engine 側で扱う。

| 操作 | 契約 |
| --- | --- |
| `GetCurrentSceneName()` | 現在のパスのファイル名から拡張子を除いた名前を返す。名前が同じ Scene はパスで区別できる。 |
| `GetCurrentScenePath()` | 復元が完了した最新の Scene の `Assets/` 基準の相対パスを返す。 |
| `LoadScene(path)` | `.scene` の相対パスを検証し、受理できればフレーム末尾の切り替えを予約して `true` を返す。読み込みと復元の結果はフレーム末尾で確定する。 |

現在の名前とパスは初回の復元まで空で、切り替え要求を受理した時点では変化しない。取得した `std::string_view` は Scene の切り替え完了か GameRuntime 終了まで借用でき、それ以降も使う場合はコピーする。

## フレーム末尾での処理

描画後にEngineが次の順に呼ぶ。

1. `GameRuntime::EndFrame()` で破棄対象のScriptを除去し、保留中のScript変更と保存用の逆引き索引を反映する。
2. `ECSSystem::EndFrame()` で構造変更とEntityの破棄を確定する。
3. `SceneSystem::SerializeScene()` は保存要求のフラグを確認する。フラグが立っていれば解除し、確定済みWorldのスナップショットと保存先を `AssetSystem::Save` に渡す。フラグが立っていなければ関数から戻る。 [YAML Asset の保存](AssetSystem.md#yaml-asset-の保存)
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

起動用の `Scenes/Startup.scene` は生成役のEntityとScript参照を保持する。`DebugCubeGenerator::OnStart` が組み込みCubeとファイル由来の共有Materialを取得し、立方体を生成する。描画のView・ProjectionはEngineの起動処理で設定する。将来的には起動パスの取得元を `ProjectSettings` に移す。 [起動Scene](../../Assets/Scenes/Startup.scene) ・ [生成Script](../../Assets/Gameplay/DebugCubeGenerator.cpp)

## 初版の確認項目

- 複数ルート、子階層、非ActiveのEntity、GUID付きScriptが保存と復元で一致する。未登録GUIDが混ざっている場合は、当該Scriptのエラーを記録したうえで残りを復元する。
- 新規保存したSceneを同じ起動中に読み込める。現在のSceneと同じパスを指定しても、ファイルを読み直して再構築する。
- 保存を繰り返し要求してもフレーム末尾の書き出しは1回とする。ロードを繰り返し要求した場合は最後に指定されたパスを読み込む。
- 切り替えではScriptの `OnDestroy` が旧Entityの解放より先に呼ばれ、旧Handleが無効になる。保存済みSceneの構造が不正な場合は、エラーを記録して終了処理へ進む。
