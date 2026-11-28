# 変更履歴

## [2.1.6] - 2026-11-27

### 追加・変更
- 物理エンジン（Jolt Physics）を追加。当たり判定はこの上で動く。Collider コンポーネントの関数は、今までどおり使える
- `OnCollisionEnter` / `OnCollisionStay` / `OnCollisionExit` の引数を、`const Tako::Collision& collision` にした（Unity の `Collision` に当たる）。相手の Actor（`collision.actor`）に加えて、触れた位置・相手の面の向き・速度の差・ぶつかった強さ（`impulse`、Enter だけ）が渡される
- Rigidbody コンポーネントを追加。Dynamic は重力と当たりで動き、Kinematic は Actor の動きに付いていって Dynamic を押す。形は、同じ Actor と子の Actor の Collider（Trigger を除く）をまとめた物
- Rigidbody は、スクリプトの `AddForce`・`AddImpulse`・`AddTorque`・`SetLinearVelocity`・`SetAngularVelocity` で動かす。質量・減衰・重力の倍率・動きを止める軸・連続衝突（速くても通り抜けない）は Inspector で決める
- Collider に `Is Trigger` を追加。オン（今までの動き）は重なりを知らせるだけ、オフ（新しく作る Collider の既定）は押し合う実体になる。形に Capsule、滑りにくさの Friction、跳ね返りの Restitution、スクリプトから形を選ぶ `SetShape` を追加
- Collider の形に Mesh と Convex Hull を追加。Collider の Model の欄のモデル（空なら、同じ Actor の Mesh コンポーネントのモデル）から形を作る。Mesh は動かない物と Kinematic 用で、Dynamic の Rigidbody には Convex Hull を使う
- Character Controller コンポーネントを追加。カプセルの形のキャラクターが、壁と床に沿って動き、坂と段差を登る。スクリプトは `SetVelocity`・`GetVelocity`・`IsGrounded` で動かす（重力は物理が足す）
- Character Controller は、歩いて当たった Dynamic の Rigidbody を押す。押す強さ（Push Strength）と、上に乗った物に掛かる重さ（Mass）は Inspector で決める
- スクリプトに `OnControllerColliderHit(const Tako::ControllerColliderHit& hit)` を追加（Unity の同じ名前の関数に当たる）。同じ Actor の Character Controller が実体（壁・床・Rigidbody・他の Character Controller）に触れている間、物理のステップごとに呼ばれ、相手の Actor・位置・面の向き・動こうとしていた向きが渡される
- Joint コンポーネントを追加。Rigidbody を、他の Rigidbody かワールドにつなぐ（位置と向きを固める Fixed、1 点でつなぐ Point、軸の周りだけ回る Hinge、距離を保つ Distance）
- Collider の大きさと Offset に、Actor の Scale が掛かる（Unity と同じ決まり。Box は軸ごと、Sphere は 3 軸の最大、Capsule の半径は X と Z の大きい方・高さは Y）。Play 中に Scale を変えても付いていく
- スクリプトに `FixedTick(float fixedDeltaTime)` を追加。物理が 1 ステップ（1/60 秒）進む直前に呼ばれる（1 フレームに 0 回のことも、2 回以上のこともある）
- `PhysicsWorld::GetInstance()->Raycast(始点, 向き, 長さ, 層のマスク)` を追加。最初に当たった Actor・位置・法線・距離が返る。Trigger にも当たり、5 番目の引数を `false` にすると素通りする
- レイキャストは、始点を中に含む Collider（自分の Collider など）と、Mesh の形の裏面には当たらない（Unity と同じ決まり）
- `PhysicsWorld` に、形を飛ばす `SphereCast`・`BoxCast`・`CapsuleCast`、重なっている Actor を調べる `OverlapSphere`・`OverlapBox`・`OverlapCapsule`、線が当たる全部を近い順に返す `RaycastAll` を追加。飛ばす前から重なっている Collider には当たらない（Unity と同じ決まり）
- Rigidbody に、位置を決めて掛ける `AddForceAtPosition`・`AddImpulseAtPosition`、寝ているかを調べる・起こす・寝かせる `IsSleeping`・`WakeUp`・`Sleep`、重心をずらす Inspector の `Center Of Mass`（`SetCenterOfMass`）を追加
- 動きが止まって寝た Rigidbody の組には、`OnCollisionStay` が届かない（Unity と同じ決まり。離れたのではないので Exit は届かず、起きるとまた届く）。届け続けたいときは、`FixedTick` で `WakeUp()` を呼ぶ
- Character Controller に、足元を調べる `GetGroundNormal`・`GetGroundActor`・`IsOnSteepGround`、Play 中に全長を変える `SetHeight`（しゃがむ・立つ。ふさがっていれば `false`）、動く床に付いていく Inspector の `Ride Moving Ground`（既定はオフ）を追加
- Joint でつないだ 2 つの Rigidbody は、お互いに当たらない（Unity と同じ決まり）。当てるには、Joint の `Enable Collision` をオンにする
- Joint に、Hinge のモーター（`Motor Speed`・`Motor Torque`、スクリプトの `SetMotor`）と摩擦（`Friction`）、Distance のばね（`Spring`・`Damper`）、強い力で外れる `Break Force` を追加。外れると、その Actor のスクリプトの `OnJointBreak(float breakForce)` が呼ばれる
- `PhysicsWorld::GetInstance()->IgnoreCollision(actorA, actorB)` を追加。その 2 つの Actor は押し合わず、当たりの知らせも届かない（弾が、撃った本人に当たらないようにする、など）
- Debug > Collider Visibility（Engine Settings > Physics の Collider Debug Draw）で、Play 中のコライダーの形とジョイントが線で出る（今までは、オンにしても何も出なかった）
- エディタで Actor を選ぶと、Collider（Sphere・Box・Capsule）と Character Controller の形が、Debug Viewport に緑の線で出る（Edit 中にも出る）
- 層の当たり表（Engine Settings > Physics の Collision Matrix）の既定を、Unity と同じにした。決めていない組は当たり、足した層は最初から全部の層と当たる（今までは、チェックを入れた組だけが当たった）
- Engine Settings > Physics に、重力の Gravity を追加
- Engine Settings > Physics（と Collision Debug の窓）に、Collider Debug Draw の選択肢を追加。色分け（`Debug Color`。`Sleep` なら、寝ている Rigidbody が赤で出る）と、速度・重心・ボディを囲む箱の線を足せる
- Collider・Rigidbody・Joint の Inspector に、今の設定で使う欄だけが出る（Shape が Box なら Size、Body Type が Kinematic なら Mass などは出ない、など）。隠れた欄の値は残る
- TPROPERTY に `ShowIf = 式` を追加。式が `false` の間、その欄を Inspector に出さない（例：`TPROPERTY(Edit, ShowIf = usesTimer_)`）

