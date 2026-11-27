# Jolt Physics へのローカル修正

取り込んだ版は 5.7.0（`Jolt/Core/Core.h` の `JPH_VERSION_*`。配布元のタグは `v5.7.0`）。配布元のリポジトリのうち、`Jolt/` フォルダーと `LICENSE` だけを入れてある。

ライブラリを差し替えるときは、下の修正を入れ直す。修正した所には `[TakoEngine]` で始まるコメントを付けてある。

## ソースの修正

無し。

## ソース以外の違い

### 1. CMake を使わず、Jolt.vcxproj でビルドする（2026-11-27）

- **場所**：`Jolt.vcxproj`・`Jolt.vcxproj.filters`（配布元には無い）
- **作り方**：`tools/gen_jolt_vcxproj.py` が、`Jolt/Jolt.cmake` の最初の `set(JOLT_PHYSICS_SRC_FILES ...)`（CMake のオプションに関係なくビルドされるソース）から作る。Jolt を差し替えたら流し直す
  - `ENABLE_OBJECT_STREAM`・`JPH_USE_DX12`・`JPH_USE_VK`・`JPH_USE_CPU_COMPUTE` のオプションで足されるソースは、入れていない
- **コンパイルの設定**：
  - 出力先（`$(TakoEngineBinDir)`・`$(TakoEngineObjDir)`）と CRT（`$(TakoRuntimeLibrary)`）は、`build/TakoEngineCommon.props` から受け取る
  - 言語標準は、エンジンと同じ C++20（配布元は C++17 以上）
  - 配布元の CMake が MSVC に付ける `/fp:fast`・`/GR-`・例外なし・`/GS-`・`/arch:AVX2`・`/GL` は付けていない（エンジンと同じ、コンパイラの既定のまま）
- **マクロ**：`build/TakoEngineCommon.props` の `TakoJoltDefines`（Debug と Develop は `JPH_DEBUG_RENDERER`、Debug は `JPH_DEBUG` も）。`Jolt.vcxproj` と、Jolt のヘッダーを include する `TakoEngine.vcxproj` が同じ値を使う。配布元の CMake が既定で付ける `JPH_PROFILE_ENABLED`・`JPH_OBJECT_STREAM` は付けていない
- **理由**：リポジトリに CMake が無く、`TakoEngine.sln` をビルドするだけで済ませるため（CI・`UpdateEngine`・`tools/package_sdk.ps1`）。マクロがライブラリと使う側で食い違うと、`RegisterTypes` が起動時に止めるか、クラスの形が合わなくなる（`JPH_DEBUG` は `RegisterTypes` の検査の対象外）
