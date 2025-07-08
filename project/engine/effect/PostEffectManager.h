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


class Camera;
class IPostEffect;
class DX12Basic;

class PostEffectManager {
private:
  // シングルトン設定
  static PostEffectManager* instance_;

  PostEffectManager() = default;
  ~PostEffectManager() = default;

public:
  PostEffectManager(PostEffectManager&) = delete;
  PostEffectManager& operator=(PostEffectManager&) = delete;

public: // メンバ関数

  // ComPtrのエイリアス
  template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

  // インスタンスの取得
  static PostEffectManager* GetInstance();

  // 初期化
  void Initialize(DX12Basic* dx12);

  // 終了処理
  static void Finalize();

  // エフェクトチェーンの管理
  void AddEffectToChain(const std::string& name);
  void RemoveEffectFromChain(const std::string& name);
  void ClearEffectChain();

  // 描画前の処理
  void BeginDrawEffectTarget();
  void BegineDrawNonEffectTarget();

  // 描画
  void Draw();
  void DrawFinalResult();
  void DrawImgui();

  // レンダーテクスチャの再作成
  void RecreateRenderTexture();

  // 汎用パラメーター設定関数
  bool SetEffectParam(const std::string& effectName, const EffectParam& param);

  template<typename ParamType>
  bool SetEffectParam(const std::string& effectName, const ParamType& param) {
    return SetEffectParam(effectName, EffectParam(param));
  }

  void SetCamera(Camera* camera) {
    if (camera) {
      camera_ = camera;
    }
  }

  // エフェクト順序変更関数
  bool MoveEffectUp(const std::string& effectName);
  bool MoveEffectDown(const std::string& effectName);
  bool MoveEffectToPosition(const std::string& effectName, int newPosition);
  bool SwapEffects(const std::string& effectName1, const std::string& effectName2);
  bool SwapEffectsByIndex(int index1, int index2);

  // エフェクト位置取得
  int GetEffectPosition(const std::string& effectName) const;
  bool IsEffectInChain(const std::string& effectName) const;

  // エフェクトチェーン情報取得
  size_t GetEffectChainSize() const;
  std::string GetEffectAtPosition(int position) const;
  std::vector<std::string> GetEffectChain() const;

private: // プライベートメンバー関数
  // レンダーターゲットの作成
  void CreateRenderTextures();

  // エフェクトの登録
  void RegisterEffect(const std::string& name, std::unique_ptr<IPostEffect> effect);

  // エフェクトを適用
  void ApplyEffectChain();

  // ImGuiヘルパー関数
  void DrawEffectParametersTab();

  // バリアの設定
  void SetBarrier(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);
  void SetBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

private: // メンバ変数
  // レンダーターゲット構造体
  struct RenderTarget {
    ComPtr<ID3D12Resource> resource;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
    uint32_t srvIndex;
  };

  // DX12の基本情報
  DX12Basic* m_dx12_ = nullptr;

  Camera* camera_ = nullptr; // カメラ情報

  // UI用の選択状態
  std::string selectedAvailableEffect_ = "";    // 利用可能エフェクトの選択
  std::string selectedActiveEffect_ = "";       // アクティブエフェクトの選択

  // エフェクト適用対象用RT
  RenderTarget effectTargetRT_;

  // 非適用対象用RT
  RenderTarget nonEffectTargetRT_;

  // 中間バッファ（複数エフェクト用）
  std::vector<RenderTarget> intermediateRTs_;

  // エフェクトのレジストリ
  std::unordered_map<std::string, std::unique_ptr<IPostEffect>> effectRegistry_;

  // エフェクトチェーン
  std::vector<std::string> effectChain_;

  // 利用可能なエフェクトのリスト（ImGui用）
  std::vector<std::string> availableEffects_;

  // 深度バッファのSRV
  uint32_t depthSrvIndex_ = 0;

  // クリアカラー
  const Vector4 kEffectTargetClearColor_ = { 0.17f, 0.17f, 0.17f, 1.0f };
  Vector4 nonEffectTargetClearColor_ = { 0.0f, 0.0f, 0.0f, 0.0f };

};
