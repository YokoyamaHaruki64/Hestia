# AssetDB
アセット名とファイルの対応をYAMLで管理し、名前からロードするための草案。初期はAsset Browserを作らず、ファイル配置とDBの編集を手作業で行う。


## Public Facadeと内部Subsystem
Game.dllはAssetsの型付きC++ APIを使い、その実装をGameAPI.libへリンクする。Engine::AssetDBは登録Map / Cache / Loader接続を持つSubsystem実体であり、中継Serviceではない。Engine内部はその実体を直接使う。Public AssetはSTLを含んでもよく、所有入力の限定変換だけをAPI lib内へ隠す。GetはEngine所有の借用Pointer、Register(T&&)は入力を消費しEngineが登録結果を所有する。具体的な内部View / コピー案とLifetimeは[[FacadeBoundarySketch]]・[[AssetSceneSketch]]を参照する。

## 基本方針
- アセット名はDB全体で一意。Typeごとに同名を許可する構成にはしない。
- 編集元はYAML。AssetDBは起動時に読み込み、登録情報をメモリに常駐させる。
- 起動中のDB更新は明示的な手動Reloadで反映する。変更検知による自動Reloadは後回し。
- 名前変更とファイル移動は利用者が参照・パスを修正する。自動追跡や参照の自動修復は初期に作らない。
- 重複、不正な型、存在しないパスを確認する小さな検証Toolを用意する。

アセット管理UIや自動追跡の開発を抑え、ゲーム制作を始めやすい構成にする。

## 登録内容
アセット名をYAML上のキーとして、型とパスを記述する。起動時にTypeごとの検索Mapへ振り分ける。以下のパスと拡張子は例であり、実際のアセット形式を確定するものではない。

読み込み時にアセット名を内部的に小文字へ統一する。検索時も要求文字列を同じ規則で小文字化するため、PlayerMesh / playermesh / PLAYERMESHは同じアセットを指す。小文字化後に同じ名前になる登録は重複エラーとする。これはアセット名の扱いであり、登録パスを小文字へ書き換える処理ではない。

~~~yaml
PlayerMesh:
  type: Mesh
  path: Models/Player.mesh

PlayerFaceMaterial:
  type: Material
  path: Materials/PlayerFace.material

PlayerFaceTexture:
  type: Texture
  path: Textures/PlayerFace.png
~~~

pathはAssetsルートからの相対パスとして扱う。DBファイルの配置場所と登録するTypeの一覧は詳細設計時に決める。
名前変更はコードやScene等の参照にも影響する。ファイルだけ移動した場合は名前を維持し、DBのpathを手動で直す。GUIDやリネーム履歴、旧名からの自動変換は初期に用意しない。

## 名前からのロード
利用側はファイルパスを直接指定せず、アセット名を指定する。API名は仮。
~~~cpp
auto mesh = Assets::Load<Mesh>("PlayerMesh");
auto material = Assets::Load<Material>("PlayerFaceMaterial");
~~~

~~~text
要求されたアセット名を小文字化
→ 要求Type用のCache Mapを検索
→ Cache Hitなら既存Handleを返す（手動登録Assetも含む）
→ Cache Missなら要求Type用のDB Mapで名前と登録Typeを確認
→ 登録パスからType別Loaderへ渡し、Cache Mapへ登録
→ アセットのHandle等を返す
~~~

型別Loader、Cache、利用側APIの構成は [[AssetManagement]] にまとめる。

アセット名はDB全体で一意に検証する。YAMLを読む際は全Typeを通した名前集合で正規化後の重複を先に検出し、その後、TypeごとのMapへ登録する。

AssetDB Mapには名前・パス等の登録情報を保持し、TypeごとのCache MapにはHandleに対応する管理レコードとロード済みのアセット本体を保持する。DB MapとCache Mapは別にし、DB Reload時も既存キャッシュを維持する。DBの常駐は全アセット本体を起動時にロードすることを意味しない。
不明な名前や型不一致はエラーとして扱う。キャッシュを利用する場合も要求型との整合を確認する。`Load<T>`の戻り値は`AssetHandle<T>`とする。Handleの寿命、エラーの伝達方法、同期 / 非同期ロードは [[AssetManagement]] で検討する。

