#pragma once
#include <d3d12.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <wrl.h>

#include "IPostEffect.h"
#include "Vector2.h"
#include"Vector4.h"
#include "PostEffectStruct.h"
#include <variant>
#include <string>

#include "Dissolve.h"
#include "TextureManager.h"


class IPostEffect;
class DX12Basic;

namespace Tako {

  class Camera;

  /// <summary>
  /// ポストエフェクト管理クラス
  /// エフェクトチェーンシステムで複数エフェクトの連続適用を制御
  /// </summary>
  class PostEffectManager {
  private:
    // シングルトン設定
    static std::unique_ptr<PostEffectManager> instance_;

    PostEffectManager() = default;
    ~PostEffectManager() = default;

    friend struct std::default_delete<PostEffectManager>;

  public:
    PostEffectManager(const PostEffectManager&) = delete;
    PostEffectManager& operator=(const PostEffectManager&) = delete;

  public: // メンバ関数

    // ComPtr のエイリアス
    template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    /// <summary>
    /// シングルトンインスタンスを取得
    /// </summary>
    /// <returns>PostEffectManager のインスタンスポインタ</returns>
    static PostEffectManager* GetInstance();

    /// <summary>
    /// ポストエフェクトマネージャーを初期化
    /// レンダーテクスチャの作成と全エフェクトの登録を行う
    /// </summary>
    /// <param name="dx12">DirectX 12基盤クラスのポインタ</param>
    void Initialize(DX12Basic* dx12);

    /// <summary>
    /// ポストエフェクトマネージャーを終了
    /// シングルトンインスタンスを解放する
    /// </summary>
    static void Finalize();

    /// <summary>
    /// エフェクトチェーンに指定されたエフェクトを追加
    /// 追加されたエフェクトは描画時に適用順序に従って実行される
    /// </summary>
    /// <param name="name">追加するエフェクトの名前</param>
    void AddEffectToChain(const std::string& name);

    /// <summary>
    /// エフェクトチェーンから指定されたエフェクトを削除
    /// </summary>
    /// <param name="name">削除するエフェクトの名前</param>
    void RemoveEffectFromChain(const std::string& name);

    /// <summary>
    /// エフェクトチェーンをクリアして全エフェクトを削除
    /// </summary>
    void ClearEffectChain();

    /// <summary>
    /// エフェクト適用対象のレンダーターゲットへの描画を開始
    /// この関数の後に描画されたオブジェクトにはポストエフェクトが適用される
    /// </summary>
    void BeginDrawEffectTarget();

    /// <summary>
    /// エフェクト非適用対象のレンダーターゲットへの描画を開始
    /// この関数の後に描画されたオブジェクトにはポストエフェクトが適用されない
    /// </summary>
    void BegineDrawNonEffectTarget();

    /// <summary>
    /// エフェクトチェーンを適用して描画処理を実行
    /// 内部で ApplyEffectChain を呼び出す
    /// </summary>
    void Draw();

    /// <summary>
    /// 最終的なエフェクト適用結果を描画
    /// </summary>
    /// <param name="drawToSwapChain">スワップチェーンに描画するかどうか（デフォルト: true）</param>
    void DrawFinalResult(bool drawToSwapChain = true);

    /// <summary>
    /// ImGui デバッグ UI を描画
    /// エフェクトチェーン管理とパラメータ調整用の UI を表示する
    /// </summary>
    void DrawImgui();

    /// <summary>
    /// レンダーテクスチャを現在のウィンドウサイズで再生成
    /// ウィンドウリサイズ時などに呼び出される
    /// </summary>
    void RecreateRenderTexture();

    /// <summary>
    /// 指定されたエフェクトのパラメータを設定
    /// </summary>
    /// <param name="effectName">エフェクト名</param>
    /// <param name="param">設定するパラメータ（EffectParam 型）</param>
    /// <returns>設定が成功した場合 true、エフェクトが見つからない場合 false</returns>
    bool SetEffectParam(const std::string& effectName, const EffectParam& param);

    /// <summary>
    /// 指定されたエフェクトのパラメータを設定（テンプレート版）
    /// 任意の型のパラメータを受け取り、内部で EffectParam に変換する
    /// </summary>
    /// <typeparam name="ParamType">パラメータの型</typeparam>
    /// <param name="effectName">エフェクト名</param>
    /// <param name="param">設定するパラメータ</param>
    /// <returns>設定が成功した場合 true、エフェクトが見つからない場合 false</returns>
    template<typename ParamType>
    bool SetEffectParam(const std::string& effectName, const ParamType& param) {
      return SetEffectParam(effectName, EffectParam(param));
    }