### 直しが要る変更（ゲーム側）
- エンジンを入れ替えたら、エディタでプロジェクトを 1 回開く。保存形式の変換の確認が出て、今あるシーンとプレハブの Collider に `Is Trigger` のオンが付く（今までどおり、重なりを知らせる）。変換されたファイルはコミットする
- `CollisionManager`・`Collider`・`AABBCollider`・`OBBCollider`・`SphereCollider` を削除。これらの `#include` と前方宣言、`CollisionManager::GetInstance()->Reset()` と `DrawColliders()` の呼び出しを消す。層の関数（`SetCollisionMask` など）は `PhysicsWorld::GetInstance()` にある
- コードで足した Collider（`AddComponent<ColliderComponent>()`）は、押し合う実体になる。重なりを知らせるだけにするには `SetTrigger(true)` を呼ぶ。Trigger でも Rigidbody でもない Collider どうしは、当たっても知らされない
- 名前を付けていない層の Collider（層を選んでいない Collider の層 31 など）は、全部の層と当たる（今までは、層 31 どうしだけ）。当てたくない組は、Engine Settings の Physics で層に名前を付けて、Collision Matrix のチェックを外す
- コードの `SetCollisionMask(a, b, true)` で当たる組を決めていたゲームは、先に `PhysicsWorld::GetInstance()->ClearCollisionMasks()` を呼ぶ（最初は、全部の層どうしが当たる）
- `OnCollisionEnter` / `OnCollisionStay` / `OnCollisionExit` の引数を、`Tako::Actor* other` から `const Tako::Collision& collision` に変える（ヘッダーと .cpp の両方）。中で使っている `other` は `collision.actor` に置き換えるか、関数の頭に `Tako::Actor* other = collision.actor;` を足す
- `OnCollisionEnter`・`OnCollisionStay`・`OnCollisionExit` は、物理のステップ（1/60 秒ごと）の後に届く（Unity と同じ決まり）。`Tick` で動かした・`SetActive(true)` にした Collider の Enter は次のフレームになり、時間の倍率が 0 の間（ヒットストップ）は Enter も Stay も届かない
- `OnCollisionStay` は、1 フレームに 0 回のことも 2 回以上のこともある（今までは毎フレーム 1 回）。中で毎回足している値（ダメージ・タイマーなど）は、1 回を `Tako::PhysicsWorld::kFixedDeltaTime` 秒として数える
- Collider の付いた Actor の Scale が 1 でないときは、Collider の Size・Radius・Offset を Scale で割った値に直す（今までは Scale が掛からなかった）

## [2.1.5] - 2026-11-27

