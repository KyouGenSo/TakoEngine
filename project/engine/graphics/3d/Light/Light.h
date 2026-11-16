#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <span>
#include <vector>
#include "vector2.h"
#include "vector3.h"
#include "ColorFunc.h"

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// 平行光源・点光源・スポットライトを管理するクラス
  /// </summary>
  class TAKO_API Light
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
      Color   color;
      Vector3 direction;
      int32_t lightType;  ///< 0:Lambert 1:Half-Lambert
      float intensity;
      float padding[3];  ///< 定数バッファの 16 バイト境界まで
    };

    /// <summary>
    /// 点光源データ
    /// </summary>
    struct PointLight
    {
      Color   color;
      Vector3 position;
      float intensity;
      float radius;
      float decay;
      bool enable;
      int32_t shadowSlice;  ///< 影の区画の番号（6 面の先頭）。負なら影なし。ShadowRenderer が書く
    };

    /// <summary>
    /// スポットライトデータ
    /// </summary>
    struct SpotLight
    {
      Color   color;
      Vector3 position;
      float intensity;
      Vector3 direction;
      float distance;
      float decay;
      float cosAngle;  ///< コーン半角のコサイン
      bool enable;
      int32_t shadowSlice;  ///< 影の区画の番号。負なら影なし。ShadowRenderer が書く
    };

    /// <summary>
    /// このフレームに影を落とすと登録された点光源・スポットライト 1 つ
    /// </summary>
    struct ShadowRequest
    {
      bool     isSpot;
      uint32_t lightIndex;  ///< 点光源・スポットライトそれぞれの配列の中の番号
      Vector3  position;
      Vector3  direction;   ///< スポットライトの向き
      float    range;
      float    cosAngle;    ///< スポットライトのコーン半角のコサイン
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

    /// <summary>
    /// 点光源とスポットライトを 0 個に戻す。描画物を登録する前に毎フレーム呼ぶ（ライトは描画物と同じく Draw で登録する）
    /// </summary>
    void BeginFrame();

    /// <summary>
    /// このフレームの点光源に足す（MAX_POINT_LIGHT を超えた分は捨てる）
    /// </summary>
    /// <param name="castShadow">影を落とすか（区画が足りなければ、カメラから遠いライトは影なしになる）</param>
    void AddPointLight(const PointLight& light, bool castShadow = false);

    /// <summary>
    /// このフレームのスポットライトに足す（MAX_SPOT_LIGHT を超えた分は捨てる）
    /// </summary>
    /// <param name="castShadow">影を落とすか（区画が足りなければ、カメラから遠いライトは影なしになる）</param>
    void AddSpotLight(const SpotLight& light, bool castShadow = false);

    /// <summary>
    /// 平行光源を初期値（真下向き・白・強さ 1・Half-Lambert）に戻す
    /// </summary>
    void ResetDirectionalLight();

    /// <summary>
    /// ライト用の各リソースをルートシグネチャにバインドする
    /// </summary>
    void PreDraw() const;

    //============================================================
    //Setter
    //============================================================
    // DirectionalLight
    void SetDirectionalLight(const Vector3& direction, const Color& color, int32_t lightType, float intensity);
    void SetDirectionalLightDirection(const Vector3& direction) { directionalLightData_->direction = direction; }
    void SetDirectionalLightColor(const Color& color) { directionalLightData_->color = color.ToLinear(); }
    void SetDirectionalLightType(int32_t lightType) { directionalLightData_->lightType = lightType; }
    void SetDirectionalLightIntensity(float intensity) { directionalLightData_->intensity = intensity; }

    /// <summary>
    /// request のライトが読む影の区画の番号を書く
    /// </summary>
    void SetShadowSlice(const ShadowRequest& request, int32_t slice);

    //============================================================
    //Getter
    //============================================================
    /// <summary>
    /// GPU に渡している値（color は Set* で渡した色をリニアに直した後の値）
    /// </summary>
    const DirectionalLight& GetDirectionalLight() const { return *directionalLightData_; }

    /// <summary>
    /// このフレームに影を落とすと登録された点光源・スポットライト
    /// </summary>
    std::span<const ShadowRequest> GetShadowRequests() const { return shadowRequests_; }

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

    //影
    std::vector<ShadowRequest> shadowRequests_;
  };

} // namespace Tako