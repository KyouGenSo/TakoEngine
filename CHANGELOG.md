# 変更履歴

## [2.1.4] - 2026-11-20

### 追加
- ビルドの構成 Develop。Debug と同じくエディタ（`TakoEditor.exe`）で動き、スクリプトのコンパイルと差し替えもできる。C++ の最適化が効いていて、DirectX のデバッグレイヤーを使わないので、Debug より速い（エンジンのテスト用のシーンで、1 フレームが約 4 ms から約 1.3 ms になった）。普段の作業は Develop、デバッガーで 1 行ずつ追うときと、デバッグレイヤーの検査が要るときは Debug を使う
  - Visual Studio の構成を Develop にして実行すると、Develop のエディタが開く。ゲームの DLL は `Generated\outputs\Develop\Game.dll` にできる
  - `assert` は Develop でも効く
  - エディタのメニューバーの中央に、構成の名前が出る（`TakoEngine 2.1.4 (Develop)`）
- マクロ `TAKO_EDITOR`。エディタがある構成（Debug と Develop）で定義される。`_DEBUG` は今までどおり、Debug だけで定義される
- 描画の時間の計測。Render Pipeline ウィンドウの Frame タブに、手順ごとの描画回数・CPU の時間（描画のコマンドを積むのに掛かった時間）・GPU の時間が出る（直近 0.5 秒の、1 フレームあたりの平均）
  - 影は視点ごと（Cascade・Local）、ポストエフェクトはエフェクトごと、パーティクルは計算（Simulate）と描画（Draw）に分かれる。`RenderPipeline::AddRenderPass` で足したパスも、その名前の行で出る。エディタの分（Debug Viewport・プレビュー・ImGui）も出る
  - Frame タブの Measure のチェックを外すと、計測を止める（保存はされず、エディタを起動するたびにオンに戻る）
  - Debug のエディタでは、デバッグレイヤーと、シェーダーを最適化しないぶん、時間が長く出る。実際の値は Develop のエディタで見る
- Performance ウィンドウの Frame Time。1 フレームの内訳（CPU Update・CPU Render・GPU・Wait for GPU・Idle）が出る。FPS の上限で待っている時間（Idle）と、実際に掛かっている時間を分けて見られる
  - その下の CPU Update の欄に、更新の内訳が出る。コンポーネントとスクリプトは型ごと（スクリプトはクラスの名前）に 1 行で、Tick と LateTick を合わせた時間と、その型の有効なコンポーネントの個数が出る。どのスクリプトが重いかを見られる
  - 当たり判定は Collision の行に出る。その下の Callbacks は、`OnCollisionEnter` などの通知に掛かった時間（受け取る側のコードの時間）
- 起動の引数 `--profile`。Release の exe に付けて起動すると、同じ計測の結果を画面の左上に、FPS を右上に重ねて出す（付けなければ計測しない）
  - 左上には表が 2 つ並ぶ（左：1 フレームの内訳と更新の内訳、右：描画の内訳）。更新の内訳の型ごとの行は、時間の長い順に 5 個まで出る
  - 新しく作るプロジェクトのルートには `RunProfile.bat` が入っている。実行すると、Release の exe を `--profile` 付きで起動する（先に Release をビルドしておく）。今あるプロジェクトでは、エンジンのひな形 `Engine\TakoEngine\templates\Default\RunProfile.bat` をプロジェクトのルートに写し、中の `__ProjectName__` をすべて exe の名前（拡張子なし）に替える

### 変更
- シェーダーが使う値を渡さずに描いたときに止まる検査は、Debug と Develop の両方で動く。足りない名前は、エディタの Console と Visual Studio の出力に出る（今までは、止まったときの窓に出ていた）