    /// <summary>
    /// 一時的なポストエフェクトを適用
    /// 指定した持続時間でフェードアウトし、終了後に自動削除される
    /// </summary>
    /// <typeparam name="ParamType">エフェクトパラメータの型</typeparam>
    /// <param name="effectName">エフェクト名</param>
    /// <param name="duration">持続時間（秒）</param>
    /// <param name="param">開始時のパラメータ</param>
    /// <param name="easing">イージング種別（デフォルト: EaseOut）</param>
    template<typename ParamType>
    void ApplyTemporaryEffect(
      const std::string& effectName,
      float duration,
      const ParamType& param,
      EasingType easing = EasingType::EaseOut)
    {
      if (!IsEffectInChain(effectName)) {
        AddEffectToChain(effectName);
      }
      SetEffectParam(effectName, param);

      TemporaryEffectInfo info;
      info.duration = duration;
      info.elapsedTime = 0.0f;
      info.easing = easing;
      info.baseParam = EffectParam(param);
      temporaryEffects_[effectName] = info;
    }

    /// <summary>
    /// 一時エフェクトの更新処理（毎フレーム呼び出し）
    /// </summary>
    /// <param name="deltaTime">前フレームからの経過時間（秒）</param>
    void Update(float deltaTime);

    /// <summary>
    /// 一時エフェクトをキャンセル
    /// </summary>
    /// <param name="effectName">キャンセルするエフェクト名</param>
    void CancelTemporaryEffect(const std::string& effectName);

    /// <summary>
    /// 指定エフェクトが一時エフェクトとして動作中か判定
    /// </summary>
    /// <param name="effectName">エフェクト名</param>
    /// <returns>一時エフェクトとして動作中なら true</returns>
    bool IsTemporaryEffectActive(const std::string& effectName) const;

    /// <summary>
    /// ポストエフェクトで使用するカメラを設定
    /// 深度ベースエフェクト等でカメラ情報が必要な場合に使用
    /// </summary>
    /// <param name="camera">設定するカメラのポインタ</param>
    void SetCamera(Camera* camera) {
      if (camera) {
        camera_ = camera;
      }
    }

    /// <summary>
    /// ディゾルブエフェクト用のマスクテクスチャを設定
    /// </summary>
    /// <param name="textureName">マスクテクスチャの名前</param>
    void SetDissolveMaskTex(const std::string& textureName) {
      if (!textureName.empty()) {
        dissolveMaskSrvIndex_ = TextureManager::GetInstance()->GetSRVIndex(textureName);
      }
    }

    /// <summary>
    /// ディゾルブエフェクト用のベーステクスチャを設定
    /// </summary>
    /// <param name="textureName">ベーステクスチャの名前</param>
    void SetDissolveBaseTex(const std::string& textureName);

    /// <summary>
    /// 指定されたエフェクトをチェーン内で1つ上に移動
    /// </summary>
    /// <param name="effectName">移動するエフェクトの名前</param>
    /// <returns>移動が成功した場合 true、失敗した場合 false</returns>
    bool MoveEffectUp(const std::string& effectName);

    /// <summary>
    /// 指定されたエフェクトをチェーン内で1つ下に移動
    /// </summary>
    /// <param name="effectName">移動するエフェクトの名前</param>
    /// <returns>移動が成功した場合 true、失敗した場合 false</returns>
    bool MoveEffectDown(const std::string& effectName);

    /// <summary>
    /// 指定されたエフェクトをチェーン内の特定位置に移動
    /// </summary>
    /// <param name="effectName">移動するエフェクトの名前</param>
    /// <param name="newPosition">移動先の位置（0から始まるインデックス）</param>
    /// <returns>移動が成功した場合 true、失敗した場合 false</returns>
    bool MoveEffectToPosition(const std::string& effectName, int newPosition);

    /// <summary>
    /// チェーン内の2つのエフェクトの位置を交換
    /// </summary>
    /// <param name="effectName1">1つ目のエフェクトの名前</param>
    /// <param name="effectName2">2つ目のエフェクトの名前</param>
    /// <returns>交換が成功した場合 true、失敗した場合 false</returns>
    bool SwapEffects(const std::string& effectName1, const std::string& effectName2);

    /// <summary>
    /// チェーン内の2つのエフェクトをインデックスで指定して位置を交換
    /// </summary>
    /// <param name="index1">1つ目のエフェクトのインデックス</param>
    /// <param name="index2">2つ目のエフェクトのインデックス</param>
    /// <returns>交換が成功した場合 true、失敗した場合 false</returns>
    bool SwapEffectsByIndex(int index1, int index2);

    /// <summary>
    /// 指定されたエフェクトのチェーン内での位置を取得
    /// </summary>
    /// <param name="effectName">エフェクトの名前</param>
    /// <returns>エフェクトの位置（0から始まるインデックス）、見つからない場合は-1</returns>
    int GetEffectPosition(const std::string& effectName) const;

    /// <summary>
    /// 指定されたエフェクトがチェーン内に存在するか判定
    /// </summary>
    /// <param name="effectName">エフェクトの名前</param>
    /// <returns>チェーン内に存在する場合 true、存在しない場合 false</returns>
    bool IsEffectInChain(const std::string& effectName) const;

