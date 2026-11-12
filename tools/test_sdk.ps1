# package_sdk.ps1 が作った zip だけで、プロジェクトを作ってビルドできるかを確かめる（リリースの前と CI で流す）。
#   pwsh tools/test_sdk.ps1 [-Zip <zip>] [-WorkDir <作業フォルダー>]
# zip を作業フォルダーへ開き、そこの TakoEditor.exe でプロジェクトを作り、Debug と Release をビルドする。エンジンのソースのフォルダーは使わない。
param([string]$Zip = "", [string]$WorkDir = "")

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
if ($Zip -eq "") {
  $version = (Get-Content "$root\VERSION" -TotalCount 1).Trim()
  $Zip     = Join-Path $root "dist\TakoEngine-$version.zip"
}
if (-not (Test-Path $Zip)) { throw "zip がありません: $Zip（先に tools/package_sdk.ps1 を流す）" }
# 一時フォルダーの下でビルドすると MSB8029 の警告が出るので、既定は dist の中にする
if ($WorkDir -eq "") { $WorkDir = Join-Path $root "dist\sdk_test" }
if (Test-Path $WorkDir) {
  # 前回の残り。プロジェクトの中の Engine\TakoEngine はジャンクションなので、リンクだけを先に外す
  Get-ChildItem $WorkDir -Recurse -Directory -Attributes ReparsePoint -ErrorAction SilentlyContinue | ForEach-Object { cmd /c rmdir "$($_.FullName)" }
  Remove-Item $WorkDir -Recurse -Force
}
$sdk = Join-Path $WorkDir "sdk"
New-Item -ItemType Directory -Force $sdk | Out-Null
tar -x -f $Zip -C $sdk
if ($LASTEXITCODE -ne 0) { throw "zip を開けませんでした" }

# エディタは窓を出さずにプロジェクトを作って終わる
$create = Start-Process -FilePath "$sdk\bin\Debug\TakoEditor.exe" -ArgumentList "--new", "`"$WorkDir`"", "SmokeGame" -PassThru -Wait
if ($create.ExitCode -ne 0) { throw "プロジェクトを作れませんでした (exit code: $($create.ExitCode))" }

$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -prerelease -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) { throw "MSBuild が見つかりません" }
$project = Join-Path $WorkDir "SmokeGame"
foreach ($configuration in "Debug", "Release") {
  & $msbuild "$project\SmokeGame.sln" -m -nologo -v:minimal "-p:Configuration=$configuration" -p:Platform=x64
  if ($LASTEXITCODE -ne 0) { throw "ゲームのビルド（$configuration）に失敗しました" }
}
$outputs = @("Generated\outputs\Debug\Game.dll", "Generated\outputs\Release\SmokeGame.exe", "Generated\outputs\Release\EngineResources", "GameProject\EngineResources")
$missing = $outputs | Where-Object { -not (Test-Path (Join-Path $project $_)) }
if ($missing) { throw "ビルドの結果が足りません: $($missing -join ', ')" }

Write-Host "SDK の確認: OK（$project）" -ForegroundColor Green
exit 0
