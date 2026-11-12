# 変更履歴

エンジンを更新したときに、ゲーム側で直す所が分かるように書く。バージョンごとに「追加」「変更」「直しが要る変更」の順。

`UpdateEngine.bat` は、更新する前のバージョンより新しい分をここから抜き出して表示する。見出しは `## [バージョン] - 日付` の形にする。

## [2.1.0] - 2026-11-13

### 追加
- Debug（エディタ）が `TakoEditor.exe` ＋ `TakoEngine.dll` ＋ ゲームの `Game.dll` になった。Release は今までどおり 1 つの exe
- スクリプトのホットリロード。エディタを閉じずに、ビルドし直した `Game.dll` に差し替える（TPROPERTY の値・選択・BT エディタの木は残る。Play 中なら Edit に戻る）
- エディタからのコンパイル（Game Viewport のツールバーのボタン、Debug > Compile Scripts、Ctrl+B）。エラーは Console に出て、行をダブルクリックすると VS で開く
- Auto Compile（Engine Settings > Editor）。エディタのウィンドウが前面に戻ったとき、`Script/`・`Source/` に変更があればコンパイルする
- プロジェクトの作成（File > New Project、`TakoEditor.exe --new 場所 名前`）と、File > Open Project / Open Recent
- プロジェクトの印 `名前.takoproject`（最後に開いたエンジンのバージョンと、保存形式の番号）
- 保存形式の移行の仕組み（`editor/ProjectMigration.cpp` の表）。古い形式のプロジェクトは、バックアップを取ってから直す
- `UpdateEngine.bat`（エンジンを入れる・更新する）。`-Zip <ファイル>` で、手元で作った SDK の zip を入れられる（リリース前のエンジンを試す用）
- 新しいリリースの通知（Engine Settings > Editor > Check for Updates。1 日 1 回）
- 配置ファイル（imgui.ini）が無いときの既定のウィンドウの並び
- クラッシュ対策。未保存のシーンの中身を `Generated/Recovery/` に書いておき（Play の前・コンパイルの前・未保存のまま 2 分ごと）、正常に終了しなかった次の起動で、読み込むか捨てるかを聞く
- 落ちたときに `Generated/Crashes/` へミニダンプを書く（Visual Studio で開くと、落ちた場所と呼び出し履歴が見える）。落ちた次の起動は、Play ではなく Edit で始まる
- 配る用の zip（SDK）を作る `tools/package_sdk.ps1` と、zip だけでプロジェクトを作ってビルドできるかを確かめる `tools/test_sdk.ps1`。タグ `v*` の push で両方を流してリリースに添付するワークフロー（`.github/workflows/Release.yml`）

### 変更
- エンジンのビルド結果は、エンジンのフォルダーの `bin/`・`obj/` に出る（ゲームの `Generated/` には出ない）
- エンジンの include パスの一覧は `build/TakoEngineCommon.props` の 1 か所になった。engine 以下にフォルダーを足したら、ここに 1 行足す
- ゲームのビルドは、エンジンのライブラリを `Engine/TakoEngine/bin/` から直接リンクする（プロジェクト参照は要らない）
- `RandomEngine` の乱数ジェネレータは `.cpp` の中に移した（使い方は変わらない）
- Debug でエンジンのシェーダーのソースを探す場所を、ビルドしたときのパスではなく、動いているエンジンのフォルダーから決めるようにした（SDK は、SDK の中のシェーダーを読む）

### 直しが要る変更（ゲーム側）
- ゲームの `.vcxproj`：Debug の `ConfigurationType` を `DynamicLibrary` にする。エンジン・imgui・DirectXTex への `ProjectReference` を消す
- ゲームの `.sln`：エンジンのプロジェクトを持つなら `editor_app/TakoEditor.vcxproj` を足し、ゲームのプロジェクトの依存に入れる（エンジンのプロジェクトを持たない形でもよい）
- プロジェクトのルートに `名前.takoproject`（`{ "engineVersion": "2.1.0", "formatVersion": 1 }`）を置く
- Debug で起動する exe の名前が `TakoEditor.exe` になった。exe の名前で設定している外部のツール（ウィンドウマネージャーの除外など）は直す
- エンジンのクラスを増やすときは `class TAKO_API 名前` と書く（TCLASS に無ければ、ビルドの時点でエラーになる）
- ゲームがエンジンの全体の表に登録した物（`RegisterPostEffect`・`RenderPipeline::AddRenderPass`・`TransitionManager::SetCurrentEffect`）は、`EndPlay` で外す。ホットリロードで古い DLL のコードを指したまま残るため