Cache Mapのキーは小文字へ統一したアセット名の文字列とする。同じ名前は全Typeで一意なので、各TypeのCache Mapでその名前を使える。初期はファイル読み込み等のロードコストを優先して削減し、名前検索が実測で重い場合に名前の事前ハッシュ化等を検討する。キャッシュHit時にはファイル読み込みが発生しないため、その経路の検索コストは別に計測する。

手動作成したAssetは `Register<T>(AssetName, Asset)` でCacheに登録し、YAMLには追記しない。名前の正規化と一意性検査はファイル由来の登録と共通にする。手動登録名とYAML上の名前の衝突はエラーとし、DBの手動Reloadでも同じ検査を行う。登録したAssetのGPUアップロードについては [[AssetType]] を参照する。
## 起動と手動Reload
起動時にYAMLを読み、検証して名前から検索できるDBを構築する。DBのメモリは起動中維持し、LoadのたびにYAMLを読み直さない。
起動時にDBが存在しない場合はLogへエラーを出力する。不正な内容も検証エラーとして報告する。エラー後の起動継続 / 停止の扱いは詳細設計時に決める。
初期のReloadはDBの登録情報を更新する操作とする。
EditorのImGuiにReload AssetDB操作を用意する。変更検知による自動Reloadは初期に行わない。

~~~text
YAMLを手動編集
→ Reload AssetDBを実行
→ 新しいDBを一時領域へ読み込み・検証
→ 成功した場合だけ現在のDBと差し替え
~~~

Reloadに失敗した場合はエラーを表示し、現在の有効なDBを維持する。差し替えはDBの検索等と競合しないタイミングで行い、利用側は古いDB内のレコードへの参照・ポインタを保持し続けない。
手動Reload後もロード済みアセット本体と各TypeのCache Mapはそのまま維持し、再ロードや無効化を自動では行わない。DBのpathを変更してもキャッシュHit時は既存データを返し、Cache Miss時は更新後の登録情報からロードする。新しいファイル内容を既存アセットへ反映する操作は初期対象に含めない。
Scene等に残った旧名や、起動中に削除された登録への参照を自動修復しない。初期は検証結果を見て利用者が修正する。

## 検証Tool
初期の検証対象は次の範囲に絞る。

- YAMLの構文と必須項目。
- 小文字化後の名前について、DB全体での重複。
- 未対応・不正なType。
- 登録パスのファイルが存在するか。

YAMLのキー重複が通常のMap構築で上書きされないよう、読み込み時点で検出する。DBの読み込みと検証Toolで同じ判定を使い、名前と問題箇所を表示する。
Sceneやゲームコード内の名前参照をすべて自動解析するToolは初期対象にしない。Typeと実ファイル内容の整合、依存アセットの検証は必要になった段階で追加を検討する。

## 規模が増えた場合
一覧編集がYAMLでは扱いづらくなったら、GoogleスプレッドシートからAssetDB用YAMLを書き出すToolを作る。

~~~text
スプレッドシートで編集
→ 書き出しTool
→ 同じYAML形式
→ 検証Tool
→ 手動Reload
~~~

導入時はスプレッドシートを編集元、YAMLを生成物として扱い、二つを並行して手編集する運用にしない。ゲームはローカルのYAMLを読むため、起動やLoadのたびにGoogleへの接続を必要としない。
Asset Browserや変更検知による自動Reloadも、制作上の必要性が出た段階で検討する。

## 未確定事項
- アセット名の命名規則と、小文字化の具体的な文字の扱い。
- DBの配置場所、Type一覧、各Loaderへ渡すファイル形式。
- 起動時のDBエラーをLogへ出した後、起動を継続するか停止するか。

## 関連資料
- [[HestiaArchitecture]]
- [[EditorArchitecture]]
- [[AssetManagement]]