### 直しが要る変更（ゲーム側）
- エディタ用のコード（`DrawCustomImGui`・`#include "imgui.h"` など）を囲んでいる `#ifdef _DEBUG` を、`#ifdef TAKO_EDITOR` に替える。替えなくてもビルドは通り、Debug と Release は今までどおり動くが、Develop のエディタではその表示が出ない
- 今あるプロジェクトで Develop を使うには、プロジェクトに構成を足す（新しく作るプロジェクトには最初から入っている。足さなくても Debug と Release は今までどおり使えるが、Develop のエディタでは、ゲームのスクリプトを読めず、コンパイルもできない）
  - `GameProject\<名前>.vcxproj`：エンジンのひな形 `Engine\TakoEngine\templates\Default\GameProject\__ProjectName__.vcxproj` で `Develop|x64` が付いている 2 つの組（`ProjectConfiguration` と `PropertyGroup`）を、それぞれ Debug の組の後ろに写す
  - `<名前>.sln`：ひな形 `Engine\TakoEngine\templates\Default\__ProjectName__.sln` の `Develop|x64` の 3 行を写す（`{__GUID__}` は、そのプロジェクトの Debug の行と同じ物にする）
  - `<名前>.slnx` を使っているプロジェクト：`<Configurations>` の中に `<BuildType Name="Debug" />`・`<BuildType Name="Develop" />`・`<BuildType Name="Release" />` を足す

## [2.1.3] - 2026-11-20

### 追加
- 平行光源の影のカスケード。カメラからの距離ごとに、1〜4 枚（既定 4）のシャドウマップに分けて描くので、近くの影が細かくなる。Engine Settings > Rendering > Shadows の Cascade Count・Cascade Split（`ProjectSettings.json` の Rendering の `ShadowCascadeCount`・`ShadowCascadeSplit`）
- 点光源とスポットライトの影。Light コンポーネントの Cast Shadow をオンにしたライトが影を落とす（既定はオフ）。コードからは `Light::AddPointLight(light, true)`・`AddSpotLight(light, true)`
  - 影を落とせるのは、スポットライトを 1 枚・点光源を 6 枚と数えて、合わせて 16 枚まで。足りないときは、カメラから遠いライトが影なしになり、Console に 1 回出る
  - 解像度は Engine Settings の Local Shadow Map Size（`LocalShadowMapSize`。既定 2048 で、1 枚は 512）
- Engine Settings > Rendering > Shadows の Stagger Far Cascades（`ShadowStaggerFarCascades`。既定オン）。3・4 枚目のカスケードを 1 フレームおきに描き直して、影の描画を減らす。その範囲で動く物の影は 1 フレーム遅れる
- Engine Settings > Rendering > Shadows の Show Cascades（カスケードごとの色分け）と Shadow Maps（シャドウマップの画像）
- 自作のシェーダーで影を受けるための `Shadow.hlsli`（`DirectionalShadow`・`SpotShadow`・`PointShadow`）
- 視錐台カリング。カメラに映らない位置にあるモデル（Mesh コンポーネント・`Object3d`）を描かない。ゲームの画面・Debug Viewport・エディタのプレビューで、それぞれの視点で決める。影を描くときも、影の範囲の外のモデルは描かない
  - スキニング付きのモデルは、今のポーズで決める
  - `InstancedObject3d`・スプライト・文字・スカイボックスは、今までどおり常に描く
  - 自作の頂点シェーダーで頂点をメッシュの外へ大きく動かしているモデルは、画面の端で消えることがある。そのときは Engine Settings > Rendering > Culling の Frustum Culling をオフにする（全部描く。`ProjectSettings.json` の Rendering の `FrustumCulling`、コードからは `Renderer::SetFrustumCulling`。既定オン）
- Debug Viewport のツールバーの「Cull with the game camera」。オンにすると、ゲームのカメラで描かれないモデルが Debug Viewport でも消える（何がカリングされているかを外から見る）

### 変更
- `UpdateEngine.bat`：新しいリリースがあるのに「リリース（v で始まるタグ）が見つかりません。」と出て止まることがあったのを直した（同じネットワークから GitHub へ 1 時間に 60 回を超えて問い合わせると起きていた）。GitHub に繋がらないときは、そう出す
- 影の見た目が変わる
  - カメラを動かしても、止まっている物の影の縁がちらつかない
  - 影の距離（Light コンポーネントの Shadow Distance）の手前 10% で薄くなって消える（今までは急に切れていた）
  - 1 枚板のような閉じていないメッシュも、影を落とす
  - 影の範囲からライト側へ遠く離れた物（高い所の物など）の影が、切れなくなった
  - 平行光源の向きを変えたフレームから、影もその向きになる（今までは 1 フレーム遅れていた）
- Shadow Map Size は、カスケードが 2 枚以上のとき、1 枚がその半分の辺になる（2048 なら 1024）。遠くの影が粗いときは 4096 にする
- PCF Kernel Size は 7x7 までになった（9x9 を保存してあれば 7x7 になる）
- 影のバイアスの設定は、Depth Bias・Normal Bias（どちらもテクセル単位。`ShadowDepthBias`・`ShadowNormalBias`）になった。`ProjectSettings.json` の `ShadowBias` は読まない（既定の値になる）

