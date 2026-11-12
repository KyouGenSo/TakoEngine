# TakoEngine を入れる・更新する。プロジェクトのルート（.takoproject のあるフォルダー）で UpdateEngine.bat を実行する。
#   UpdateEngine.bat                  最新のリリースにする。Engine\TakoEngine が無ければ、.takoproject に書いてあるバージョンを入れる
#   UpdateEngine.bat -Version 2.1.0   そのバージョンにする
#   UpdateEngine.bat -Source          ビルド済みの SDK ではなく、ソースを git で取ってきて自分でビルドする（エンジンが無いときだけ効く）
#
# Engine\TakoEngine の中身で、やり方が決まる:
#   ジャンクション   別の場所のエンジンを共有している。そのフォルダーで更新するよう知らせて終わる
#   .git がある      ソース。タグ v<バージョン> に切り替えて、エンジンをビルドする
#   それ以外         SDK。GitHub のリリースの zip に入れ替える
param([string]$Version = "", [switch]$Source)

$ErrorActionPreference = "Stop"
$repository = "KyouGenSo/TakoEngine"
$root       = $PSScriptRoot
$engine     = Join-Path $root "Engine\TakoEngine"
$marker     = Get-ChildItem $root -Filter *.takoproject | Select-Object -First 1

function Fail($message) {
  Write-Host $message -ForegroundColor Red
  exit 1
}

if (-not $marker) { Fail "このフォルダーに .takoproject がありません。プロジェクトのルートで実行してください。" }
if ($Version -ne "" -and $Version -notmatch '^\d+\.\d+\.\d+$') { Fail "バージョンは 2.1.0 の形で指定してください: $Version" }
if (Get-Process TakoEditor -ErrorAction SilentlyContinue) { Fail "TakoEditor が動いています。閉じてから実行してください。" }

$project = Get-Content $marker.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
$item    = Get-Item $engine -ErrorAction SilentlyContinue
$current = ""
if (Test-Path "$engine\VERSION") { $current = (Get-Content "$engine\VERSION" -TotalCount 1).Trim() }

if ($item -and $item.LinkType -eq "Junction") {
  Write-Host "Engine\TakoEngine は、別の場所のエンジンを指しています: $($item.Target)"
  Write-Host "エンジンの更新は、そのフォルダーで行ってください（今のバージョン: $current）。"
  exit 0
}

$isGit = (Test-Path "$engine\.git") -or ((-not $item) -and $Source)

# 入れるバージョンを決める
$target = $Version
if ($target -eq "" -and -not $item -and $project.engineVersion) { $target = $project.engineVersion }
if ($target -eq "") {
  $tag = $null
  if ($isGit -and $item) {
    git -C $engine fetch --tags --quiet
    $tag = git -C $engine tag --list "v*" --sort=-v:refname | Select-Object -First 1
  }
  else {
    try { $tag = (Invoke-RestMethod "https://api.github.com/repos/$repository/releases/latest").tag_name } catch { $tag = $null }
  }
  if (-not $tag) { Fail "リリース（v で始まるタグ）が見つかりません。" }
  $target = $tag.TrimStart("v")
}
if ($target -eq $current) {
  Write-Host "TakoEngine $current のままです（入れようとしたバージョンと同じ）。"
  exit 0
}
$from = $current
if ($from -eq "") { $from = "(なし)" }
Write-Host "TakoEngine: $from -> $target"

if ($isGit) {
  if (-not $item) {
    git clone --branch "v$target" "https://github.com/$repository.git" $engine
    if ($LASTEXITCODE -ne 0) { Fail "git clone に失敗しました。" }
  }
  else {
    if (git -C $engine status --porcelain) { Fail "Engine\TakoEngine に、コミットしていない変更があります。コミットするか戻してから実行してください。" }
    git -C $engine fetch --tags --quiet
    git -C $engine checkout --quiet "v$target"
    if ($LASTEXITCODE -ne 0) { Fail "v$target に切り替えられませんでした。" }
  }
  $msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -prerelease -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
  if (-not $msbuild) { Fail "MSBuild が見つかりません。Visual Studio（C++ によるデスクトップ開発）を入れてください。" }
  foreach ($configuration in "Debug", "Release") {
    Write-Host "エンジンをビルドします（$configuration）..."
    & $msbuild "$engine\project\TakoEngine.sln" -m -nologo -v:minimal "-p:Configuration=$configuration" -p:Platform=x64
    if ($LASTEXITCODE -ne 0) { Fail "エンジンのビルド（$configuration）に失敗しました。" }
  }
}
else {
  if ($item) {
    $answer = Read-Host "Engine\TakoEngine を入れ替えます（このフォルダーの中で直した物は消えます）。続けますか？ [y/N]"
    if ($answer -ne "y") { exit 0 }
  }
  $zip = Join-Path $env:TEMP "TakoEngine-$target.zip"
  $url = "https://github.com/$repository/releases/download/v$target/TakoEngine-$target.zip"
  Write-Host "ダウンロード: $url"
  try { Invoke-WebRequest $url -OutFile $zip -UseBasicParsing } catch { Fail "ダウンロードできませんでした: $($_.Exception.Message)" }
  $fresh = "$engine.new"
  if (Test-Path $fresh) { Remove-Item $fresh -Recurse -Force }
  New-Item -ItemType Directory -Force $fresh | Out-Null
  tar -x -f $zip -C $fresh
  if ($LASTEXITCODE -ne 0) { Fail "zip を開けませんでした: $zip" }
  if (-not (Test-Path "$fresh\VERSION")) { Fail "zip の中身が TakoEngine の SDK ではありません: $url" }
  # 古い物を消すのは、新しい物を開き終えてから
  if ($item) { Remove-Item $engine -Recurse -Force }
  New-Item -ItemType Directory -Force (Split-Path $engine) | Out-Null
  Move-Item $fresh $engine
  Remove-Item $zip -Force
}

# 前のバージョンより新しい分の変更履歴を出す
$changelog = "$engine\CHANGELOG.md"
if (Test-Path $changelog) {
  Write-Host ""
  $show = $false
  foreach ($line in Get-Content $changelog -Encoding UTF8) {
    if ($line -match '^## \[(\d+\.\d+\.\d+)\]') { $show = ($current -eq "") -or ([version]$Matches[1] -gt [version]$current) }
    if ($show) { Write-Host $line }
  }
}

# engineVersion の値だけを書き換える（ConvertTo-Json で書き直すと、BOM が付き、キーの順と字下げが変わる）
$text = [IO.File]::ReadAllText($marker.FullName)
[IO.File]::WriteAllText($marker.FullName, ($text -replace '("engineVersion"\s*:\s*)"[^"]*"', "`${1}`"$target`""))
Write-Host ""
Write-Host "TakoEngine $target にしました。ゲームのソリューションをビルドし直してから、エディタを開いてください。" -ForegroundColor Green
