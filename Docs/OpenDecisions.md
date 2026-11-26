# 未決事項

実装に着手する前に、決定内容を `Docs/Design/` の正本へ反映する。

| 分野 | 決めること |
| --- | --- |
| Editor | 編集対象、描画との接続、構成ごとの配置、初期化・終了順を決める |
| Game から Application への操作 | FPS 設定や終了要求など、用途に応じた公開範囲と API を決める |
| Game 向けサブシステム | Audio、Physics の利用ケースと公開操作を決める |
| Input の拡張 | ゲームパッドと操作割り当ての公開範囲を決める。キーボードとマウスは [InputSystem の設計](Design/InputSystem.md) に定める |
| Asset の拡張 | Game からの Material 作成・Property 操作、個別アンロード時の Handle と CPU／GPU 資源の寿命を決める |
| 描画の拡張 | RenderGraph、描画スレッド、Record の分割を機能要件と計測結果から決める。初期 Pass は [RenderPass](Design/RenderPass.md) に定める |
| Transform の並列化 | 更新起点、親子間の順序、書き込みと描画参照の同期点を決める。現行設計は [Transform](Design/Transform.md) に定める |
| ProjectSettings | [Sceneの起動設定](Design/SceneSystem.md#起動scene)を移す段階で、設定ファイルの形式と読み込む境界を決める |
| Script の増分ビルド | `GameSetup.generated.cpp` が全 Script の生成ヘッダーを取り込むため、1つのヘッダーを変更するたびに全 Script のヘッダーを再解析する。Script 数が増えた段階で増分ビルド時間を計測し、型登録コードの分割が必要か判断する。[Script 登録の設計](Design/GameRuntime/CppScript.md#生成物とscript登録) |
| Script インスタンスの保存 | 型 GUID の復元に加えて、保存する値と既定値を決める。[Scene の保存形式](Design/Serialize/SceneSerialization.md) |
| Game.dll のホットリロード | CLI 監視の起動を開発用初期化に接続する段階で、再ビルドと DLL の入れ替え、Script の状態の扱いを決める。[保存監視](Design/GameRuntime/CppScript.md#保存監視) |
| 描画インスタンスの色 | 現在 `MeshRenderer` が保持する InstanceColor の最終的な管理先と保存方法を決める。[MeshRenderer](Implementation/ECSProcess/MeshRenderer.md) |
| DLL の独立配布 | 別バージョンを接続する必要が生じた時点で ABI の検査と互換範囲を定める |

## 将来案：ECS 初期構成と Prefab

Prefabの初版は、Entityの親子関係、Component構成、選択したフィールドの初期値をYAMLに保存する。スナップショットの取得、YAML入出力、`ECSSystem::Instantiate` によるWorldへの展開は[Componentのシリアライズ](Design/Serialize/ComponentSerialization.md)に定める。Process実行中に展開を要求した場合の扱い、PrefabでのScript型の保存・復元、展開元を追跡する `PrefabInstance` は今後決める。
