#pragma once
#include "Quaternion.h"
#include "Vector3.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace Tako {

  class Actor;
  class Component;

  /// <summary>
  /// 物理の世界（Jolt Physics）を持つシングルトン。ボディ・層の当たり表・ステップ・当たりの通知・レイキャストを扱う
  /// </summary>
  class TAKO_API PhysicsWorld {
  public: //定数
    static constexpr size_t   kMaxLayers        = 32;            ///< 層の上限（Unity / Godot と同じ）。層ごとの当たり表は uint32_t 1 個のビット
    static constexpr uint32_t kDefaultLayer     = 31;            ///< 層を選んでいないボディの層
    static constexpr float    kFixedDeltaTime   = 1.0f / 60.0f;  ///< 1 ステップで進める時間（秒）
    static constexpr uint32_t kInvalidBody      = 0xffffffff;
    static constexpr uint32_t kInvalidCharacter = 0xffffffff;
    static constexpr uint32_t kInvalidJoint     = 0xffffffff;

    // BodySettings::lockedAxes のビット。Dynamic のボディが動けない軸（ワールドの軸）
    static constexpr uint32_t kLockPositionX = 1u << 0;
    static constexpr uint32_t kLockPositionY = 1u << 1;
    static constexpr uint32_t kLockPositionZ = 1u << 2;
    static constexpr uint32_t kLockRotationX = 1u << 3;
    static constexpr uint32_t kLockRotationY = 1u << 4;
    static constexpr uint32_t kLockRotationZ = 1u << 5;

  public: //構造体
    using BodyHandle      = uint32_t;
    using CharacterHandle = uint32_t;
    using JointHandle     = uint32_t;

    enum class BodyKind : uint8_t {
      Static,     ///< 動かない実体。Actor の Transform が変わったら、その位置へ置き直す
      Trigger,    ///< 重なりを知らせるだけで、押し合わない。Actor の Transform に付いていく
      Kinematic,  ///< Actor の Transform に付いていく実体。Dynamic を押すが、自分は押されない
      Dynamic,    ///< 力と当たりで動く実体。位置と向きは物理が決めて、Actor の Transform へ書く
    };

    enum class ShapeType : uint8_t {
      Sphere,
      Box,
      Capsule,
      Mesh,        ///< 三角形のメッシュそのまま。動かない物と Kinematic に使う（Dynamic の形にはできない）
      ConvexHull,  ///< 頂点を包む、へこみの無い形。Dynamic にも使える
    };

    /// <summary>
    /// Mesh と ConvexHull の元になる頂点と三角形（scale を掛ける前の値）
    /// </summary>
    struct MeshData {
      std::vector<Vector3>  positions;
      std::vector<uint32_t> indices;    ///< 3 つで三角形 1 枚（ConvexHull は使わない）
    };

    /// <summary>
    /// ボディに付ける形 1 つ。大きさと位置には、ボディの Actor のワールドの scale が掛かる（Unity と同じ決まり。Play 中に scale を変えても付いていく）
    /// </summary>
    struct Shape {
      ShapeType  type     = ShapeType::Sphere;
      float      radius   = 0.5f;                        ///< Sphere（scale は 3 軸の最大）と Capsule（scale は X と Z の大きい方）
      float      height   = 2.0f;                        ///< Capsule の全長（両端の半球を含む）。軸は Y で、scale も Y
      Vector3    size     = { 1.0f, 1.0f, 1.0f };        ///< Box の全長（scale は軸ごと）
      Vector3    position = {};                          ///< ボディから見た位置（scale は軸ごと）
      Quaternion rotation = { 0.0f, 0.0f, 0.0f, 1.0f };  ///< ボディから見た向き
      Vector3    scale    = { 1.0f, 1.0f, 1.0f };        ///< ボディの Actor から見た、形の Actor の scale（子の Actor に付いた形のとき）
      Component* owner    = nullptr;                     ///< 当たりの通知先。owner の Actor の全コンポーネントに OnCollision〜 が届く

      std::shared_ptr<const MeshData> mesh;  ///< Mesh と ConvexHull の頂点（scale は軸ごと）
    };

    struct BodySettings {
      BodyKind kind        = BodyKind::Static;
      uint32_t layer       = kDefaultLayer;
      Actor*   actor       = nullptr;  ///< 位置と向きの元（Dynamic は書く先）
      float    friction    = 0.5f;     ///< 滑りにくさ（0 = 滑る）。当たった相手の値との相乗平均が使われる
      float    restitution = 0.0f;     ///< 跳ね返り（0 = 跳ねない、1 = 同じ速さで跳ね返る）。当たった相手の値の大きい方が使われる

      //Dynamic だけ
      float    mass           = 1.0f;
      float    linearDamping  = 0.05f;  ///< 動きの減り方（1 秒あたりに失う速さの割合）
      float    angularDamping = 0.05f;  ///< 回転の減り方
      float    gravityScale   = 1.0f;   ///< 重力の倍率（0 = 重力を受けない）
      uint32_t lockedAxes     = 0;      ///< kLock〜 のビット
      bool     isContinuous   = false;  ///< 速く動いても、実体を通り抜けない（計算は重くなる）
    };

    struct RaycastHit {
      Actor*  actor    = nullptr;
      Vector3 point    = {};
      Vector3 normal   = {};  ///< 当たった面の向き（ワールド）
      float   distance = 0.0f;
    };

    /// <summary>
    /// 壁と床に沿って動くキャラクター。形は、ワールドの Y 軸に立ったカプセル（Actor が回っても傾かない）
    /// </summary>
    struct CharacterSettings {
      Actor*     actor         = nullptr;        ///< 位置の元で、動いた結果を書く先（向きは読み書きしない）
      Component* owner         = nullptr;        ///< 当たりの通知先
      uint32_t   layer         = kDefaultLayer;
      float      radius        = 0.5f;           ///< scale は X と Z の大きい方
      float      height        = 2.0f;           ///< 全長（両端の半球を含む）。scale は Y
      Vector3    offset        = {};             ///< Actor から見た、カプセルの中心（scale は軸ごと）
      float      maxSlopeAngle = 0.8726646f;     ///< 登れる坂の角度（ラジアン）。これより急な面は、壁として滑る
      float      stepHeight    = 0.4f;           ///< 乗り越えられる段差の高さ。下りでは、この高さまで床に吸い付く
      float      gravityScale  = 1.0f;           ///< 重力の倍率（0 = 重力を受けない）
      float      pushStrength  = 100.0f;         ///< 歩いて当たった Dynamic のボディを押す力の上限（N）。0 なら押さずに、壁のように止まる
      float      mass          = 70.0f;          ///< 立っている Dynamic のボディに掛かる重さ（kg）。0 なら掛けない
    };

    enum class JointType : uint8_t {
      Fixed,     ///< 相手との位置と向きを、今のまま固める
      Point,     ///< 1 点でつなぐ（向きは自由）
      Hinge,     ///< 1 点でつなぎ、軸の周りだけ回る
      Distance,  ///< 2 点の距離を保つ
    };

    /// <summary>
    /// 2 つのボディ（か、ボディとワールド）をつなぐ決まり。点と軸は、つなぐときのワールドの値
    /// </summary>
    struct JointSettings {
      JointType  type            = JointType::Fixed;
      BodyHandle body            = kInvalidBody;
      BodyHandle connectedBody   = kInvalidBody;          ///< kInvalidBody なら、ワールドにつなぐ
      Vector3    anchor          = {};                    ///< つなぐ点（Fixed は使わない）
      Vector3    connectedAnchor = {};                    ///< Distance の、相手の側の点
      Vector3    axis            = { 0.0f, 1.0f, 0.0f };  ///< Hinge の軸
      float      minAngle        = -3.1415927f;           ///< Hinge の回る範囲の下限（ラジアン、-π〜0。つないだときの向きが 0）
      float      maxAngle        = 3.1415927f;            ///< Hinge の回る範囲の上限（ラジアン、0〜π）
      float      minDistance     = -1.0f;                 ///< Distance の範囲。負なら、つないだときの距離
      float      maxDistance     = -1.0f;
    };

    /// <summary>
    /// BeginFrame から後の合計（計測の表示用）
    /// </summary>
    struct FrameStats {
      uint32_t stepCount     = 0;
      float    stepMs        = 0.0f;  ///< Jolt の計算
      uint32_t callbackCount = 0;     ///< 配った OnCollision〜 の組の数
      float    callbackMs    = 0.0f;
    };

    /// <summary>
    /// 世界の今の状態（デバッグ表示用）
    /// </summary>
    struct WorldStats {
      size_t                           bodyCount         = 0;  ///< 作ってあるボディ（キャラクターも 1 つと数える）
      size_t                           enabledCount      = 0;  ///< そのうち、世界に入っているボディ
      uint32_t                         awakeCount        = 0;  ///< そのうち、寝ていないボディ（Trigger は寝ない）
      size_t                           touchingPairCount = 0;  ///< 当たっている通知先の組
      std::array<uint32_t, kMaxLayers> layerBodyCounts{};      ///< 添字 = 層
    };

  private: //構造体
    struct Impl;      ///< Jolt の型を持つ中身。このヘッダに Jolt を出さないよう、PhysicsWorld.cpp に置く
    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー

  public: //メンバー関数
    explicit PhysicsWorld(Token);
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    static PhysicsWorld* GetInstance();

    /// <summary>
    /// Jolt を初期化し、空の世界と計算用のスレッドを作る
    /// </summary>
    void Initialize();

    /// <summary>
    /// 世界を壊してスレッドを止める。DLL が外れる前に、エンジンの終了処理から呼ぶ
    /// </summary>
    void Finalize();

    //============================================================
    //フレーム（BaseScene が呼ぶ）
    //============================================================
    /// <summary>
    /// フレームの頭で呼ぶ。前のフレームに止めたボディの Exit を配り、Actor の Transform をボディへ写す
    /// </summary>
    void BeginFrame();

    /// <summary>
    /// 世界を kFixedDeltaTime 秒だけ進め、当たり始めた組に Enter、離れた組に Exit を配る
    /// </summary>
    void Step();

    /// <summary>
    /// ステップの後に呼ぶ。Dynamic の姿勢を Actor へ書き、止めたボディの Exit と、当たり続けている組の Stay を配る（ステップが 0 回のフレームでも行う）
    /// </summary>
    /// <param name="alpha">次のステップまでの進み具合（0〜1）。直前の 2 ステップの姿勢を、この割合で補間して書く</param>
    void EndFrame(float alpha);

    //============================================================
    //ボディ
    //============================================================
    /// <summary>
    /// ボディを作って世界に入れる。位置・向き・形の拡大は、settings.actor のワールドの Transform
    /// </summary>
    /// <returns>作れなければ kInvalidBody</returns>
    BodyHandle CreateBody(const BodySettings& settings, std::span<const Shape> shapes);

    /// <summary>
    /// ボディを消す。当たっていた相手に Exit は届かない
    /// </summary>
    void DestroyBody(BodyHandle body);

    /// <summary>
    /// ボディを世界から出す・戻す。出すと、当たっていた相手との間に Exit が届く（戻して重なっていれば、Enter が出直す）
    /// </summary>
    void SetBodyEnabled(BodyHandle body, bool enabled);

    /// <summary>
    /// 形を作り直す。当たっている組はそのまま続く
    /// </summary>
    void SetBodyShapes(BodyHandle body, std::span<const Shape> shapes);

    /// <summary>
    /// 層・摩擦・跳ね返りと、Dynamic の質量などを入れ直す（kind と actor は変えられない）
    /// </summary>
    void SetBodySettings(BodyHandle body, const BodySettings& settings);

    /// <summary>
    /// 次の 1 ステップの間、力を掛ける（質量が大きいほど効きにくい）。掛け続けるには、FixedTick で毎回呼ぶ。Dynamic だけに効く（下の 4 つも同じ）
    /// </summary>
    void AddForce(BodyHandle body, const Vector3& force);

    /// <summary>
    /// 次の 1 ステップの間、回す力を掛ける（向きが軸、長さが強さ）
    /// </summary>
    void AddTorque(BodyHandle body, const Vector3& torque);

    /// <summary>
    /// 速度をその場で変える（質量が大きいほど効きにくい）。ジャンプや、弾が当たった衝撃に使う
    /// </summary>
    void AddImpulse(BodyHandle body, const Vector3& impulse);

    void SetLinearVelocity(BodyHandle body, const Vector3& velocity);

    /// <summary>
    /// 向きが回転の軸、長さが速さ（ラジアン / 秒）
    /// </summary>
    void SetAngularVelocity(BodyHandle body, const Vector3& velocity);

    //============================================================
    //キャラクター
    //============================================================
    /// <summary>
    /// キャラクターを作って世界に入れる。位置と形の拡大は、settings.actor のワールドの Transform
    /// </summary>
    /// <returns>作れなければ kInvalidCharacter</returns>
    CharacterHandle CreateCharacter(const CharacterSettings& settings);

    /// <summary>
    /// キャラクターを消す。当たっていた相手に Exit は届かない
    /// </summary>
    void DestroyCharacter(CharacterHandle character);

    /// <summary>
    /// キャラクターを世界から出す・戻す。出すと、当たっていた相手との間に Exit が届く（戻して重なっていれば、Enter が出直す）
    /// </summary>
    void SetCharacterEnabled(CharacterHandle character, bool enabled);

    /// <summary>
    /// 動く速度。毎ステップ、ここに重力が足されてから動く（立っている間は、下向きの分を溜めない）
    /// </summary>
    void SetCharacterVelocity(CharacterHandle character, const Vector3& velocity);

    /// <summary>
    /// 歩いて当たった Dynamic のボディを押す力の上限（N）を変える。キャラクターは作り直さない（速度は残る）
    /// </summary>
    void SetCharacterPushStrength(CharacterHandle character, float pushStrength);

    /// <summary>
    /// 立っている Dynamic のボディに掛かる重さ（kg）を変える。キャラクターは作り直さない
    /// </summary>
    void SetCharacterMass(CharacterHandle character, float mass);

    //============================================================
    //ジョイント
    //============================================================
    /// <summary>
    /// 2 つのボディを、今の位置でつなぐ。どちらかのボディを消すと、ジョイントも消える。どちらかを世界から出している間は、効かない
    /// </summary>
    /// <returns>settings.body が無ければ kInvalidJoint</returns>
    JointHandle CreateJoint(const JointSettings& settings);

    void DestroyJoint(JointHandle joint);

    //============================================================
    //問い合わせ
    //============================================================
    /// <summary>
    /// origin から direction へ maxDistance までの間で、最初に当たるボディを返す。始点を中に含む形と、メッシュの三角形の裏面には当たらない（Unity と同じ）。ボディの位置は、最後の BeginFrame か Step の時点
    /// </summary>
    /// <param name="layerMask">当たる層のビット（ビット n = 層 n）</param>
    /// <param name="hitsTriggers">false なら、Trigger を素通りする（Unity と同じく、既定では当たる）</param>
    std::optional<RaycastHit> Raycast(const Vector3& origin, const Vector3& direction, float maxDistance, uint32_t layerMask = 0xffffffff, bool hitsTriggers = true) const;

    //============================================================
    //層
    //============================================================
    /// <summary>
    /// 全ての層の組み合わせを、当たらない状態にする（最初は、全部の層どうしが当たる。当てる組だけをコードで決めるときに、先に呼ぶ）
    /// </summary>
    void ClearCollisionMasks() { collisionMask_.fill(0); }

    /// <summary>
    /// 層を削除し、後ろの層の名前と当たり表を 1 つ前へ詰める（ボディに設定済みの層の番号は変わらない）
    /// </summary>
    void RemoveLayer(uint32_t index);

    /// <summary>
    /// 2 つの層の名前と当たり表を入れ替える（ボディに設定済みの層の番号は変わらない）
    /// </summary>
    void SwapLayers(uint32_t layerA, uint32_t layerB);

    //============================================================
    //デバッグ描画
    //============================================================
    /// <summary>
    /// 世界の全ボディの形を線で描く（SetDebugDrawEnabled がオンのときだけ。エディタのある構成だけ）
    /// </summary>
    /// <param name="cameraPosition">遠い形を粗く描くための、視点の位置</param>
    void DrawDebug(const Vector3& cameraPosition);

    /// <summary>
    /// shapes を、actor の位置に置いたボディの形として線で描く（世界に入れていない形も描ける。エディタのある構成だけ）
    /// </summary>
    /// <param name="cameraPosition">遠い形を粗く描くための、視点の位置</param>
    /// <param name="isUpright">actor の向きを使わない（キャラクターのカプセル）</param>
    void DrawShapes(const Actor& actor, std::span<const Shape> shapes, const Vector3& cameraPosition, bool isUpright = false);

    //============================================================
    //Setter
    //============================================================
    /// <summary>
    /// 2 つの層が当たるかを決める（kMaxLayers 以上の層は無視する）。最初は、全部の層どうしが当たる。実体どうしは押し合い、Trigger は重なりを知らせる
    /// </summary>
    void SetCollisionMask(uint32_t layerA, uint32_t layerB, bool canCollide);

    /// <summary>
    /// 層の enum をキャストせずに渡せる版
    /// </summary>
    template<class E> requires std::is_enum_v<E>
    void SetCollisionMask(E layerA, E layerB, bool canCollide) {
      SetCollisionMask(static_cast<uint32_t>(layerA), static_cast<uint32_t>(layerB), canCollide);
    }

    /// <summary>
    /// 層ごとの表示名（添字 = 層の番号、kMaxLayers を超えた分は切り捨てる）
    /// </summary>
    void SetLayerNames(std::vector<std::string> names);

    void SetGravity(const Vector3& gravity);

    void SetDebugDrawEnabled(bool enabled) { debugDrawEnabled_ = enabled; }

    //============================================================
    //Getter
    //============================================================
    /// <summary>
    /// 層 A と層 B が当たるか（kMaxLayers 以上の層は常に false）
    /// </summary>
    bool CanCollide(uint32_t layerA, uint32_t layerB) const;

    std::vector<std::string>& GetLayerNames() { return layerNames_; }

    /// <summary>
    /// 添字 = 層 A、ビット b = 層 B と当たる
    /// </summary>
    const std::array<uint32_t, kMaxLayers>& GetCollisionMasks() const { return collisionMask_; }

    /// <summary>
    /// 表示用の層の名前（名前があればその名前、無ければ "Layer N"）
    /// </summary>
    std::string GetLayerLabel(uint32_t layer) const;

    Vector3 GetGravity() const;

    /// <summary>
    /// Dynamic 以外と、世界から出してあるボディは 0
    /// </summary>
    Vector3 GetLinearVelocity(BodyHandle body) const;

    Vector3 GetAngularVelocity(BodyHandle body) const;

    Vector3 GetCharacterVelocity(CharacterHandle character) const;

    /// <summary>
    /// 最後のステップで、立てる面（maxSlopeAngle より緩い面）の上に居たか
    /// </summary>
    bool IsCharacterGrounded(CharacterHandle character) const;

    bool IsDebugDrawEnabled() const { return debugDrawEnabled_; }

    const FrameStats& GetFrameStats() const { return frameStats_; }

    /// <summary>
    /// 最後の DrawDebug が描いた線の数（LineRenderer::kLineMaxCount を越えた分は、描かれない）
    /// </summary>
    uint32_t GetDebugLineCount() const { return debugLineCount_; }

    /// <summary>
    /// 作ってあるボディとキャラクターの数（世界から出してある物も含む）
    /// </summary>
    size_t GetBodyCount() const;

    /// <summary>
    /// 全ボディを数えるので、表示するときだけ呼ぶ
    /// </summary>
    WorldStats GetWorldStats() const;

  private: //非公開関数
    ~PhysicsWorld();

    friend struct std::default_delete<PhysicsWorld>;

    // 当たり表の層の番号を remap で付け替える。nullopt を返した層は、組み合わせごと外す
    void RemapCollisionMasks(const std::function<std::optional<uint32_t>(uint32_t)>& remap);

  private: //メンバー変数
    std::unique_ptr<Impl> impl_;

    //層の当たり表。ステップの間は、計算用のスレッドが読む
    std::array<uint32_t, kMaxLayers> collisionMask_{};  ///< 添字 = 層 A、ビット b = 層 B と当たる（対称に保つ）
    std::vector<std::string>         layerNames_;       ///< 添字 = 層の番号。意味付けはゲーム側の層の定義に合わせる

    FrameStats frameStats_;
    bool       debugDrawEnabled_ = false;
    uint32_t   debugLineCount_   = 0;

    static std::unique_ptr<PhysicsWorld> instance_;  ///< シングルトン
  };

} // namespace Tako
