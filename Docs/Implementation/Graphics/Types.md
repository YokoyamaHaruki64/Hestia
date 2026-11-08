# Graphicsの型

[処理・構造](README.md)

## Systemと資源管理

| 型 | 責務 | 定義 |
| --- | --- | --- |
| `GraphicsSystem` | Backendの所有、描画提出、View/Projection設定、Asset Uploadの入口 | [GraphicsSystem.h](../../../Engine/Include/Graphics/GraphicsSystem.h) |
| `DX12Backend` | DX12資源・コマンド記録・同期・Presentを管理 | [DX12Backend.h](../../../Engine/Include/Graphics/DX12Backend.h) |
| `FrameContext` | フレームごとのallocator、command list、Frame定数Buffer、Material定数のUploadページ、shader-visible SRV heapとFence値 | 同上、Backend内部 |
| `WaitingResource` | 転送用ResourceのComPtrと解放可能になるFence値 | 同上 |
| `UploadContextPool` | Upload用allocator/listを管理し、空きがなければ追加。初期数16 | 同上 |
| `UploadContext` | allocator、command list、使用中Fence値、空き状態 | 同上、Pool内部 |
| `RootParameterIndex` | Frame/Object/Material CBV、System/Material SRVのRoot引数番号 | 同上 |
| `PerObjectCB` | Object定数Bufferとmap先。要素strideは256byte境界 | [DX12Draw.h](../../../Engine/Include/Graphics/DX12Draw.h) |
| `GPUResourceStorage<TAsset,TGpuResource>` | HandleとGPU Resourceの対応を所有。Registerで重複indexを拒否、Getで世代も照合、Clearで全解放 | [GPUResourceStorage.h](../../../Engine/Include/Graphics/GPUResourceStorage.h) |
| `GPUResourceStorage::Slot` | Resourceとgenerationを保持する内部slot | 同上 |

## 描画データ

以下のC++型は [DX12Object.h](../../../Engine/Include/Graphics/DX12Object.h) に定義される。ComPtrはCOM参照を所有する。descriptor indexはBackend所有heap内の位置であり、それ単独ではheapを所有しない。

| 型 | 保持するデータ・意味 |
| --- | --- |
| `DXRenderTexture` | Resource、RTV index、SRV index。描画先の組 |
| `DXDepthStencil` | Resource、DSV、depth/stencilそれぞれのSRV index |
| `DXTexture` | Texture Resourceとmaster SRV index。CPU Textureに対応 |
| `DXVertexBuffer` | ResourceとD3D12_VERTEX_BUFFER_VIEW |
| `DXMesh` | Stream別Vertex Buffer、index Resource/view、頂点数、SubMeshの描画範囲 |
| `PerFrameConstants` | View、Projection、cameraPosition、経過時間 |
| `PerObjectConstants` | worldMatrix |
| `RenderData` | worldMatrix、Mesh Handle、MaterialSlot順のMaterial Handle列 |

## HLSLの型

`Common.hlsl` の `PSInput_Default` はclip位置・world位置・UV・法線・接線・色、`PSInput_Sprite` はclip位置・UV・色を持つ。`Sprite.matsh` は位置・UV・色を持つ `SpriteInput` を定義し、VSからPSへ `PSInput_Sprite` を渡す。

`Common.hlsl` の `PerFrameBuffer` と `PerObjectBuffer` はC++側のFrame定数とObject定数に対応する。Materialの数値PropertyにはCompilerが生成する `PerMaterial` CBufferを使う。 [MaterialShaderの実装仕様](MaterialShader.md)
