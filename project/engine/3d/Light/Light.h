#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <vector>
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Matrix4x4.h"

namespace Tako {

  class DX12Basic;
  class Camera;

  /// <summary>
  /// 平行光源・点光源・スポットライトを管理するクラス
  /// </summary>
  class Light
  {
  public: //定数
    // ライトの最大数
    static const int32_t MAX_DIRECTIONAL_LIGHT = 1;  ///< 平行光源の最大数
    static const int32_t MAX_POINT_LIGHT = 256;  ///< 点光源の最大数
    static const int32_t MAX_SPOT_LIGHT = 256;  ///< スポットライトの最大数

  public: //構造体
    /// <summary>
    /// 平行光源データ
    /// </summary>
    struct DirectionalLight
    {
      Vector4 color;
      Vector3 direction;
      int32_t lightType;  ///< 0:Lambert 1:Half-Lambert
      float intensity;
      Matrix4x4 viewMatrix;  ///< シャドウマップ用
      Matrix4x4 projMatrix;  ///< シャドウマップ用
      Matrix4x4 viewProjMatrix;  ///< シャドウマップ用
      Vector3 position;  ///< シャドウマップ用
    };

    /// <summary>
    /// 点光源データ
    /// </summary>
    struct PointLight
    {
      Vector4 color;
      Vector3 position;
      float intensity;
      float radius;
      float decay;
      bool enable;
    };

    /// <summary>
    /// スポットライトデータ
    /// </summary>
    struct SpotLight
    {
      Vector4 color;
      Vector3 position;
      float intensity;
      Vector3 direction;
      float distance;
      float decay;
      float cosAngle;  ///< コーン半角のコサイン
      bool enable;
    };

    /// <summary>
    /// ライト数定数
    /// </summary>
    struct LightConstants
    {
      int numPointLights;
      int numSpotLights;
      int pad1;
      int pad2;
    };

  public: //メンバー関数
    ~Light();

    void Initialize(DX12Basic* dx12);
    void Update();

    /// <summary>
    /// ライト用の各リソースをルートシグネチャにバインドする
    /// </summary>
    void PreDraw();

    /// <summary>
    /// カメラ視錐台を覆う平行光源のシャドウ行列（view/proj/viewProj）を計算する
    /// </summary>
    /// <param name="camera">基準カメラ。nullptr なら何もしない</param>
    /// <param name="maxShadowDistance">影を収める視錐台の最大奥行き</param>
    void UpdateDirectionalLightShadowMatrices(const Camera* camera, float maxShadowDistance = 1000.0f);

    //============================================================
    //Setter
    //============================================================
    // DirectionalLight
    /// <summary>
    /// 平行光源をまとめて設定（自動更新時は位置をシーン中心へリセット）
    /// </summary>
    void SetDirectionalLight(const Vector3& direction, const Vector4& color, int32_t lightType, float intensity);

    /// <summary>
    /// 平行光源の方向を設定（自動更新時は位置をシーン中心へリセット）
    /// </summary>
    void SetDirectionalLightDirection(const Vector3& direction);
    void SetDirectionalLightColor(const Vector4& color) { directionalLightData_->color = color; }
    void SetDirectionalLightType(int32_t lightType) { directionalLightData_->lightType = lightType; }
    void SetDirectionalLightIntensity(float intensity) { directionalLightData_->intensity = intensity; }
    void SetDirectionalLightPosition(const Vector3& position) { directionalLightData_->position = position; }

    // 自動更新
    void SetAutoUpdatePosition(bool enable) { autoUpdatePosition_ = enable; }
    void SetSceneCenter(const Vector3& center) { sceneCenter_ = center; }

    // PointLight
    /// <summary>
    /// index 番の点光源をまとめて設定
    /// </summary>
    void SetPointLight(const Vector3& position, const Vector4& color, float intensity, float radius, float decay, bool enable, int index);
    void SetPointLightColor(const Vector4& color, int index) { pointLightData_[index].color = color; }
    void SetPointLightPos(const Vector3& position, int index) { pointLightData_[index].position = position; }
    void SetPointLightIntensity(float intensity, int index) { pointLightData_[index].intensity = intensity; }
    void SetPointLightRadius(float radius, int index) { pointLightData_[index].radius = radius; }
    void SetPointLightDecay(float decay, int index) { pointLightData_[index].decay = decay; }
    void SetPointLightEnable(bool enable, int index) { pointLightData_[index].enable = enable; }

    // SpotLight
    /// <summary>
    /// index 番のスポットライトをまとめて設定
    /// </summary>
    /// <param name="cosAngle">コーン半角のコサイン</param>
    void SetSpotLight(const Vector3& position, const Vector3& direction, const Vector4& color, float intensity, float distance, float decay, float cosAngle, bool enable, int index);
    void SetSpotLightColor(const Vector4& color, int index) { spotLightData_[index].color = color; }
    void SetSpotLightPos(const Vector3& position, int index) { spotLightData_[index].position = position; }
    void SetSpotLightIntensity(float intensity, int index) { spotLightData_[index].intensity = intensity; }
    void SetSpotLightDirection(const Vector3& direction, int index) { spotLightData_[index].direction = direction; }
    void SetSpotLightDistance(float distance, int index) { spotLightData_[index].distance = distance; }
    void SetSpotLightDecay(float decay, int index) { spotLightData_[index].decay = decay; }

    /// <summary>
    /// スポットライトのコーン半角のコサインを設定
    /// </summary>
    void SetSpotLightCosAngle(float cosAngle, int index) { spotLightData_[index].cosAngle = cosAngle; }
    void SetSpotLightEnable(bool enable, int index) { spotLightData_[index].enable = enable; }

    //============================================================
    //Getter
    //============================================================
    const DirectionalLight& GetDirectionalLight() const { return *directionalLightData_; }
    bool GetAutoUpdatePosition() const { return autoUpdatePosition_; }
    Vector3 GetSceneCenter() const { return sceneCenter_; }

  private: //非公開関数
    void CreateDirectionalLightData();
    void CreatePointLightData();
    void CreateSpotLightData();
    void CreateLightConstants();

  private: //メンバー変数
    //基盤
    DX12Basic* dx12_;

    //マップ済みデータ
    DirectionalLight* directionalLightData_;
    PointLight*       pointLightData_;
    SpotLight*        spotLightData_;
    LightConstants*   lightConstantsData_;

    //GPUリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> lightConstantsResource_;

    //SRVインデックス
    uint32_t pointLightSrvIndex_ = 0;
    uint32_t spotLightSrvIndex_  = 0;

    //有効ライトのインデックスリスト
    std::vector<uint32_t> pointLightIndexList_;  ///< サイズ = 有効な点光源数
    std::vector<uint32_t> spotLightIndexList_;   ///< サイズ = 有効なスポットライト数

    //シャドウマップ自動更新
    bool    autoUpdatePosition_ = true;
    Vector3 sceneCenter_        = { 0.0f, 0.0f, 0.0f };
  };

} // namespace Tako