### 追加・変更
- 描画物用のシェーダー `HalfTone` を追加。光が弱い所ほど大きいドットで陰を描く（平行光源・点光源・スポットライトと影に反応する）。Material Editor で Shader に HalfTone を選んだマテリアルを作り、Mesh コンポーネントの Material に指定する
- 自作のシェーダーでライトを読むための `Light.hlsli` を追加（ライトの構造体と宣言、距離の減衰の `PointLightAttenuation`・`SpotLightAttenuation`）
- マテリアルを差し替えたモデル（Mesh コンポーネントの Material・`Object3d::SetMaterial`）は、そのマテリアルが `gTexture` を指定していなければ、モデルのテクスチャで描く（今までは白）。マテリアルや Mesh コンポーネントの Textures に画像を指定すれば、今までどおりその画像で描く
- シーンを、Actor ごとのファイルに保存するようにした（`Scenes\名前.scene` フォルダーに Actor 1 つが 1 ファイル、並びは `order.txt`）。チームで同じシーンに別々の Actor を足しても、git で競合しない。今までのシーンはそのまま開け、保存すると新しい形になる
- 新しく作る Actor の ID を、連番から乱数にした（別々のブランチで足した Actor の ID が重ならない）。今ある Actor の ID は変わらない
- 引数なしで作れない `TCLASS`（コンストラクタに引数がある、引数ありのコンストラクタしか無い型を初期値なしでメンバーに持つ）は、ビルドでエラーになる。今までは、エラーなしで Add Component の一覧に出なくなっていた
- Material Editor・Shader Editor・PostEffect Editor・UI Editor・Camera Animation Editor・Animation Editor に、開いている物を消す Delete ボタンを追加（確認の後、ファイルをごみ箱へ移す）。シーンが読み込んでいた物も外れる。マテリアルか PostEffect のプロファイルが使っているシェーダーは消せない（使っている物の名前が出る）
- Material Editor の New は、既にあるマテリアルの名前では作れない（今までは、確認なしで初期値に上書きしていた）

### 直しが要る変更（ゲーム側）
- シーンを保存すると `名前.scene` フォルダーができるので、`名前.scene.json` と一緒にコミットする。チームで使っているシーンは、全員のシーンの変更を取り込んでから、1 人が保存し直してコミットする（古い形のシーンを直しているブランチが残っていると、マージで競合する）
- リポジトリの `.gitattributes` に `**/Scenes/*.scene/order.txt merge=union` を足す（ひな形 `Engine\TakoEngine\templates\Default\.gitattributes` と同じ行）。足すと、2 人が同じシーンに Actor を足したときに、並びのファイルも自動でマージされる
- ポストエフェクトの `HalfTone` を削除（上の描画物用のシェーダーに置き換えた）。プロファイルに HalfTone があれば、読み込んだときに外れるので保存し直す。コードで `HalfToneParam` を使っている所は消す
- 差し替えたマテリアルで、テクスチャ付きのモデルを白のまま描きたいときは、マテリアルの Textures の `gTexture` に `white.png` を指定する
- ビルドで「TCLASS 名前 を引数なしで作れない」と出たら、そのクラスのコンストラクタから引数を外すか、原因のメンバーに宣言で初期値を渡す（`Foo foo_{ 1, 2 };`）

## [2.1.4] - 2026-11-20

### 追加・変更
- ビルドの構成 Develop を追加。最適化が効いたエディタで、Debug より速く動く（エンジンのテスト用のシーンで、1 フレームが約 4 ms から約 1.3 ms）。普段の作業は Develop、デバッガーで 1 行ずつ追うときは Debug を使う
- マクロ `TAKO_EDITOR` を追加。エディタがある構成（Debug と Develop）で定義される。`_DEBUG` は今までどおり Debug だけ
- 描画の時間の計測を追加。Render Pipeline ウィンドウの Frame タブに、描画の手順ごとの描画回数・CPU の時間・GPU の時間が出るので、どの描画が重いかを見られる
- 更新の時間の計測を追加。Performance ウィンドウに、1 フレームの内訳（Frame Time）と、更新の内訳（CPU Update）が出る。スクリプトはクラスごと、当たり判定は別の行で出るので、どのスクリプトが重いかを見られる
- 起動の引数 `--profile` を追加。Release の exe に付けて起動すると、同じ計測の結果と FPS を画面に重ねて出す。新しく作るプロジェクトには、これで起動する `RunProfile.bat` が入っている
- シェーダーが使う値を渡さずに描いたときに止まる検査が、Develop でも動くようにした。足りない名前は、エディタの Console と Visual Studio の出力に出る

### 直しが要る変更（ゲーム側）
- エディタ用のコードを囲んでいる `#ifdef _DEBUG` を、`#ifdef TAKO_EDITOR` に替える。替えないと、Develop のエディタでその表示が出ない（Debug と Release は今までどおり動く）
- 今あるプロジェクトで Develop を使うには、`.vcxproj` と `.sln` に構成 `Develop|x64` を足す。エンジンのひな形（`Engine\TakoEngine\templates\Default`）の同じファイルから、`Develop` の付いた所を写す。`.slnx` のプロジェクトは、`<Configurations>` に Debug・Develop・Release の `BuildType` を並べる。足さなくても、Debug と Release は今までどおり使える

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
