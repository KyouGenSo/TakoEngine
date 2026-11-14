# 変更履歴

エンジンを更新したときに、ゲーム側で直す所が分かるように書く。バージョンごとに「追加」「変更」「直しが要る変更」の順。

`UpdateEngine.bat` は、更新する前のバージョンより新しい分をここから抜き出して表示する。リリースの本文と Discord の通知も、そのバージョンの節をここから抜き出す。見出しは `## [バージョン] - 日付` の形にする。

## [2.1.2] - 2026-11-14

### 追加
- スクリプトのコンパイル中、画面の右下に、動くバーと今コンパイルしているファイル名・経過秒を出す（割合は出さない。MSBuild は全部で何ファイルかを出さないため）
- `UpdateEngine.bat`：ダウンロードの進み具合を、1 行のバーで出す（`curl.exe` を使う。Windows 10 以降に入っている）
- `UpdateEngine.bat`：エンジンを入れた後、エンジンの中のテンプレート（`templates/Default/UpdateEngine.ps1`）の方が新しければ、プロジェクトの `UpdateEngine.ps1` も入れ替える（プロジェクトの git に差分が出るので、コミットする）。`UpdateEngine.bat` は入れ替えない
- リリースを公開したとき、Discord に通知する（`.github/workflows/Release.yml`。ウェブフックの URL は、リポジトリの Secrets の `DISCORD_WEBHOOK`。無ければ送らない）
- ライセンス（MIT。`LICENSE`）。SDK の zip にも入る
- 色の構造体 `Color`（RGBA）・`ColorHSV`・`Color32`（0〜255）と、変換（`ToHSV`・`ToColor`・`ToColor32`・`Color32::FromHex`・`ToHex`）。名前の付いた色（`Color::kRed` など 10 個）、`WithAlpha`・`Lerp`・`LerpHSV`（色相環に沿って補間）・`FromKelvin`（色温度）・`+ - * /`（`ColorFunc.h`）
- `Gradient`（色の段階。`Evaluate(t)` がその位置の色を返す。`TPROPERTY` にすると、Inspector で段階を編集でき、段階の上に色の帯が出る。`Gradient.h`）
- `RandomEngine` の `GetHueColor`（色相だけランダム）・`GetColor(from, to)`（2 色の間のランダム）
- `TPROPERTY` に `Tako::Color` と `Tako::Gradient` を使える。`Color` は、`Color` の指定子が無くても色の欄になる
- 画像ごとの「色の画像か」の設定。Assets の画像の右クリックの Color (sRGB) で切り替え、`resources/Json/TextureSettings.json` に書く（既定のオンと違う物だけ）。オフの画像は、リニアに直さずに読む。エンジンの `noise0.png`・`noise1.png` はオフ
- `Color::ToLinear`・`ToSRGB`（1 を超える色は、色合いを sRGB のまま、強さをリニアの倍率として扱う）。シェーダーの `Common.hlsli` に `SRGBToLinear`・`LinearToSRGB`

### 変更
- スクリプトのコンパイルのエラーと警告で、日本語が文字化けしていたのを直した（MSBuild は UTF-8 で書くのに、Shift_JIS として読んでいた）。行の末尾の `[プロジェクトのパス]` も、落ちるようになった
- エディタでプロジェクトを開くときに、足りない空のフォルダー（`Source`、`resources` の `Texture`・`Model`・`Sound`・`Fonts` など 15 個）を作るようにした（git は空のフォルダーを持てないので、clone した人の所には無かった）。わざと消したフォルダーも、次に開いたときに戻る
- `UpdateEngine.bat`：更新の後に出す変更履歴を、Markdown の記号を外し、見出しに色を付けて出すようにした。最後に、リリースページの URL を出す
- リリースの本文を、`CHANGELOG.md` のそのバージョンの節にした（今までは「See CHANGELOG.md」だけだった）。節が無いタグは、リリースのビルドの前に失敗する
- `README.md` を英語にし、日本語は `README.ja.md` に分けた。特徴・使っているライブラリ・エディタの画像を足し、ロゴの画像を外した
- エンジンの色の引数・戻り値・データを、`Vector4` から `Color` にした（`SetColor`・`SetMaterialColor`・`GetMaterialColor`・`DrawLine` など。GPU に送る構造体の色の欄も）。`Vector4` と `Color` は互いに自動で変わるので、`Vector4` で書いたゲームのコードはそのまま動く。保存の形（4 つの数の配列）は変えていない
- `Color::kWhite`・`kClear` の型と、`Color::FromHex`・`FromHSV` の戻り値を、`Vector4` から `Color` にした（`Color` は名前空間から構造体になった）
- `Vector4` の `==`・`!=` を、メンバーから名前空間の関数にした（`Color` どうしや、`Color` と `Vector4` を比べられるように。中身は同じ）
- マテリアルの定数を CPU のメモリに持ち、描くたびに 1 フレーム用のバッファ（`UploadAllocator`）へ写して渡すようにした
- `TextureManager::GetSRVGPUHandle`（ImGui 用）は、ファイルの値のまま読むビューを返す

