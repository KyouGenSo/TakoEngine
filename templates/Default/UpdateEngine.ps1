# TakoEngine を入れる・更新する。プロジェクトのルート（.takoproject のあるフォルダー）で UpdateEngine.bat を実行する。
#   UpdateEngine.bat                  最新のリリースにする。Engine\TakoEngine が無ければ、.takoproject に書いてあるバージョンを入れる
#   UpdateEngine.bat -Version 2.1.0   そのバージョンにする
#   UpdateEngine.bat -Source          ビルド済みの SDK ではなく、ソースを git で取ってきて自分でビルドする（エンジンが無いときだけ効く）
#   UpdateEngine.bat -Zip <ファイル>   手元の SDK の zip（エンジンの tools\package_sdk.ps1 が作る）を入れる。バージョンが同じでも入れ替える
#
# Engine\TakoEngine の中身で、やり方が決まる:
#   ジャンクション   別の場所のエンジンをリンクで共有している。何も変えず、共有先と更新のしかたを知らせて終わる
#   .git がある      ソース。タグ v<バージョン> に切り替えて、エンジンをビルドする
#   それ以外         SDK。GitHub のリリースの zip に入れ替える
#
# エンジンを入れた後、エンジンの中のテンプレート（templates\Default\UpdateEngine.ps1）の方が新しければ、このファイルも入れ替える
param([string]$Version = "", [switch]$Source, [string]$Zip = "")

$ErrorActionPreference = "Stop"
# このファイルを直したら 1 つ上げる（エンジンの中のテンプレートの番号の方が大きいときだけ、このファイルを入れ替える）
$scriptRevision = 1
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
if ($Zip -ne "" -and ($Version -ne "" -or $Source)) { Fail "-Zip は、-Version / -Source と一緒には使えません。" }
if ($Zip -ne "" -and -not (Test-Path $Zip -PathType Leaf)) { Fail "zip がありません: $Zip" }
if (Get-Process TakoEditor -ErrorAction SilentlyContinue) { Fail "TakoEditor が動いています。閉じてから実行してください。" }

$project = Get-Content $marker.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
$item    = Get-Item $engine -ErrorAction SilentlyContinue
$current = ""
if (Test-Path "$engine\VERSION") { $current = (Get-Content "$engine\VERSION" -TotalCount 1).Trim() }

