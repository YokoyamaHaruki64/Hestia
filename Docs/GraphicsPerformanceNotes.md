# Graphics 初期実装の性能観測メモ

記録日：2026/10/09

## 目的と位置づけ

最小限の描画・テクスチャ表示が通った段階で、ユーザーの手動計測から見つかった CPU 側の負荷候補を記録する。正式なベンチマークや改善効果の検証ではなく、後で計測・改善するときの参考資料とする。

数値はユーザーの報告値であり、今回こちらで再計測していない。原因候補と改善案は確定事項ではない。この資料の作成に伴う実装変更は行わない。

関連設計：[GraphicsSystem](../Reference/HestiaDesign/DevelopNotes/Class/GraphicsSystem.md)、[GraphicsFrameFlow](../Reference/HestiaDesign/DevelopNotes/GraphicsFrameFlow.md)

## 観測結果

| 対象 | ユーザー報告の一回あたりの時間 | 観測・原因候補 |
|---|---|---|
| BindMaterial 内の Commit | 約 4 µs | descriptor コピーの負荷が大きい可能性 |
| SetupMaterial と呼称したキー作成・検索経路 | 当初の観測：約 5〜10 µs | 追加計測では、文字列連結によるキー作成が大半を占めた |
| 上記経路の map 検索単体 | 約 300〜400 ns（0.3〜0.4 µs、1 µs 未満） | 検索自体の負荷はキー作成より小さい |
| Submit | 平均約 3.5 µs、観測最大約 5 µs | 提出データの値コピーが大きい可能性 |

時間の単位は表の各値に記載する。1 µs = 1,000 ns。最大値は今回の観測範囲内の値であり、上限保証ではない。「毎回必ず 4 µs」などの固定コストとも確定しない。

この三つは負荷候補として優先的に調べる価値がある。ただし、フレーム全体の占有率や GPU 時間との比較は未確認で、Graphics 全体のボトルネックと断定しない。

## 現在のコードとの対応

確認対象は `bdc768a` の後の作業ツリー。計測用の未コミット変更を含むため、その commit だけでは今回確認したコードを再現できない。

| 報告上の呼び方 | 確認した関数 | 留保 |
|---|---|---|
| BindMaterial の Commit | `DX12Backend::CommitMaterialTexture` | descriptor コピー以外に handle 計算と Root Descriptor Table 設定も含む |
| SetupMaterial と呼称したキー作成・検索経路 | `DX12Backend::SetupPipeline` が有力な対応候補 | パスを連結したキー作成と検索を分けて記録する。`BindMaterial` にも別の名前検索があり、関数名の対応は要確認 |
| Submit | `GraphicsSystem::Submit` と `Engine::FrameExecute` の呼び出し側 | 呼び出し側のタイマーには `RenderData` の構築と破棄も含む |

参照コード：[DX12Backend.cpp](../Engine/Source/Graphics/DX12Backend.cpp)、[GraphicsSystem.cpp](../Engine/Source/Graphics/GraphicsSystem.cpp)、[Engine.cpp](../Engine/Source/Engine.cpp)、[DX12Object.h](../Engine/Include/Graphics/DX12Object.h)

### 1. Material Texture の Commit

現在の `BindMaterial` は、Material 定数の配置、Texture の各スロットへの設定、`CommitMaterialTexture` を行う。Commit では最大 16 スロットを走査し、各有効スロットについて `CopyDescriptorsSimple(1, ...)` を呼び、Master Heap から Frame の Visible Heap へコピーする。

未使用スロットにも Null SRV の index を設定するため、テクスチャ一枚の Material でも、現在の経路では 16 件の descriptor コピーになり得る。これは画像ピクセルや Texture 本体の転送ではなく、SRV descriptor のコピーである。

**原因候補**：一件ずつの descriptor コピー呼び出し、handle の計算、Root Descriptor Table の設定。約 4 µs の内訳は未確認。`BindMaterial` 全体の時間を Commit 単体の時間として扱わない。

**次回の切り分け**：スロット設定、descriptor コピー、Root Table 設定を別々に集計する。実 Texture 数と Null スロット数も記録し、一枚・複数枚・同じ Material の連続描画を比較する。

**改善候補**：複数 descriptor を一回の API 呼び出しでコピーする方式、同一 Texture binding の Frame 内再利用。ただし、コピー元が連続しているか、Visible Heap の範囲と Frame Fence の寿命が維持できるかを確認してから判断する。未使用スロットの Null SRV を省いてよいとはしない。

### 2. 文字列連結によるキー作成と map 検索

現在の `SetupPipeline` は VS／PS の `filesystem::path` を値で受け取り、`vsPath.string() + "|" + psPath.string()` で文字列キーを作ってから `m_PipelineCache.find` を行う。cache hit 後にも `ComPtr` の代入がある。

**追加の観測結果**：ユーザーの切り分け計測では、文字列連結によるキー作成が時間の大半を占め、map 検索単体は約 300〜400 ns（0.3〜0.4 µs）だった。当初の 5〜10 µs を `unordered_map::find` 単体の負荷として扱う解釈を訂正する。キー作成単体の正確な時間は未報告であり、差し引きで確定値を作らない。

キー作成にはパスから文字列への変換、連結、一時文字列の生成が含まれる。引数のコピーと hit 後の参照カウント操作は別の区間として確認する。cache miss では Shader 読み込みと PSO 作成も加わり、通常の cache hit とは別の処理になる。

