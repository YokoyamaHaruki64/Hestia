# Graphicsの型

[処理・構造](README.md)

## Systemと資源管理

| 型 | 責務 | 定義 |
| --- | --- | --- |
| `GraphicsSystem` | Backendの所有、描画提出、View/Projection設定、Asset Uploadの入口 | [GraphicsSystem.h](../../../Engine/Include/Graphics/GraphicsSystem.h) |
| `DX12Backend` | DX12資源・コマンド記録・同期・Presentを管理 | [DX12Backend.h](../../../Engine/Include/Graphics/DX12Backend.h) |
| `FrameContext` | フレームごとのallocator、command list、Frame定数Buffer、shader-visible SRV heapとFence値 | 同上、Backend内部 |
| `WaitingResource` | 転送用ResourceのComPtrと解放可能になるFence値 | 同上 |
| `UploadContextPool` | Upload用allocator/listを管理し、空きがなければ追加。初期数16 | 同上 |
| `UploadContext` | allocator、command list、使用中Fence値、空き状態 | 同上、Pool内部 |
| `RootParameterIndex` | Frame/Object/Material CBV、System/Material SRVのRoot引数番号 | 同上 |
| `PerMaterialCB` | map済み定数Buffer、Material名のcache、登録index。要素strideは256byte境界 | 同上 |
| `PerObjectCB` | Object定数Bufferとmap先。要素strideは256byte境界 | 同上 |
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
| `DXMesh` | Stream別Vertex Buffer、index Resource/view、頂点・index数 |
| `PerFrameConstants` | View、Projection、cameraPosition、経過時間 |
| `PerObjectConstants` | worldMatrix |
| `Material` | 名前、diffuse/specular/emission色、roughness、metallic、Texture Handle配列。Defaultは白diffuse/specular、黒emission、roughness 0.5、metallic 0 |
| `MaterialConstants` | Materialの数値・色部分。Texture Handleや名前を含まないGPU定数 |
| `RenderData` | worldMatrix、Material値、Mesh Handle、VS/PS pathのwstring_view。MeshやShader文字列の所有者ではない |

## HLSLの型

[Common.hlsl](../../../Shaders/Common.hlsl) の `VSInput` は位置・UV・法線・接線・色、`PSInput_Default` はclip位置・world位置・UV・法線・接線・色、`PSInput_Sprite` はclip位置・UV・色を持つ。現在のSprite shaderが使う受け渡しはPSInput_Sprite。

`PerFrameBuffer`、`PerObjectBuffer`、`PerMaterialBuffer` はそれぞれC++側のFrame/Object/Material定数に対応するcbuffer。CPU側のメモリ配置とShaderの行列格納規約の整合が必要であり、C++型を変更する際はShader側も確認する。