### 直しが要る変更（ゲーム側）
- 自作の頂点シェーダーで、`VertexShaderOutput` の `lightSpacePos` に値を入れている行は消す（`Object3d.hlsli`・`Object3dInstanced.hlsli` から無くなった。残っているとコンパイルエラーになる）
- 自作のピクセルシェーダーで `gShadowConstants`・`gShadowMap` を直接読んでいる物は、`Shadow.hlsli` を include して `DirectionalShadow(worldPos, normal, gDirectionalLight.direction)` の戻り値（1 = 光が当たる、0 = 影）を使う形に直す
- `Light` の `SetAutoUpdatePosition`・`SetSceneCenter`・`SetDirectionalLightPosition`・`UpdateDirectionalLightShadowMatrices` と、`GetDirectionalLight()` の `viewMatrix`・`projMatrix`・`viewProjMatrix`・`position` は無くなった（影の範囲はカメラから自動で決まる）。呼んでいる行は消す
- `ShadowRenderer::SetShadowBias`・`GetShadowBias` は、`SetDepthBias`・`SetNormalBias`（テクセル単位）に替える

## [2.1.2] - 2026-11-15

### 追加
- スクリプトのコンパイル中、画面の右下に、動くバーと今コンパイルしているファイル名・経過秒を出す
- `UpdateEngine.bat`：ダウンロードの進み具合を、1 行のバーで出す
- `UpdateEngine.bat`：エンジンを入れた後、エンジンの中のテンプレート（`templates/Default/UpdateEngine.ps1`）の方が新しければ、プロジェクトの `UpdateEngine.ps1` も入れ替える（プロジェクトの git に差分が出るので、コミットする）。`UpdateEngine.bat` は入れ替えない
- ライセンス（MIT。`LICENSE`）。SDK の zip にも入る
- 色の構造体 `Color`（RGBA）・`ColorHSV`・`Color32`（0〜255）と、変換（`ToHSV`・`ToColor`・`ToColor32`・`Color32::FromHex`・`ToHex`）。名前の付いた色（`Color::kRed` など 10 個）、`WithAlpha`・`Lerp`・`LerpHSV`（色相環に沿って補間）・`FromKelvin`（色温度）・`+ - * /`（`ColorFunc.h`）
- `Gradient`（色の段階。`Evaluate(t)` がその位置の色を返す。`TPROPERTY` にすると、Inspector で段階を編集でき、段階の上に色の帯が出る。`Gradient.h`）
- `RandomEngine` の `GetHueColor`（色相だけランダム）・`GetColor(from, to)`（2 色の間のランダム）
- `TPROPERTY` に `Tako::Color` と `Tako::Gradient` を使える。`Color` は、`Color` の指定子が無くても色の欄になる
- リニアの HDR の描画。シーンを `R16G16B16A16_FLOAT` に描き（1 を超える明るさも残る）、ポストエフェクトの最後に露出とトーンマップを掛けて sRGB にする。トーンマップは `None`（1 で切る。既定）と `ACES`。Engine Settings > Rendering > Output の Tone Mapping・Exposure、`RenderPipeline::SetToneMapping`・`SetExposure`（`ProjectSettings.json` の Rendering の `ToneMapping`・`Exposure`）
- 画像ごとの「色の画像か」の設定。Assets の画像の右クリックの Color (sRGB) で切り替え、`resources/Json/TextureSettings.json` に書く（既定のオンと違う物だけ）。オフの画像は、リニアに直さずに読む。エンジンの `noise0.png`・`noise1.png` はオフ
- `Color::ToLinear`・`ToSRGB`（1 を超える色は、色合いを sRGB のまま、強さをリニアの倍率として扱う）。シェーダーの `Common.hlsli` に `SRGBToLinear`・`LinearToSRGB`
- `TPROPERTY` の指定子 `HDR`（色の欄で 1 を超える値を入れられる）。Particle Editor の Start Color・End Color も 1 を超えられる

