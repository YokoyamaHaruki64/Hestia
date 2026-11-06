# Build Troubleshooting

## MSVC C1902: Program Database Manager mismatch

最終的な対処方針: C1902が発生した場合は、コマンド単位の承認によりSandbox外で実行する。それでも発生する場合は、実行コマンド・構成・エラー内容をユーザーへ報告して対応を相談する。

| 推定原因 | 対処法 |
|---|---|
| SandboxがPDB補助プロセスの起動・通信を制限している可能性 | ビルド・テストをコマンド単位の承認によりSandbox外で実行する。具体的な制限箇所は未特定 |
| コンパイラとPDB関連ツールの版の不一致・欠落 | 同じMSVCツールセットの`cl.exe`、`mspdbXXX.dll`、`mspdbcore.dll`、`mspdbsrv.exe`を揃える。不一致・欠落があればVisual Studio Installerで修復する |
| 環境変数・PATHによるツール探索先の混在 | 対象Visual StudioのDeveloper Command Promptを使い、実際のツールパスと版を確認する |
| 別ツールセットで生成された古いPCH／PDB | 実行中のビルドを止め、対象プロジェクト・構成の中間生成物だけをClean／Rebuildする |
| 同じPDBへの複数ビルドからの同時アクセス | 同一出力先へのビルド重複を避け、プロジェクト・構成ごとに中間出力先を分ける |
