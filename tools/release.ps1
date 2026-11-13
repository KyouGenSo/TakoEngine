# エンジンの新しいバージョンをリリースする。VERSION と CHANGELOG.md を直してコミットしてから流す。
#   pwsh tools/release.ps1            確かめる → 今のブランチを push → その CI を待つ → タグ v<VERSION> を push → リリースができるのを待つ
#   pwsh tools/release.ps1 -DryRun    確かめるだけ（何も push しない）
#
# タグは、ブランチの CI が通ってから出す（出したタグでリリースのビルドが失敗すると、公開したタグを付け直すことになる）。
# zip を作ってリリースに添付するのは、タグの push で動く .github/workflows/Release.yml。
# CI の結果を見るのに gh（GitHub CLI）を使うので、先に gh auth login でログインしておく。
param([switch]$DryRun)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

function Fail($message) {
  Write-Host $message -ForegroundColor Red
  exit 1
}

# commit で動いたワークフローが全部終わるまで待つ。workflow を渡すと、その名前の物だけを見る。1 つでも成功でなければ止める
function Wait-Runs($commit, $workflow) {
  $waited = 0
  while ($true) {
    $json = gh run list --commit $commit --limit 20 --json workflowName,status,conclusion,url | Out-String
    if ($LASTEXITCODE -ne 0) { Fail "gh run list に失敗しました（gh auth status でログインを確かめてください）。" }
    $runs = @($json | ConvertFrom-Json | ForEach-Object { $_ })
    if ($workflow -ne "") { $runs = @($runs | Where-Object { $_.workflowName -eq $workflow }) }
    $running = @($runs | Where-Object { $_.status -ne "completed" })
    if ($runs.Count -gt 0 -and $running.Count -eq 0) {
      $failed = @($runs | Where-Object { $_.conclusion -ne "success" })
      if ($failed.Count -gt 0) {
        Fail ("成功しなかったワークフローがあります:`n" + (($failed | ForEach-Object { "  $($_.workflowName)（$($_.conclusion)） $($_.url)" }) -join "`n"))
      }
      Write-Host ("  成功: " + (($runs | ForEach-Object { $_.workflowName }) -join ", "))
      return
    }
    # push の直後は、まだ一覧に出てこない
    if ($runs.Count -eq 0 -and $waited -ge 120) { Fail "ワークフローが始まりません（この push で動くワークフローがあるかを確かめてください）。" }
    Start-Sleep -Seconds 15
    $waited += 15
    Write-Host "  待っています（$waited 秒）"
  }
}

$version = (Get-Content "$root\VERSION" -TotalCount 1).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { Fail "VERSION が 2.1.0 の形ではありません: $version" }
$tag = "v$version"
if (-not (Select-String -Path "$root\CHANGELOG.md" -Pattern ("^## \[" + [regex]::Escape($version) + "\]") -Quiet)) {
  Fail "CHANGELOG.md に「## [$version]」の見出しがありません。"
}
if (git status --porcelain) { Fail "コミットしていない変更があります。コミットしてから流してください。" }
$branch = git branch --show-current
if (-not $branch) { Fail "ブランチの上で流してください（今は、どのブランチでもないコミットにいます）。" }
if (git tag --list $tag) { Fail "タグ $tag は、もうあります。VERSION を上げてください。" }
if (git ls-remote --tags origin "refs/tags/$tag") { Fail "タグ $tag は、リモートにもうあります。VERSION を上げてください。" }
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { Fail "gh（GitHub CLI）が見つかりません。" }

$commit = git rev-parse HEAD
Write-Host "TakoEngine $version をリリースします（$branch の $($commit.Substring(0, 7))）"
if ($DryRun) {
  Write-Host "確かめた所まで（-DryRun）。何も push していません。" -ForegroundColor Green
  exit 0
}

git push origin $branch
if ($LASTEXITCODE -ne 0) { Fail "ブランチを push できませんでした。" }
Write-Host "ブランチの CI を待ちます"
Wait-Runs $commit ""

git tag -a $tag -m "TakoEngine $version"
git push origin $tag
if ($LASTEXITCODE -ne 0) { Fail "タグを push できませんでした（手元のタグは git tag -d $tag で消せます）。" }
Write-Host "リリースのワークフローを待ちます"
Wait-Runs $commit "Release"

Write-Host ("リリースしました: " + (gh release view $tag --json url -q .url)) -ForegroundColor Green
exit 0