### 変更
- スクリプトのコンパイルのエラーと警告で、日本語が文字化けしていたのを直した。行の末尾の `[プロジェクトのパス]` も、落ちるようになった
- エディタでプロジェクトを開くときに、足りない空のフォルダー（`Source`、`resources` の `Texture`・`Model`・`Sound`・`Fonts` など 15 個）を作るようにした（git は空のフォルダーを持てないので、clone した人の所には無かった）。わざと消したフォルダーも、次に開いたときに戻る
- `UpdateEngine.bat`：更新の後に出す変更履歴を、Markdown の記号を外し、見出しに色を付けて出すようにした。最後に、リリースページの URL を出す
- `README.md` を英語にし、日本語は `README.ja.md` に分けた
- エンジンの色の引数・戻り値・データを、`Vector4` から `Color` にした（`SetColor`・`SetMaterialColor`・`GetMaterialColor`・`DrawLine` など）。`Vector4` と `Color` は互いに自動で変わるので、`Vector4` で書いたゲームのコードはそのまま動く。保存の形（4 つの数の配列）は変えていない
- `Color::kWhite`・`kClear` の型と、`Color::FromHex`・`FromHSV` の戻り値を、`Vector4` から `Color` にした（`Color` は名前空間から構造体になった）
- 画像が、ファイルどおりの色で出るようになった（今までは暗く出ていた。128 が 55 になっていた）。Assets のサムネイルと、`TextureManager::GetSRVGPUHandle` で ImGui に出す画像も同じ
- 色の値（`Color`・Inspector・JSON・16 進数）は、今までどおり sRGB で書く。シーンへ描くときにエンジンがリニアに直す（マテリアルの定数とポストエフェクトの `gParam` のうち名前に color を含む float3/float4、ライト・デカール・線・粒子・インスタンシングの色、Bloom の tint、クリア色）
- UI（描画キューが Overlay 以上）と、AfterPostProcess・BeforeUI・AfterUI に差し込むパスとポストエフェクトは、今までどおりガンマで描く（色も画像も値のまま。半透明の重なり方も今と同じ）。postEffect をオンにした UI（UILayout）には、シーンと一緒にトーンマップも掛かる（None なら値は今と同じ）
- ライト・半透明の重なり・加算・ブルーム・空の見た目が変わる（リニアで計算するため。ライトの当たり方は明るく柔らかくなる）
- glTF（`.gltf`・`.glb`）のベースカラーを、仕様どおりリニアの値として読む（今までより明るく、仕様どおりの色で出る。Inspector には見た目どおりの値が出る）
- エディタのプレビュー（Debug Viewport・各エディタ）も、本編と同じトーンマップで出す
- Release で、1 フレームに約 16,000 回を超えて描くと、メモリを壊す恐れがあったのを直した
- Particle Editor で、動いているエミッターを開いたりクリックしたりしただけで未保存（` *`）になり、終了時に確認が出ていたのを直した。値を戻したときや、Undo で保存した所まで戻したときも、` *` が消える

### 直しが要る変更（ゲーム側）
- `Color::WithAlpha(色, a)` は `色.WithAlpha(a)` にする
- 三項演算子の左右に `Vector4` と `Color` を混ぜると（`条件 ? vector4 : Color::kWhite` など）、どちらの型にするかが決まらず、コンパイルエラーになる。どちらかにそろえる
- 色の Getter や `Color::kWhite` を `auto` で受けて `.x`〜`.w` を使っている所は、`.r`〜`.a` にする
- 既にあるプロジェクトの `UpdateEngine.ps1`（2.1.1 までの物）は、そのまま動くが、自分を入れ替えない。新しい表示・バー・入れ替えの仕組みを入れるなら、1 回だけ `templates/Default/UpdateEngine.ps1` をプロジェクトのルートへ写し直す（それ以降は、自動で入れ替わる）
- 見た目が変わるので、ライトの色と強さ、半透明・加算のエフェクト、ブルームのしきい値と強さ、0〜1 を前提にしたポストエフェクト（BWFilter・HalfTone・WhiteNoise・GrayScale など）を見直す
- ノイズ・マスク・法線など、色でない画像は、Assets の右クリックで Color (sRGB) を外す
- 自作のシェーダーには、リニアの値が入る。gMaterial・gParam の、名前に color を含む float3/float4 は自動で直る。それ以外の色は `Common.hlsli` の `SRGBToLinear` で直す
- .shader.json の途中のパスの Format に 8 bit の形式を指定したポストエフェクトは、1 を超える明るさが切れる。残すなら Format を外す（入力と同じになる）か、`R16G16B16A16_FLOAT` にする
- `Light::GetDirectionalLight()` の `color` は、GPU に渡すリニアの値になった。その色を `SetDirectionalLight`・`SetDirectionalLightColor` に渡し直すときは、`ToSRGB()` で戻してから渡す（そのまま渡すと 2 回直されて暗くなる）

