# DirectXTex へのローカル修正

取り込んだ版は `DIRECTX_TEX_VERSION 198`（`DirectXTex.h`）。

ライブラリを差し替えるときは、下の修正を入れ直す。修正した所には `[TakoEngine]` で始まるコメントを付けてある。

## ソースの修正

無し。

## ソース以外の違い

### 1. 出力先と CRT を TakoEngineCommon.props から受け取る（2026-11-12）

- **場所**：`DirectXTex_Desktop_2022_Win10.vcxproj`
- **修正後**：
  - `ExtensionSettings` で `..\..\..\build\TakoEngineCommon.props` を import する
  - `OutDir` を `$(TakoEngineBinDir)`、`IntDir` を `$(TakoEngineObjDir)` にする（Debug / Release / Profile の 3 か所）
  - `RuntimeLibrary` を `$(TakoRuntimeLibrary)` にする（Debug と Release）
- **元**：`OutDir` と `IntDir` は `$(SolutionDir)\Generated\...`、`RuntimeLibrary` は Debug が `MultiThreadedDebug`、Release が `MultiThreaded`（どちらも、取り込んだ後にこのリポジトリで書き換えた値。配布元の値ではない）
- **理由**：Debug のエンジンは `TakoEngine.dll` で、ゲームの DLL と CRT を共有する（`/MDd`）。CRT の種類が違う静的ライブラリはリンクできない（LNK2038）。出力先は、エンジンのビルド結果をエンジンのフォルダー（`bin/`）にまとめるため

### それ以前の違い（コミットの記録から）

- プラットフォームツールセットを v145 にした（`a2f68949`）
- 構成を x64 だけにした（`4066fc21`）
