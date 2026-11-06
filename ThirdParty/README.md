# 外部依存ファイル

`ThirdParty/` は外部ライブラリの配布ファイルを管理する。自前プロジェクトをビルドして生成する `/Library/` とは区別し、同梱する Header と必要な配布 `.lib` を Git に含める。

## DirectXTex

- 公式取得元：[microsoft/DirectXTex](https://github.com/microsoft/DirectXTex)
- 同梱 Header：`Include/DirectXTex/DirectXTex.h`、`Include/DirectXTex/DirectXTex.inl`
- 同梱 Library：`Library/DirectXTex/DirectXTex_Debug.lib`、`Library/DirectXTex/DirectXTex_Release.lib`
- Header のバージョン識別子：`DIRECTX_TEX_VERSION = 211`
- このリポジトリでの配置日は 2026/10/08。元のダウンロード日、リリースタグ、上流 commit、Library と Header の対応版は未確認。現在の公式最新版から取得したものとは扱わない。
- ライセンス：MIT。[同梱する著作権表示・許諾全文](LICENSES/Microsoft-MIT.txt)、[公式 LICENSE](https://github.com/microsoft/DirectXTex/blob/main/LICENSE)

2026/10/09 に Library の COFF Header と linker directives を確認した。

| ファイル | 対象 | Runtime | Iterator debug level |
|---|---|---|---|
| `DirectXTex_Debug.lib` | x64 | `/MDd` | 2 |
| `DirectXTex_Release.lib` | x64 | `/MD` | 0 |

利用側は Debug 系で Debug Library、Release 系で Release Library をリンクし、Runtime を合わせる。元のビルドコマンド、Visual Studio／MSVC の実際のバージョン、Windows SDK の版、その他のビルドオプションは未確認。

現在の配布ファイルを識別する SHA-256 は次のとおり。

| ファイル | SHA-256 |
|---|---|
| `DirectXTex.h` | `6E1705467997D4DF800CC1D68CCC9FB0C056868534C94F71B26C8C300BB65AA0` |
| `DirectXTex.inl` | `1851635A031274A69C93CA195C8D65AAF7E241BBE56FA6FA8E14EA7F4594DBDF` |
| `DirectXTex_Debug.lib` | `29E750F34DA9C17A4C12B537CBF75C9C6494C68CDF5CB0F401B42EB1B0BCB3DA` |
| `DirectXTex_Release.lib` | `CFCC646A6728B64CAFA77CF5A14894A4BF4EDB9C6AFC5278DE0C2A37C5CF6599` |

## D3DX12

- 同梱ファイル：`Include/D3DX12/d3dx12.h`
- 公式プロジェクト：[microsoft/DirectX-Headers](https://github.com/microsoft/DirectX-Headers)
- 同梱 Header の先頭に Microsoft Corporation の著作権表示と MIT License の記載がある。
- 正確な取得元の版、上流 commit、取得日は未確認。上記公式プロジェクトは参照先であり、同梱ファイルをその最新版と同一とは扱わない。
- ライセンス：MIT。[同梱する著作権表示・許諾全文](LICENSES/Microsoft-MIT.txt)、[公式 LICENSE](https://github.com/microsoft/DirectX-Headers/blob/main/LICENSE)

## 更新と配布

- 外部依存を更新するときは取得元、リリースタグまたは commit、対象アーキテクチャ、Runtime、使用したビルド条件を記録し、Header と Library の対応を確認する。
- 同梱ファイルの著作権表示を保持し、これらを再配布する場合は `LICENSES/Microsoft-MIT.txt` の全文も配布物に含める。
- `/Library/`、Shader の `.cso`、`.pdb`、`.idb` など自前のビルド生成物は Git 管理対象にしない。`.lib` の追跡例外は配布ファイルを置く `ThirdParty/Library/` に限定する。