`BindMaterial` の名前キャッシュは別の map である。こちらも hit と miss を分ける。Pipeline の観測値をそのまま Material の名前キャッシュに適用しない。

**次回の切り分け**：キー作成側を優先し、パスの文字列化、連結、割り当てを分けて確認する。有効な Shader パスを使って cache を warm-up し、hit／miss 件数、キー文字列の長さ、cache 件数を記録する。

**改善候補**：キーを毎 Draw で生成せず登録時に保持する方式を優先候補とする。同じ Pipeline が続く場合の検索省略、将来の Shader／Material ID への移行も候補だが、map の種類を変える根拠にはしない。どれを採用するかは未決定。現在暫定採用している Material の名前キャッシュを、この観測だけで置き換えない。

### 3. Submit と提出データの構築

`Submit(RenderData renderData)` の内部は `m_renderQueue.push_back(std::move(renderData))` であり、queue への追加を明示的なコピーで行っているわけではない。それでも、呼び出し側で提出データを作るコストは残る。

現在の `RenderData` は World、Material、DXMesh、VS／PS パスを持つ。Material は名前文字列と Texture handle の vector、DXMesh は GPU Resource の `ComPtr` を持つ。呼び出し側が既存の Material・DXMesh から `RenderData{...}` を作る場合、Material のコピーや参照カウント操作が発生し得る。これは GPU Mesh／Texture 本体の全データをコピーする処理とは区別する。

確認時の `Engine::FrameExecute` は、10,000 回のループで `RenderData` を構築して Submit し、その前後を nanoseconds で計測している。そのため、約 3.5 µs の原因を Submit 関数内だけに限定しない。`std::move` を追加すれば全コストが消える、とも考えない。

**原因候補**：提出データの構築・破棄、Material 内の文字列／vector のコピー、`ComPtr` の参照カウント操作、queue の容量不足時の再確保。どれが主因かは未確認。

**次回の切り分け**：`RenderData` 構築と queue への追加を別々に集計する。queue の size／capacity、容量が増えた回数、初回と容量再利用後のフレームを分けて記録する。

**改善候補**：予測できる件数の容量確保、提出データの再利用、将来の Asset ID による提出データの縮小。提出元のデータを不用意に move して、その後の描画要求から値を失わせないこと。寿命が不明な参照に置き換える案も自動では採用しない。

## 描画件数による影響の目安

報告値が同じ条件で一件ずつ発生し、件数に比例すると仮定した単純な換算。実測フレーム時間ではない。

| 対象 | 1,000 件 | 10,000 件 |
|---|---|---|
| Commit：4 µs／件 | 4 ms | 40 ms |
| キー作成を含む経路：当初 5〜10 µs／件 | 5〜10 ms | 50〜100 ms |
| map 検索単体：0.3〜0.4 µs／件 | 0.3〜0.4 ms | 3〜4 ms |
| Submit：平均 3.5 µs／件 | 3.5 ms | 35 ms |

これらの数値は、計測区間に重複がないと確認するまで合算しない。キー作成を含む経路と、その内訳である map 検索単体を合算しない。同様に `BindMaterial` 全体と、その内部の Commit を二重に数えない。

## 計測条件と不足情報

現時点ではビルド構成、最適化設定、Debugger／DX12 Debug Layer の有無、CPU／GPU、実際の cache hit 率、フレーム数・サンプル数、平均値の算出方法が未記録。上記の一回あたりの報告値と、現在の計測用コードが完全に同じ条件だったかも未確認。

確認時のコードから、次の点に注意する。

- `Draw` 内では各呼び出しの時間を整数 microseconds に変換してから合計している。各回の小数部分が失われるため、合計時間を最後に変換する測り方とは値が異なる。
- `Draw` の出力値は各処理のフレーム内合計。一回あたりに換算する場合は、実際に計測した呼び出し件数を分母にする。
- Submit のループでは一件ごとに Logger を呼ぶ。Logger はその回のタイマー区間外だが、全体の処理時間や次回の計測条件への影響は残り得る。
- 確認時の Submit の VS／PS パスは空文字列。計測用の仮入力と考えられるが、そのままでは有効な Pipeline の cache hit を測っているとは限らない。失敗経路・初回作成・通常描画を混ぜない。
- CPU 関数の前後で測った時間は、その区間の CPU 経過時間。GPU の描画・転送完了時間や Frame 全体の性能を直接示すものではない。

## 次回の確認順序

1. 有効な描画データ・Shader パスを使用し、Release と Debug を混ぜずに条件を記録する。
2. warm-up 後の複数フレームで、合計時間、成功件数、cache hit／miss、queue の容量増加をまとめて記録する。一件ごとのログ出力は別の検証として分ける。
3. Commit、キー生成／検索、RenderData 構築／Submit を切り分け、原因候補に対応する時間を確認する。
4. 一つの変更候補だけを比較し、同じ件数・入力・条件で Frame 全体への効果と描画結果を確認する。

現在の目標は最小限の描画・テクスチャ表示。参考 Shader、既存の Vertex／Index Buffer 作成経路、暫定 TextureManager、Material の名前キャッシュは維持する。性能上の候補があることと、今その方式を差し替えることは別の判断とする。
