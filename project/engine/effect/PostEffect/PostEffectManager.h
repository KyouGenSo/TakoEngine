#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "PostEffectStruct.h"
#include "PostProcess.h"
#ifdef _DEBUG
#include <span>
#include "EditTracker.h"
#include "ShortcutTable.h"
#endif

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// 本編のポストエフェクトスタックの窓口。組み込みエフェクトの登録・一時エフェクトのフェード・プロファイルの保存と読み込み・編集 UI を受け持つ
  /// </summary>
  class PostEffectManager {
  private: // シングルトン設定
    static std::unique_ptr<PostEffectManager> instance_;

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~PostEffectManager() = default;

    friend struct std::default_delete<PostEffectManager>;

  public:
    explicit PostEffectManager(Token) {}
    PostEffectManager(const PostEffectManager&) = delete;
    PostEffectManager& operator=(const PostEffectManager&) = delete;

  public: //構造体
    enum class AntiAliasing { None, FXAA };

  private: //構造体
    /// <summary>
    /// 一時エフェクト情報
    /// </summary>
    struct TemporaryEffectInfo {
      float       duration;     ///< 持続時間（秒）
      float       elapsedTime;  ///< 経過時間（秒）
      EasingType  easing;       ///< イージング種別
      EffectParam baseParam;    ///< 基本パラメータ（開始時の値）
    };

  public: //メンバー関数
    static PostEffectManager* GetInstance();

    /// <summary>
    /// 組み込みのエフェクトを登録する。スタックにエフェクトを足すより前に呼ぶ
    /// </summary>
    void Initialize(DX12Basic* dx12);

    static void Finalize();

    /// <summary>
    /// 定義の既定値でエフェクトを本編スタックの末尾に追加する
    /// </summary>
    /// <param name="name">エフェクトの名前</param>
    /// <param name="maskId">0 なら画面全体。それ以外は Object3d::SetMaskId が一致する描画物だけに掛ける</param>
    /// <returns>インスタンスの id（未登録の名前なら 0）</returns>
    uint32_t AddEffect(std::string_view name, uint32_t maskId = 0);

    /// <summary>
    /// param でエフェクトを本編スタックの末尾に追加する
    /// </summary>
    /// <returns>インスタンスの id（未登録の名前なら 0）</returns>
    uint32_t AddEffect(std::string_view name, const EffectParam& param, uint32_t maskId = 0);

    void RemoveEffect(uint32_t id);

    /// <summary>
    /// 一時エフェクトも含めて全て外す
    /// </summary>
    void ClearEffects();

    /// <summary>
    /// 一時的なエフェクトを掛ける。duration 秒でフェードアウトして自動で外れる。同名の一時エフェクトが動作中ならそれを最初からやり直す
    /// </summary>
    /// <param name="name">エフェクトの名前</param>
    /// <param name="duration">持続時間（秒）</param>
    /// <param name="param">開始時のパラメータ</param>
    /// <param name="easing">イージング種別</param>
    /// <param name="maskId">掛ける描画物の maskId（0 なら画面全体）</param>
    /// <returns>インスタンスの id（未登録の名前なら 0）</returns>
    uint32_t ApplyTemporaryEffect(std::string_view name, float duration, const EffectParam& param, EasingType easing = EasingType::EaseOut, uint32_t maskId = 0);

    /// <summary>
    /// 一時エフェクトの更新処理（毎フレーム呼び出し）
    /// </summary>
    /// <param name="deltaTime">前フレームからの経過時間（秒）</param>
    void Update(float deltaTime);

    void CancelTemporaryEffect(uint32_t id);

    /// <summary>
    /// 本編スタックを input→output へ掛ける（アンチエイリアス設定を含む）
    /// </summary>
    void Execute(const PostEffectFrame& frame, RenderTexture& input, RenderTexture& output) const { stack_.Execute(frame, input, output, antiAliasing_ == AntiAliasing::FXAA); }

    /// <summary>
    /// ImGui デバッグ UI を描画
    /// </summary>
    void DrawImgui();

    /// <summary>
    /// 現在のスタックと各エフェクトのパラメータを resources/Json/PostEffects/{name}.json に保存する（一時エフェクトは除く）
    /// </summary>
    /// <returns>書き込めたら true</returns>
    bool SaveProfile(const std::string& name) const;

    /// <summary>
    /// プロファイルのスタックで置き換える（一時エフェクトは残す）
    /// </summary>
    /// <returns>ファイルが無い、または途中で失敗したら false</returns>
    bool LoadProfile(const std::string& name);

    /// <summary>
    /// SaveProfile が書き込む内容（JSON 文字列）
    /// </summary>
    std::string SerializeProfile() const;

    /// <summary>
    /// SerializeProfile の内容でスタックを置き換える。一時エフェクトはスタックに残す
    /// </summary>
    /// <returns>途中で失敗したら false（そこまでの適用は残る）</returns>
    bool ApplyProfile(const std::string& profile);

    /// <summary>
    /// PostEffect ウィンドウで編集して未保存なら、そのプロファイルを out に追加する（終了時の確認用）
    /// </summary>
    void CollectUnsavedChanges(std::vector<std::string>& out) const;

#ifdef _DEBUG
    /// <summary>
    /// キー操作の表。DrawImgui のキー処理と About の一覧が使う
    /// </summary>
    static std::span<const ShortcutBinding<PostEffectManager>> Shortcuts();
#endif

    //============================================================
    //Setter
    //============================================================
    /// <summary>
    /// インスタンスのパラメータを設定する
    /// </summary>
    /// <returns>id が無い、またはパラメータの型がエフェクトと違えば false</returns>
    bool SetEffectParam(uint32_t id, const EffectParam& param);

    /// <summary>
    /// Dissolve が読むマスクテクスチャを設定
    /// </summary>
    void SetDissolveMaskTex(const std::string& textureName);

    /// <summary>
    /// Dissolve が溶けた部分に出すテクスチャを設定
    /// </summary>
    void SetDissolveBaseTex(const std::string& textureName);

    void SetAntiAliasing(AntiAliasing antiAliasing) { antiAliasing_ = antiAliasing; }

    //============================================================
    //Getter
    //============================================================
    AntiAliasing GetAntiAliasing() const { return antiAliasing_; }
    bool UsesObjectMask() const { return stack_.UsesObjectMask(); }
    bool IsTemporaryEffectActive(uint32_t id) const { return temporaryEffects_.contains(id); }

  private: //非公開関数
    void RegisterBuiltinEffects();

#ifdef _DEBUG
    /// <summary>
    /// profileName_ の名前で保存し、結果をログに出す（Save ボタン・Ctrl+S・確認モーダルの Save）
    /// </summary>
    bool SaveCurrentProfile();

    void DrawStackTab();

    void DrawEffectParametersTab();
#endif

    /// <summary>
    /// イージング関数を適用
    /// </summary>
    /// <param name="t">進行度（0.0〜1.0）</param>
    /// <param name="type">イージング種別</param>
    /// <returns>イージング適用後の値</returns>
    float ApplyEasing(float t, EasingType type) const;

    /// <summary>
    /// フェード係数をパラメータに適用
    /// </summary>
    /// <param name="param">基本パラメータ</param>
    /// <param name="fadeFactor">フェード係数（0.0〜1.0）</param>
    /// <returns>フェード適用後のパラメータ</returns>
    EffectParam ApplyFadeToParam(const EffectParam& param, float fadeFactor) const;

  private: //メンバー変数
    DX12Basic*                                        dx12_                 = nullptr;
    PostProcessStack                                  stack_;                                      ///< 本編のエフェクト（並び順に掛ける）
    std::unordered_map<uint32_t, TemporaryEffectInfo> temporaryEffects_;                           ///< インスタンス id ごとの一時エフェクト
    uint32_t                                          dissolveMaskSrvIndex_ = 0;                   ///< Dissolve のマスクテクスチャの SRV（0 ならエンジン既定の noise0）
    uint32_t                                          dissolveBaseSrvIndex_ = 0;                   ///< Dissolve で溶けた部分のテクスチャの SRV（0 ならエンジン既定の black）
    AntiAliasing                                      antiAliasing_         = AntiAliasing::None;

    //PostEffect ウィンドウ
    std::string          selectedAvailableEffect_;      ///< 追加する定義の名前
    uint32_t             selectedActiveEffect_    = 0;  ///< 選択中のインスタンス id
    std::array<char, 64> profileName_{};                ///< プロファイル名入力欄。LoadProfile で読み込んだ名前になる
#ifdef _DEBUG
    EditTracker<>        profileTracker_{ { [this] { return SerializeProfile(); }, [this](const std::string& profile) { ApplyProfile(profile); } } };
    UnsavedChangesPrompt unsavedPrompt_;  ///< 未保存のまま別のプロファイルを読み込むときの確認
#endif
  };

} // namespace Tako