    /// <summary>
    /// エフェクトチェーンのサイズ（登録されているエフェクト数）を取得
    /// </summary>
    /// <returns>エフェクトチェーンのサイズ</returns>
    size_t GetEffectChainSize() const;

    /// <summary>
    /// 指定位置にあるエフェクトの名前を取得
    /// </summary>
    /// <param name="position">取得する位置（0から始まるインデックス）</param>
    /// <returns>指定位置のエフェクト名、範囲外の場合は空文字列</returns>
    std::string GetEffectAtPosition(int position) const;

    /// <summary>
    /// エフェクトチェーン全体を取得
    /// </summary>
    /// <returns>エフェクト名のリスト</returns>
    std::vector<std::string> GetEffectChain() const;

    /// <summary>
    /// 現在のレンダーターゲットの CPU ディスクリプタハンドルを取得
    /// </summary>
    /// <returns>エフェクト対象レンダーターゲットの RTV ハンドル</returns>
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTVHandle() const { return effectTargetRT_.rtvHandle; }

    /// <summary>
    /// 最終的なエフェクト適用結果の SRV インデックスを取得
    /// ImGui でのテクスチャ表示などに使用
    /// </summary>
    /// <returns>最終結果テクスチャの SRV インデックス</returns>
    uint32_t GetFinalResultSrvIndex() const;

    /// <summary>
    /// 最終的なエフェクト適用結果のリソースを取得
    /// ImGui でのテクスチャ表示などに使用
    /// </summary>
    /// <returns>最終結果テクスチャのリソースポインタ</returns>
    ID3D12Resource* GetFinalResultResource() const;

  private: // プライベートメンバー関数
    // レンダーターゲットの作成
    void CreateRenderTextures();

    // エフェクトの登録
    void RegisterEffect(const std::string& name, std::unique_ptr<IPostEffect> effect);

    // エフェクトを適用
    void ApplyEffectChain();

    /// <summary>
    /// ImGui エフェクトパラメータタブを描画
    /// </summary>
    void DrawEffectParametersTab();

    /// <summary>
    /// リソースバリアを設定
    /// </summary>
    /// <param name="stateBefore">遷移前の状態</param>
    /// <param name="stateAfter">遷移後の状態</param>
    void SetBarrier(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

    /// <summary>
    /// 指定リソースのバリアを設定
    /// </summary>
    /// <param name="resource">対象リソース</param>
    /// <param name="stateBefore">遷移前の状態</param>
    /// <param name="stateAfter">遷移後の状態</param>
    void SetBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

    /// <summary>
    /// 状態追跡付きバリア遷移
    /// </summary>
    void TransitionResourceWithTracking(ID3D12Resource* resource, D3D12_RESOURCE_STATES newState);

    // リソース状態取得
    D3D12_RESOURCE_STATES GetResourceState(ID3D12Resource* resource) const;

    // 初期リソース状態設定（バリア遷移なし）
    void SetInitialResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState);

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

  private: // 内部構造体
    /// <summary>
    /// 一時エフェクト情報
    /// </summary>
    struct TemporaryEffectInfo {
      float duration;           ///< 持続時間（秒）
      float elapsedTime;        ///< 経過時間（秒）
      EasingType easing;        ///< イージング種別
      EffectParam baseParam;    ///< 基本パラメータ（開始時の値）
    };

  private: // メンバ変数
    // DX12の基本情報
    DX12Basic* m_dx12_ = nullptr;

    Camera* camera_ = nullptr; // カメラ情報

    // UI 用の選択状態
    std::string selectedAvailableEffect_ = "";    // 利用可能エフェクトの選択
    std::string selectedActiveEffect_ = "";       // アクティブエフェクトの選択

    // エフェクト適用対象用 RT
    RenderTexture effectTargetRT_;

    // 非適用対象用 RT
    RenderTexture nonEffectTargetRT_;

    // 中間バッファ（複数エフェクト用）
    std::vector<RenderTexture> intermediateRTs_;

    // エフェクトのレジストリ
    std::unordered_map<std::string, std::unique_ptr<IPostEffect>> effectRegistry_;

    // エフェクトチェーン
    std::vector<std::string> effectChain_;

    /// <summary>
    /// 一時エフェクトの管理マップ
    /// </summary>
    std::unordered_map<std::string, TemporaryEffectInfo> temporaryEffects_;

    // 利用可能なエフェクトのリスト（ImGui 用）
    std::vector<std::string> availableEffects_;

    // 深度バッファの SRV
    uint32_t depthSrvIndex_ = 0;

    // Dissolve マスクテクスチャの SRV
    uint32_t dissolveMaskSrvIndex_ = 0;

    // クリアカラー
    const Vector4 kEffectTargetClearColor_ = { 0.17f, 0.17f, 0.17f, 1.0f };
    Vector4 nonEffectTargetClearColor_ = { 0.0f, 0.0f, 0.0f, 0.0f };

    // リソース状態追跡用マップ
    mutable std::unordered_map<ID3D12Resource*, D3D12_RESOURCE_STATES> resourceStates_;

  };

} // namespace Tako
