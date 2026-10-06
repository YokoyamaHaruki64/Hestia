# Render Targetの容量と帯域の概算

[[RenderingPipeLine]]で使うTexture Formatの仮置きと、描画解像度に対する容量・読書き量を調べる資料。最初の比較基準はRTX 3050 Laptop 4GB、1920×1080、60FPSとする。ここで計算するのはFormatから求めた論理的な値であり、GPU上の実割当量や実測VRAM帯域ではない。

## 計算条件

- 1 View、1920×1080、60FPS、MSAAなし、全画面の各Textureを1枚ずつ確保。SceneColorだけはPing-Pong用に2枚確保する。
- Formatの1 Pixel当たりByte数 × 幅 × 高さ × 枚数を容量とする。`MiB = Byte / 2^20`、`GB/s = Byte/Frame × FPS / 10^9`。
- Hi-Zは既存Depthとは別の`R32_FLOAT` Pyramidとし、最初のMipを半解像度に置く。全Mipの画素数を元画像の約1/3、容量を元画像1 Pixel当たり約4/3 Byteと見積もる。
- Shadow Map、AO Texture、SSR History、Bloom、SwapChain BackBuffer、Asset、Mesh、Upload Buffer、Editorの追加Viewは下表に含めない。

DXGI Formatのbit数は[MicrosoftのDXGI_FORMAT一覧](https://learn.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)を参照した。Textureの実割当量はTile配置やAlignmentにより異なるため、実装時に`GetResourceAllocationInfo`でGPUごとに確認する。[MicrosoftのD3D12 Resource説明](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_resource_desc)

## 仮のFormatと容量

| Texture          | 仮Format                | 内容                                    | Byte/Pixel |         1080p容量 |  60FPSで1回Write |
| ---------------- | ---------------------- | ------------------------------------- | ---------: | --------------: | -------------: |
| DepthStencil     | `D24_UNORM_S8_UINT`    | Depth / Stencil                       |          4 |        7.91 MiB |     0.498 GB/s |
| GBuffer0         | `R8G8B8A8_UNORM`       | BaseColor.rgb + Material AO           |          4 |        7.91 MiB |     0.498 GB/s |
| GBuffer1         | `R8G8B8A8_UNORM`       | OctNormal.xy + Roughness + Metallic   |          4 |        7.91 MiB |     0.498 GB/s |
| GBuffer2         | `R11G11B10_FLOAT`      | Emission.rgb                          |          4 |        7.91 MiB |     0.498 GB/s |
| MaterialMetadata | `R32_UINT`             | MaterialID 16bit + ShadingModelID 8bit + Flags 8bit | 4 | 7.91 MiB | 0.498 GB/s |
| ObjectMetadata   | `R32_UINT`             | ObjectID 24bit + Flags 8bit           |          4 |        7.91 MiB |     0.498 GB/s |
| SceneColor A     | `R16G16B16A16_FLOAT`   | HDR Color / Ping-Pong                 |          8 |       15.82 MiB |     0.995 GB/s |
| SceneColor B     | `R16G16B16A16_FLOAT`   | HDR Color / Ping-Pong                 |          8 |       15.82 MiB |     0.995 GB/s |
| MotionVector     | `R16G16_FLOAT`         | TAA / Upscale / Motion Blur           |          4 |        7.91 MiB |     0.498 GB/s |
| Hi-Z             | `R32_FLOAT`半解像度からのMip列 | SSR / Occlusion Culling等              |      約1.33 |       約2.64 MiB |    約0.166 GB/s |
| **合計**           |                        |                                       | **約45.33** | **約89.65 MiB** | **約5.64 GB/s** |

「1回Write」は各Textureへ1 Pixel当たり1回書く場合の計算で、Pipeline全体の帯域ではない。Hi-Zの行は全Mipを合わせた元画像Pixel当たりの換算値。MotionVectorやObjectMetadataを使用しないViewでは、その行を除外できる。

### Metadataのbit配置

`MaterialMetadata`は下位からMaterial ID 16bit、ShadingModel ID 8bit、Flags 8bitとして`R32_UINT`へ格納する。

```text
31            24 23            16 15                         0
┌───────────────┬────────────────┬────────────────────────────┐
│ Flags: 8bit   │ Shading: 8bit  │ Material ID: 16bit         │
└───────────────┴────────────────┴────────────────────────────┘
```

`ObjectMetadata`は下位からObject ID 24bit、Flags 8bitとして`R32_UINT`へ格納する。

```text
31            24 23                                             0
┌───────────────┬────────────────────────────────────────────────┐
│ Flags: 8bit   │ Object ID: 24bit                               │
└───────────────┴────────────────────────────────────────────────┘
```

Material IDは最大65,536値、ShadingModel IDは256値、Object IDは最大16,777,216値を表せる。予約値、IDの再利用、各Flagsの意味は個別に決める。

Material Flagsの初期割当は次とする。ここでbit 1は最下位bitを指す。

| bit | Mask | 名前 | 意味 |
| ---: | ---: | --- | --- |
| 1 | `0x01` | `SSRReceive` | このPixelへSSRを適用する |
| 2 | `0x02` | `ReflectionMask` | 組込みSSRで反射対象として扱う |
| 3～8 | `0x04`～`0x80` | Reserved | 将来用 |

組込みSSRは初期構成では2つのFlagsをboolとして使い、MaterialごとのReflection Factorは適用しない。将来のCustom SSRではMaterial IDからGPU上のMaterialData Tableを参照し、Factorや追加Parameterを取得できるようにする。

### Formatの内容で再検討が必要な点

- GBuffer1はOctahedral EncodingしたNormal.xy、Roughness、Metallicを各8bitへ格納する。Normalの復元誤差とRoughness / Metallicの8bit精度を実画像で確認する。
- GBuffer2は`R11G11B10_FLOAT`で正のHDR Emissionを保持する。Alphaや負の値は持てないため、負のEmissionを仕様へ含めない。
- EmissionをGBufferへ常駐させず、発光Materialだけを別PassでSceneColorへ加算する構成も比較対象にする。まずGBuffer2方式を実装して実測し、帯域・MRT・VRAM使用量が問題になった場合に移行を検討する。
- Lighting成分を個別Textureへ出すDebug / 計測用の経路を追加する場合は、Format、値域、保持期間を改めて決める。

## 解像度ごとの常駐容量

同じFormatと同じ1 Viewの条件を解像度だけ変えた値。

| 描画解像度 | Pixel数 | 上記Texture合計 | 60FPSで各Textureを1回Write | 120FPSで各Textureを1回Write |
| --- | ---: | ---: | ---: | ---: |
| 1920×1080 | 2,073,600 | 89.65 MiB | 5.64 GB/s | 11.29 GB/s |
| 2560×1440 | 3,686,400 | 159.38 MiB | 10.03 GB/s | 20.06 GB/s |
| 3840×2160 | 8,294,400 | 358.59 MiB | 22.57 GB/s | 45.15 GB/s |

1080pの89.65 MiBは公称4 GiBの約2.2%、8 GiBの約1.1%。ただしEditorのScene ViewとGame View、Frame In Flight、Shadowや履歴Textureなどを追加すると増える。単純に同じ構成を2組持てば約179.30 MiB、3組なら約268.95 MiBになる。Render Graphで寿命が重ならないTextureを再利用できるかは別途判断する。

## 1080p出力で内部解像度を下げる場合

75% / 50%は幅と高さの倍率として扱う。Pixel数と内部解像度のRender Target容量は倍率の二乗で減る。出力解像度は1920×1080のまま。

| 内部解像度           |  内部Pixel数 | 1080p比 | 内部Render Target一式 | + 1080p LDR出力1枚 | + AfterUpscale用LDR Ping-Pong 2枚 |
| --------------- | --------: | -----: | ----------------: | ------------------: | --------------------------: |
| 100%: 1920×1080 | 2,073,600 |   100% |         89.65 MiB |           97.56 MiB |                  113.38 MiB |
| 75%: 1440×810   | 1,166,400 | 56.25% |         50.43 MiB |           58.34 MiB |                   74.16 MiB |
| 50%: 960×540    |   518,400 |    25% |         22.41 MiB |           30.32 MiB |                   46.14 MiB |

出力1枚は1080p `RGBA8_UNORM`の7.91 MiB。最後の列はUpscale後のEffectが最終RenderTextureを書き換えられるよう、1080pのLDR Textureを2枚持つ場合。AfterUpscale EffectがなくUpscalerから最終RenderTextureへ直接書く構成なら、LDR Ping-Pong 2枚のうち1枚を省ける。BackBuffer自体は各列に含めない。

内部解像度を下げても、出力解像度のBackBuffer、UI、AfterUpscale Effectは1080pのまま。Temporal Upscalerが読むMotion Vector / Depthや、History Textureは次の帯域モデルへ追加していない。

### Read / Writeモデル

全画面1回の論理的なRead / Write量を仮置きした例。Cache、Compression、Early-Z、Overdraw、Texture Filter、Ray Marchの繰り返しは反映しない。

| 処理 | Read Byte/Pixel | Write Byte/Pixel | 計 Byte/Pixel | 累計 Byte/Pixel | 1080p・60FPS |
| --- | ---: | ---: | ---: | ---: | ---: |
| GBuffer(4\*3) + Depth(4) + Metadata(4\*2) + Motion(4)のWrite | 0 | 28 | 28 | 28 | 3.48 GB/s |
| Deferred Lighting: Depth・GBuffer0 / 1 / 2・MaterialMetadataをRead、SceneColorをWrite | 20 | 8 | 28 | 56 | 3.48 GB/s |
| Reflection Pass: SceneColor・Depth・GBuffer0 / 1・MaterialMetadataをRead、SceneColorをWrite | 24 | 8 | 32 | 88 | 3.98 GB/s |
| Tone Map: HDR SceneColorをRead、LDR RenderTextureへWrite | 8 | 4 | 12 | 100 | 1.49 GB/s |
| 最終出力: LDR RenderTextureをRead、`RGBA8` BackBufferへCopy | 4 | 4 | 8 | 108 | 0.995 GB/s |
| **上記のみの合計** |  |  | **108** | **108** | **13.44 GB/s** |

このモデルでは1080p・60FPSで約13.44 GB/s、1440pで約23.89 GB/s、4Kで約53.75 GB/s。60FPSから120FPSへ上げると各値は2倍になる。Reflection PassのSSR Ray March、Environment Texture、SSAO Texture、Shadow Mapは含めない。HDR SceneColorを1回Ping-PongするScreen Effectは、RGBA16_FLOATのRead 8 + Write 8 = 16 Byte/Pixelで、1080p・60FPSあたり約1.99 GB/sを追加する。LDR RenderTextureを1回Ping-PongするEffectは、RGBA8のRead 4 + Write 4 = 8 Byte/Pixelで、1080p・60FPSあたり約0.995 GB/sを追加する。

Deferred Lightingへ渡すGBuffer0のAOはMaterial AO。ScreenEffectShaderが出力するSSAO Textureはこの表に含まず、Lightingで全面サンプルするなら約1 Byte/PixelのReadを追加する。半解像度TextureならFull-HDの常駐量は約0.49 MiBだが、実際の帯域はSample回数やCacheで変わる。
### Upscaleを含む論理帯域

Tone Mapまでの内部Passは前項のRead / Writeモデルから100 Byte/内部Pixelとする。Tone MapはHDR SceneColorを8 Byteで読み、内部解像度のLDR RenderTextureへ4 Byteで書く。Upscalerは内部LDRを4 Byte/Pixelで読み、1080p LDR RenderTextureへ4 Byte/Pixelで出力する。最後に1080p LDR RenderTextureを4 Byteで読み、`RGBA8` BackBufferへ4 ByteでCopyする。

```text
1 FrameのByte数 = 100 × 内部Pixel数
                + 4 × 内部Pixel数  // UpscalerのLDR入力
                + 4 × 1080p Pixel数 // Upscale出力RenderTexture書込み
                + 4 × 1080p Pixel数 // 最終RenderTexture読出し
                + 4 × 1080p Pixel数 // BackBuffer Copy書込み
```

| 内部解像度 | 内部Pass | Upscale Color Read / Write | 最終出力 | 合計 / Frame | 60FPS論理帯域 | 120FPS論理帯域 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Native 1080p・Upscaleなし | 207.36 MB | — | 16.59 MB | 223.95 MB | 13.44 GB/s | 26.87 GB/s |
| 75%: 1440×810 | 116.64 MB | 12.96 MB | 16.59 MB | 146.19 MB | 8.77 GB/s | 17.54 GB/s |
| 50%: 960×540 | 51.84 MB | 10.37 MB | 16.59 MB | 78.80 MB | 4.73 GB/s | 9.46 GB/s |

この計算はTone Map後をLDR `RGBA8` RenderTextureとして扱い、最終RenderTextureからBackBufferへ4 Byte/PixelのCopyを行う論理値。Upscale出力と最終RenderTextureを同じTextureへ書けるなら、出力Writeまたは最終Readを省ける。Temporal UpscalerのMotion Vector / Depth入力、History Read / Write、ScreenEffectShaderは別途加算する。100%行はUpscaleを実行せず、内部HDR Pass、Tone Map、最終RenderTextureからBackBufferへのCopyを数えている。これはGPU実測値でもフレーム時間の予測でもない。

| 内部解像度 | 論理帯域 / 60FPS | RTX 3050の仮帯域192 GB/s比 | RTX 4060 Tiの理論288 GB/s比 |
| --- | ---: | ---: | ---: |
| Native 1080p | 13.44 GB/s | 7.0% | 4.7% |
| 75%: 1440×810 | 8.77 GB/s | 4.6% | 3.0% |
| 50%: 960×540 | 4.73 GB/s | 2.5% | 1.6% |

この比率は単純化した転送量を公称ピーク値で割った参考値で、GPU使用率や実時間の予測には使えない。Upscale自体のShader負荷、SSR Ray March、Overdraw、Cache、圧縮の影響は実測する。

### Emission方式の比較

GBuffer2方式では、EmissionのGBuffer Write 4 ByteとHDR合成時のRead 4 Byteで、固定的に8 Byte/Pixelを使う。1080p・60FPSでは約1.00 GB/s。

別Pass方式ではGBuffer2を省ける一方、発光部分のSceneColorをBlendするため、単純化すると発光PixelごとにSceneColor Read 8 Byte + Write 8 Byteを使う。全画面なら16 Byte/Pixel、1080p・60FPSで約1.99 GB/s。帯域だけを比べると、発光面積×Overdrawが画面の約50%未満なら別Passが小さくなり得る。

常駐容量はGBuffer2を外す分、1080pで約7.91 MiB減る。別Passの出力先は既存SceneColor Ping-Pongを利用する想定で、追加Render Targetは数えていない。

別PassにはDraw Call、頂点処理、Depth Test、Blend、発光Objectの再提出も加わる。逆にGBuffer2方式は非発光Pixelを含む全Opaqueへ固定費用が掛かる。初期実装はGBuffer2とし、PIXでGBuffer Pass、HDR合成、別Pass試作のGPU時間とVRAM Trafficを比較して判断する。

SSRを「SceneColor、Depth、GBuffer1、反射Maskを各1回Readし、SceneColorDstへ1回Write」だけで数えても28 Byte/Pixel、1080p・60FPSで約3.48 GB/sとなる。実際のSSRはDepthやColorを何度もSampleするため、この数字から所要帯域や実行時間は予測できない。Hi-Z生成、AO、Shadow、ForwardのOverdraw、Bloom、UIも上の合計には含めない。

## 比較するGPU

帯域の理論値は`GDDRの実効データレート(Gbps) × メモリBus幅(bit) ÷ 8`で計算する。VRAM容量は同時に置けるResource量、GB/sは外部メモリの転送能力を表し、容量を増やしても帯域が同じ製品がある。

| GPU | VRAM | Bus | 実効メモリ速度 | 理論帯域 | 電力仕様 | 値の扱い |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| RTX 3050 Laptop（ノートPC） | 4 GB GDDR6 | 128-bit | **12 Gbps仮定** | **192 GB/s仮定** | NVIDIA掲載35～80 W | VRAMはユーザー確認。速度・実機TGPは未取得 |
| RTX 4060 Ti（このPC） | 8 GB GDDR6 | 128-bit | 18 Gbps | 288 GB/s | 160 W Power Limit | `nvidia-smi`で8188 MiB、最大Memory Clock 9001 MHz、Power Limit 160 Wを確認 |
| RTX 4060 Ti 16GB（比較用） | 16 GB GDDR6 | 128-bit | 18 Gbps | 288 GB/s | 165 W（MSI製品例） | 8GB版より容量は大きいが理論帯域は同じ |
| RTX 4060 8GB（比較用） | 8 GB GDDR6 | 128-bit | 17 Gbps | 272 GB/s | 115～120 W（MSI製品例） | 比較用のメーカー公表値 |

RTX 3050 Laptopの4GB・128-bit・35～80Wは[NVIDIAのLaptop GPU仕様](https://www.nvidia.com/en-ph/geforce/laptops/30-series/)に基づく。12GbpsはこのノートPCの測定値ではなく帯域計算の仮定。RTX 4060 Tiの8GB/16GB容量と128-bitは[NVIDIAの製品仕様](https://www.nvidia.com/en-us/geforce/graphics-cards/40-series/rtx-4060-4060ti/)を、実効速度と電力の比較値は[MSIの4060 Ti 8GB](https://www.msi.com/Graphics-Card/GeForce-RTX-4060-Ti-GAMING-X-8G/Specification)、[MSIの4060 Ti 16GB](https://us.msi.com/Graphics-Card/GeForce-RTX-4060-Ti-VENTUS-2X-BLACK-16G-OC/Specification)、[MSIの4060 8GB](https://us.msi.com/Graphics-Card/GeForce-RTX-4060-GAMING-8G/Specification)に基づく。PC上の`nvidia-smi`は2026-09-28に読み取った。

1080p・60FPSの13.44 GB/sは、仮の192 GB/sに対して約7.0%、4060 Tiの288 GB/sに対して約4.7%。これはPassの論理Read / Write量と理論帯域を並べただけで、実行時間や余裕率を意味しない。Cacheや圧縮で外部VRAM転送は減り得る一方、Overdraw、Shadow、SSRの多重Sample、Blendingなどで増え得る。

## 実装時に計測する値

- 各Textureの`GetResourceAllocationInfo`による実割当量と、Frame In Flight時の最大同時使用量。
- `IDXGIAdapter3::QueryVideoMemoryInfo`の`Budget`と`CurrentUsage`。公称VRAM全量を使える前提にはしない。[MicrosoftのDXGI Video Memory情報](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/ns-dxgi1_4-dxgi_query_video_memory_info)
- PIX等でのPassごとのGPU時間、実際のVRAM Traffic、Cache Hit、Overdraw。まず3050 Laptopの1080p・60FPSで予算を超えるPassを特定する。

未確定なのはノートPC実機のMemory Data RateとTGP、Metadata IDの予約値とbit 3～8のFlags、Oct Normalの精度、Emissionを別Passへ移す実測条件、Stencilを使うか、何枚のView・Frame Resourceを同時に持つかである。これらが決まったら表を更新する。
