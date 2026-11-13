# エンジンをビルドして、配る用の zip（SDK）を作る。
#   pwsh tools/package_sdk.ps1              Debug と Release をビルドしてから、dist/TakoEngine-<バージョン>.zip を作る
#   pwsh tools/package_sdk.ps1 -SkipBuild   前に流したときのビルド結果をそのまま入れる（手元で形を確かめる用）
#
# zip の中は、エンジンのフォルダーと同じ形（VERSION・bin・build・project・templates）。
# ゲームのプロジェクトの Engine\TakoEngine にそのまま置けば、エンジンをビルドせずにゲームをビルドできる。
param([string]$OutDir = "", [switch]$SkipBuild)

$ErrorActionPreference = "Stop"
$root    = Split-Path $PSScriptRoot -Parent
$version = (Get-Content "$root\VERSION" -TotalCount 1).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "VERSION が 2.1.0 の形ではありません: $version" }
if ($OutDir -eq "") { $OutDir = Join-Path $root "dist" }
$stage = Join-Path $OutDir "TakoEngine-$version"
$zip   = "$stage.zip"
$releaseSuffix = "-NoLTCG"

if (-not $SkipBuild) {
  $msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -prerelease -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
  if (-not $msbuild) { throw "MSBuild が見つかりません" }
  & $msbuild "$root\project\TakoEngine.sln" -m -nologo -v:minimal -p:Configuration=Debug -p:Platform=x64
  if ($LASTEXITCODE -ne 0) { throw "Debug のビルドに失敗しました" }
  # /GL（プログラム全体の最適化）付きの静的ライブラリは、作ったのと同じ版のコンパイラでしかリンクできないので、配る物は /GL なしで作る。
  # 普段の Release と混ざらないよう、別のフォルダー（bin\Release-NoLTCG）に出す。Rebuild は使わない（Clean が DirectXTex のコンパイル済みシェーダーを消す）
  & $msbuild "$root\project\TakoEngine.sln" -m -nologo -v:minimal -p:Configuration=Release -p:Platform=x64 -p:WholeProgramOptimization=false "-p:TakoBuildSuffix=$releaseSuffix"
  if ($LASTEXITCODE -ne 0) { throw "Release のビルドに失敗しました" }
}

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
if (Test-Path $zip) { Remove-Item $zip -Force }

# 入れない物: git の管理・中間ファイル・エンジンのビルドにしか要らない物（assimp の lib、TakoEngine.dll に入った後の静的ライブラリ）・
# エンジンの開発にしか要らない物（git の対象外の Docs など・Visual Studio の作業フォルダー .vs）
$excludeDirs = @(
  "$root\.git", "$root\.github", "$root\.claude", "$root\obj", "$root\bin", "$root\dist", "$root\Generated", "$root\Docs", ".vs",
  "$root\project\externals\assimp\lib", "$root\project\externals\DirectXTex\Bin"
)
$excludeFiles = @("*.user", ".git", ".gitignore", ".gitattributes", "CLAUDE.md")
# /R /W: 他のプロセスが掴んでいるファイルは 2 回だけやり直して、失敗にする（robocopy の既定は 30 秒おきに 100 万回で、終わらない）
robocopy $root $stage /E /XD $excludeDirs /XF $excludeFiles /R:2 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy 失敗 (exit code: $LASTEXITCODE)" }

# bin は配る物だけを入れる。Release は /GL なしでビルドした方を、bin\Release として入れる
$releaseBin = "$root\bin\Release$releaseSuffix"
if (-not (Test-Path "$releaseBin\TakoEngine.lib")) { throw "$releaseBin がありません。-SkipBuild なしで流してください" }
$intermediate = @("*.ilk", "*.idb", "*.exp", "*.iobj", "*.ipdb", "imgui.*", "imgui-node-editor.*")
# Debug の DirectXTex は TakoEngine.dll に入っている。Release の DirectXTex.lib は、ゲームがリンクするので入れる
robocopy "$root\bin\Debug" "$stage\bin\Debug" /E /XF ($intermediate + "DirectXTex.*") /R:2 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy 失敗 (exit code: $LASTEXITCODE)" }
robocopy $releaseBin "$stage\bin\Release" /E /XF $intermediate /R:2 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy 失敗 (exit code: $LASTEXITCODE)" }

$required = @(
  "VERSION", "CHANGELOG.md",
  "bin\Debug\TakoEditor.exe", "bin\Debug\TakoEngine.dll", "bin\Debug\TakoEngine.lib", "bin\Debug\dxcompiler.dll", "bin\Debug\dxil.dll",
  "bin\Release\TakoEngine.lib", "bin\Release\DirectXTex.lib",
  "build\TakoEngineGame.props", "build\TakoEngineCommon.props", "build\TakoReflection.props", "build\TakoHeaderTool.cs",
  "project\engine\core\base\TakoFramework.h", "project\engine\resources\shaders", "project\externals\imgui\imgui.h",
  "templates\Default\__ProjectName__.sln"
)
$missing = $required | Where-Object { -not (Test-Path (Join-Path $stage $_)) }
if ($missing) { throw "SDK に入っていない物があります: $($missing -join ', ')" }

# tar は Windows 10 以降に入っている。拡張子が .zip なら zip で書く
tar -a -c -f $zip -C $stage (Get-ChildItem $stage -Name)
if ($LASTEXITCODE -ne 0) { throw "zip を作れませんでした" }
Remove-Item $stage -Recurse -Force

$sizeMB = (Get-Item $zip).Length / 1MB
Write-Host ("SDK: {0} ({1:N0} MB)" -f $zip, $sizeMB)
exit 0