## [2.1.1] - 2026-11-13

### 変更
- エンジンのフォルダーの中には、プロジェクトを作れないようにした（作ると、プロジェクトのエンジンへのリンクが自分の親を指していた）
- プロジェクトを指定せずに `TakoEditor.exe` を起動して作るとき、作る場所と、使うエンジンを出して確かめるようにした。場所を選ぶダイアログは、ドキュメントから始まる
- プロジェクトの中のエンジン（`UpdateEngine.bat` が入れた写し）のエディタで New Project すると、新しいプロジェクトにエンジンを写すようにした（今までは、元のプロジェクトの中のエンジンへリンクしていた。元のプロジェクトがエンジンを更新したり、消えたりすると、巻き込まれていた）
- リンクしたプロジェクトの中のエディタで New Project すると、リンク先の本物のエンジンへ直接リンクするようにした
- `UpdateEngine.bat`：エンジンをリンクで共有しているプロジェクトでは、共有先・更新のしかた（git なら pull してビルド、zip なら開き直す）・このプロジェクトだけにエンジンを持たせる手順を出す
- `UpdateEngine.bat`：ダウンロードする一時ファイルの名前を、実行ごとに変えた（2 つ同時に流すと「ファイルが使用中」で失敗していた）
- テンプレートの vcxproj：エンジンが無い状態（clone した直後など）でビルドすると、「UpdateEngine.bat を実行する」と出して止まるようにした（今までは、設定ファイルが見つからないというエラーだけだった）
- Windows の「グラフィックス ツール」が入っていない PC で、Debug の起動時に止まる恐れがあったのを直した
- `README.md` に、フォルダーの見分け方と始め方（新しいゲームを作る・チームのゲームに参加する・エンジンを開発する）を書いた

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
- 保存形式の移行。古い形式のプロジェクトを開くと、`resources/Json` の写しを `Generated/Backup` に取ってから、今の形式に直す
- `UpdateEngine.bat`（エンジンを入れる・更新する）。`-Zip <ファイル>` で、手元で作った SDK の zip を入れられる（リリース前のエンジンを試す用）
- 新しいリリースの通知（Engine Settings > Editor > Check for Updates。1 日 1 回）
- 配置ファイル（imgui.ini）が無いときの既定のウィンドウの並び
- クラッシュ対策。未保存のシーンの中身を `Generated/Recovery/` に書いておき（Play の前・コンパイルの前・未保存のまま 2 分ごと）、正常に終了しなかった次の起動で、読み込むか捨てるかを聞く
- 落ちたときに `Generated/Crashes/` へミニダンプを書く（Visual Studio で開くと、落ちた場所と呼び出し履歴が見える）。落ちた次の起動は、Play ではなく Edit で始まる

### 変更
- エンジンのビルド結果は、エンジンのフォルダーの `bin/`・`obj/` に出る（ゲームの `Generated/` には出ない）
- ゲームのビルドは、エンジンのライブラリを `Engine/TakoEngine/bin/` から直接リンクする（プロジェクト参照は要らない）

### 直しが要る変更（ゲーム側）
- ゲームの `.vcxproj`：Debug の `ConfigurationType` を `DynamicLibrary` にする。エンジン・imgui・DirectXTex への `ProjectReference` を消す
- ゲームの `.sln`：エンジンのプロジェクトを持つなら `editor_app/TakoEditor.vcxproj` を足し、ゲームのプロジェクトの依存に入れる（エンジンのプロジェクトを持たない形でもよい）
- プロジェクトのルートに `名前.takoproject`（`{ "engineVersion": "2.1.0", "formatVersion": 1 }`）を置く
- Debug で起動する exe の名前が `TakoEditor.exe` になった。exe の名前で設定している外部のツール（ウィンドウマネージャーの除外など）は直す
- ゲームがエンジンの全体の表に登録した物（`RegisterPostEffect`・`RenderPipeline::AddRenderPass`・`TransitionManager::SetCurrentEffect`）は、`EndPlay` で外す。ホットリロードで古い DLL のコードを指したまま残るため