### 直しが要る変更（ゲーム側）
- `Color::WithAlpha(色, a)` は `色.WithAlpha(a)` にする
- 三項演算子の左右に `Vector4` と `Color` を混ぜると（`条件 ? vector4 : Color::kWhite` など）、どちらの型にするかが決まらず、コンパイルエラーになる。どちらかにそろえる
- 色の Getter や `Color::kWhite` を `auto` で受けて `.x`〜`.w` を使っている所は、`.r`〜`.a` にする
- 既にあるプロジェクトの `UpdateEngine.ps1`（2.1.1 までの物）は、そのまま動くが、自分を入れ替えない。新しい表示・バー・入れ替えの仕組みを入れるなら、1 回だけ `templates/Default/UpdateEngine.ps1` をプロジェクトのルートへ写し直す（それ以降は、自動で入れ替わる）
- ノイズ・マスク・法線など、色でない画像は、Assets の右クリックで Color (sRGB) を外す
- `Material(dx12, shader)` は `Material(shader)` にする。`TextureManager::ReleaseIntermediateResources()` は `EndFrame()` にする

## [2.1.1] - 2026-11-13

### 変更
- エンジンのプロジェクト名を `02_Engine` から `TakoEngine` にした。`TakoEngine.sln` の最初のプロジェクトを `TakoEditor` にした（開いて F5 を押すと、エディタが起動する）
- エンジンのフォルダーの中には、プロジェクトを作れないようにした（作ると、プロジェクトのエンジンへのリンクが自分の親を指していた）
- プロジェクトを指定せずに `TakoEditor.exe` を起動して作るとき、作る場所と、使うエンジンを出して確かめるようにした。場所を選ぶダイアログは、ドキュメントから始まる
- プロジェクトの中のエンジン（`UpdateEngine.bat` が入れた写し）のエディタで New Project すると、新しいプロジェクトにエンジンを写すようにした（今までは、元のプロジェクトの中のエンジンへリンクしていた。元のプロジェクトがエンジンを更新したり、消えたりすると、巻き込まれていた）
- リンクしたプロジェクトの中のエディタで New Project すると、リンク先の本物のエンジンへ直接リンクするようにした
- `UpdateEngine.bat`：エンジンをリンクで共有しているプロジェクトでは、共有先・更新のしかた（git なら pull してビルド、zip なら開き直す）・このプロジェクトだけにエンジンを持たせる手順を出す
- `UpdateEngine.bat`：ダウンロードする一時ファイルの名前を、実行ごとに変えた（2 つ同時に流すと「ファイルが使用中」で失敗していた）
- テンプレートの vcxproj：エンジンが無い状態（clone した直後など）でビルドすると、「UpdateEngine.bat を実行する」と出して止まるようにした（今までは、設定ファイルが見つからないというエラーだけだった）
- Debug で、デバッグレイヤーを取れなかったときは、DXGI ファクトリに DEBUG を付けないようにした（Windows の「グラフィックス ツール」が入っていない PC で、起動時に止まる恐れがあった）
- `README.md` に、フォルダーの見分け方と始め方（新しいゲームを作る・チームのゲームに参加する・エンジンを開発する）を書いた
- `tools/package_sdk.ps1`：Visual Studio でエンジンを開いたまま流すと終わらなかったのを直した（`.vs` を写そうとして、Visual Studio が掴んでいるファイルを待ち続けていた）。`.vs` と、git の対象外の開発用の物（`Docs` など）は zip に入れず、掴まれたファイルは 2 回やり直して失敗にする
- リリースの手順を 1 つにした `tools/release.ps1` を足した

### 直しが要る変更（ゲーム側）
- 無し。既にあるプロジェクトの `UpdateEngine.ps1` と vcxproj は、作ったときの写しのまま動く。新しい案内と「エンジンが無いときのエラー」を入れるなら、`templates/Default/UpdateEngine.ps1` を写し直し、vcxproj に `TakoCheckEngine`（`templates/Default/GameProject/__ProjectName__.vcxproj` の `InitialTargets`・`Import` の `Condition`・`Target`）を足す

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
