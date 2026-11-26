# RenderPass
---
Engine内部の描画段階を `Build → Sort → Record` に分け、各段階で受け渡すデータを定義する。初版は既存のForward描画をこの構成へ移し、同じスレッドとCommandListで実行する。

ECS の `PreRender` は Transform、Camera、MeshRenderer の順に実行する。Transform が World 行列を確定し、Camera が描画用の View と Projection を設定し、MeshRenderer が `RenderData` を提出する。Graphics はそのフレームの提出列と2つの行列を Backend に渡す。

[Graphics の実装仕様](../Implementation/Graphics/README.md) ・ [MaterialShader](./MaterialShader.md)

## Camera

初版は有効な Camera Entity 1台を使う。`CameraComponent` は描画用の `view` と `proj` を保持する。`CameraProcess` は Transform の World 位置と回転から、カメラの拡縮を含まない View を計算し、既存の `GraphicsSystem::SetView()` と `SetProj()` に渡す。Projection は固定の透視投影設定で初期化する。

Scene は CameraComponent の有無と Transform を保存する。起動 Scene の Camera は従来の固定視点と同じ位置・向きで置く。Camera のない Scene では Graphics が保持している最後の View と Projection を使う。

## IRenderPass
---
**役割**

Engine側の論理的な描画段階を表す。Shader内のPassは、描画コマンドが選択するパイプラインの定義として扱う。

```cpp
class IRenderPass
{
public:
    virtual ~IRenderPass() = default;
    virtual void Build(std::span<const RenderData> submissions,
        const DX12Resources& resources) = 0;
    virtual void Sort() = 0;
    virtual void Record(std::span<const RenderData> submissions,
        const RenderPassRecordContext& context) = 0;
};
```

**処理順**

| 段階 | 入力と成果物 |
| --- | --- |
| Build | 固定された提出列を読み、対象を選別して解決済みの `DrawCommand` 列を作る。Sort Keyもこの時点で一度生成する |
| Sort | Passの規則でコマンド列を並べ替え、同じDrawで記録できる範囲を `DrawBatch` として確定する |
| Record | 確定した範囲を読み、PSO、定数、Descriptor、必要なインスタンスデータを設定し、CommandListへ記録する |

**所有・参照**

`DX12Backend` がフレームの提出列とPassの実体を所有する。各Passは作業用のコマンド列・バッチ列を持ち、フレームの記録が済んだら借用参照を捨てて配列の容量を再利用する。CPU AssetはAssetSystem、GPU資源はBackend内の所有者が保持する。Recordが転送したGPUデータはフレームのFence完了まで保持する。

初版のContextは同一スレッド内の借用参照で構成する。Job分割を導入する段階では、Buildの部分出力、Record範囲、記録先の貸出、Materialなどの描画入力を固定する契約を追加する。

## DrawCommandとDrawBatch
---
**役割**

`DrawCommand` は各Pass共通のBuild出力で、SubMeshとShader Passの組を一件表す。Sort Key、解決済みDXMesh、Material、MaterialShader、SubMesh番号、Shader Pass番号、元の提出番号を保持する。World行列や色は提出番号で参照する。Mesh提出を使わないPassも自身のRecordを実行できる。

`DrawBatch` はソート済みのコマンド列に対する開始位置と件数を持つ。通常描画も件数1の範囲とし、バッチ化は実際の描画状態が一致することを確認して行う。Sort Keyは並び順の決定に使い、キーの表現はRecordの契約に含めない。

## ForwardRenderPass
---
**役割**

提出1件につきGPU Meshを一度解決し、SubMeshとForward Shader Passの組に展開する。描画はOpaque、Masked、Transparentの順で記録する。

- OpaqueとMaskedはShaderとPass番号、Material、DXMesh、SubMeshの順にソートする。同値の場合は提出番号を使う。初版のキーはタプルで保持する。
- 同じShader Pass、Material、DXMesh、SubMeshを使う連続コマンドを、Shaderがインスタンシングに対応するときだけまとめる。連続Upload領域に収まる単位として、1 Drawは最大1024インスタンスとする。
- Transparentは提出順を維持し、コマンドごとに記録する。
- インスタンシング用Shaderには、範囲内の提出からWorld行列と色を詰めたBufferを渡す。通常のShaderは提出のWorld行列をObject定数に使う。

フレーム開始・終了、Fence、CommandListの送信とPresentはDX12Backend側の責務とする。