if ($item -and $item.LinkType -eq "Junction") {
  $shared = "$($item.Target)"
  Write-Host "このプロジェクトは、エンジンを自分の中に持たず、別の場所のエンジンをリンクで共有しています（何も変更していません）。"
  Write-Host "  共有しているエンジン: $shared （バージョン: $current）"
  Write-Host ""
  Write-Host "エンジンを更新するには:"
  if (Test-Path "$shared\.git") {
    Write-Host "  そのフォルダーで git pull し、project\TakoEngine.sln をビルドし直します（git で取ったエンジンです）。"
  }
  else {
    Write-Host "  新しいリリースの zip（https://github.com/$repository/releases）を、そのフォルダーに開き直します。"
  }
  Write-Host "  同じエンジンを共有している他のプロジェクトも、一緒に新しくなります。"
  Write-Host ""
  Write-Host "このプロジェクトだけにエンジンを持たせるには、リンクを外してから、もう一度 UpdateEngine.bat を実行します:"
  Write-Host "  cmd /c rmdir `"$engine`""
  exit 0
}

$isGit = (Test-Path "$engine\.git") -or ((-not $item) -and $Source)
if ($Zip -ne "" -and $isGit) { Fail "-Zip は、ソース（git）のエンジンには使えません。" }

# 入れるバージョンを決める。-Zip のときは、zip の中の VERSION で決まる
$target = $Version
if ($target -eq "" -and -not $item -and $project.engineVersion) { $target = $project.engineVersion }
if ($Zip -eq "" -and $target -eq "") {
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
if ($Zip -eq "" -and $target -eq $current) {
  Write-Host "TakoEngine $current のままです（入れようとしたバージョンと同じ）。"
  exit 0
}
$from = $current
if ($from -eq "") { $from = "(なし)" }
if ($Zip -eq "") { Write-Host "TakoEngine: $from -> $target" }

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
  $package = $Zip
  if ($package -eq "") {
    # 同時に流した別の UpdateEngine とぶつからないよう、名前にプロセス ID を入れる
    $package = Join-Path $env:TEMP "TakoEngine-$target-$PID.zip"
    $url     = "https://github.com/$repository/releases/download/v$target/TakoEngine-$target.zip"
    Write-Host "ダウンロード: $url"
    # curl は Windows 10 以降に入っている。-L: 転送先を追う。-f: 404 などを失敗にする。-#: 進み具合を 1 行のバーで出す
    curl.exe -L -f -# -o $package $url
    if ($LASTEXITCODE -ne 0) {
      Remove-Item $package -Force -ErrorAction SilentlyContinue
      Fail "ダウンロードできませんでした（curl の終了コード: $LASTEXITCODE）: $url"
    }
  }
  $fresh = "$engine.new"
  if (Test-Path $fresh) { Remove-Item $fresh -Recurse -Force }
  New-Item -ItemType Directory -Force $fresh | Out-Null
  tar -x -f $package -C $fresh
  if ($LASTEXITCODE -ne 0) { Fail "zip を開けませんでした: $package" }
  if (-not (Test-Path "$fresh\VERSION")) { Fail "zip の中身が TakoEngine の SDK ではありません: $package" }
  $target = (Get-Content "$fresh\VERSION" -TotalCount 1).Trim()
  if ($Zip -ne "") { Write-Host "TakoEngine: $from -> $target ($Zip)" }
  # 古い物を消すのは、新しい物を開き終えてから
  if ($item) { Remove-Item $engine -Recurse -Force }
  New-Item -ItemType Directory -Force (Split-Path $engine) | Out-Null
  Move-Item $fresh $engine
  if ($Zip -eq "") { Remove-Item $package -Force }
}

# 前のバージョンより新しい分の変更履歴を出す。Markdown の記号を外して、見出しに色を付ける
$changelog = "$engine\CHANGELOG.md"
if (Test-Path $changelog) {
  $show  = $false
  $shown = $false
  foreach ($line in Get-Content $changelog -Encoding UTF8) {
    if ($line -match '^## \[(\d+\.\d+\.\d+)\](?: - (.+))?') {
      $show = ($current -eq "") -or ([version]$Matches[1] -gt [version]$current)
      if ($show) {
        $heading = "$($Matches[1]) "
        if ($Matches[2]) { $heading = "$($Matches[1])（$($Matches[2])）" }
        Write-Host ""
        Write-Host "━━ $heading━━" -ForegroundColor Cyan
        $shown = $true
      }
    }
    elseif (-not $show -or $line.Trim() -eq "") { }
    elseif ($line -match '^### (.+)') {
      # ゲーム側で直す所は、色を変えて目立たせる
      $color = "Yellow"
      if ($Matches[1] -like "直しが要る変更*") { $color = "Magenta" }
      Write-Host ""
      Write-Host "[$($Matches[1])]" -ForegroundColor $color
    }
    else {
      # `コード` は、記号を外して色を変える（` で分けると、奇数番目がコード）
      $parts = ($line -replace '^- ', '  ・') -split '`'
      for ($i = 0; $i -lt $parts.Count; $i++) {
        if ($i % 2 -eq 1) { Write-Host $parts[$i] -NoNewline -ForegroundColor DarkCyan }
        else { Write-Host $parts[$i] -NoNewline }
      }
      Write-Host ""
    }
  }
  # -Zip はリリース前の版のことがあり、ページが無い
  if ($shown -and $Zip -eq "") {
    Write-Host ""
    Write-Host "Markdown で読む: https://github.com/$repository/releases/tag/v$target"
  }
}

# 古いテンプレート（番号が無い・小さい）では、入れ替えない。実行中でも、PowerShell は全体を読み終えているので上書きできる
$newScript = "$engine\templates\Default\UpdateEngine.ps1"
if ((Test-Path $newScript) -and (Get-Content $newScript -Raw -Encoding UTF8) -match '(?m)^\$scriptRevision\s*=\s*(\d+)' -and [int]$Matches[1] -gt $scriptRevision) {
  Copy-Item $newScript $PSCommandPath -Force
  Write-Host ""
  Write-Host "UpdateEngine.ps1 を、新しいエンジンの物に入れ替えました（プロジェクトの git に差分が出ます）。"
}

# engineVersion の値だけを書き換える（ConvertTo-Json で書き直すと、BOM が付き、キーの順と字下げが変わる）
$text = [IO.File]::ReadAllText($marker.FullName)
[IO.File]::WriteAllText($marker.FullName, ($text -replace '("engineVersion"\s*:\s*)"[^"]*"', "`${1}`"$target`""))
Write-Host ""
Write-Host "TakoEngine $target にしました。ゲームのソリューションをビルドし直してから、エディタを開いてください。" -ForegroundColor Green
