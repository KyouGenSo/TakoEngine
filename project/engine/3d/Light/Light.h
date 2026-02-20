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
  /// ライト管理クラス
  /// 平行光源、点光源、スポットライトを統合管理
  /// </summary>
  class Light
  {
  public: // 定数
    // ライトの最大数
    static const int32_t MAX_DIRECTIONAL_LIGHT = 1;  ///< 平行光源の最大数
    static const int32_t MAX_POINT_LIGHT = 256;  ///< 点光源の最大数
    static const int32_t MAX_SPOT_LIGHT = 256;  ///< スポットライトの最大数

  public: // 構造体
    /// <summary>
    /// 平行光源データ
    /// </summary>
    struct DirectionalLight
    {
      Vector4 color;  ///< ライトカラー
      Vector3 direction;  ///< ライト方向ベクトル
      int32_t lightType;  ///< ライトタイプ
      float intensity;  ///< ライト強度
      Matrix4x4 viewMatrix;  ///< ビュー行列（シャドウマップ用）
      Matrix4x4 projMatrix;  ///< プロジェクション行列（シャドウマップ用）
      Matrix4x4 viewProjMatrix;  ///< ビュープロジェクション行列
      Vector3 position;  ///< ライトの位置（シャドウマップ用）
    };

    /// <summary>
    /// 点光源データ
    /// </summary>
    struct PointLight
    {
      Vector4 color;  ///< ライトカラー
      Vector3 position;  ///< ライト位置
      float intensity;  ///< ライト強度
      float radius;  ///< 影響半径
      float decay;  ///< 減衰率
      bool enable;  ///< 有効フラグ
    };

    /// <summary>
    /// スポットライトデータ
    /// </summary>
    struct SpotLight
    {
      Vector4 color;  ///< ライトカラー
      Vector3 position;  ///< ライト位置
      float intensity;  ///< ライト強度
      Vector3 direction;  ///< 照射方向
      float distance;  ///< 有効距離
      float decay;  ///< 減衰率
      float cosAngle;  ///< コーン角のコサイン値
      bool enable;  ///< 有効フラグ
    };

    /// <summary>
    /// ライト数定数
    /// </summary>
    struct LightConstants
    {
      int numPointLights;  ///< 点光源の数
      int numSpotLights;  ///< スポットライトの数
      int pad1;  ///< パディング1
      int pad2;  ///< パディング2
    };

  public: // メンバ関数
    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dx12">DirectX12基盤システムへのポインタ</param>
    void Initialize(DX12Basic* dx12);

    /// <summary>
    /// 更新処理
    /// </summary>
    void Update();

    /// <summary>
    /// 描画設定
    /// </summary>
    void PreDraw();

    //-----------------------------------------Setter-----------------------------------------//
    // DirectionalLight
    /// <summary>
    /// 平行光源を設定
    /// </summary>
    /// <param name="direction">ライト方向</param>
    /// <param name="color">ライトカラー</param>
    /// <param name="lightType">ライトタイプ</param>
    /// <param name="intensity">ライト強度</param>
    void SetDirectionalLight(const Vector3& direction, const Vector4& color, int32_t lightType, float intensity);

    /// <summary>
    /// 平行光源の方向を設定
    /// </summary>
    /// <param name="direction">ライト方向ベクトル</param>
    void SetDirectionalLightDirection(const Vector3& direction);

    /// <summary>
    /// 平行光源のカラーを設定
    /// </summary>
    /// <param name="color">ライトカラー</param>
    void SetDirectionalLightColor(const Vector4& color) { directionalLightData_->color = color; }

    /// <summary>
    /// 平行光源のタイプを設定
    /// </summary>
    /// <param name="lightType">ライトタイプ</param>
    void SetDirectionalLightType(int32_t lightType) { directionalLightData_->lightType = lightType; }

    /// <summary>
    /// 平行光源の強度を設定
    /// </summary>
    /// <param name="intensity">ライト強度</param>
    void SetDirectionalLightIntensity(float intensity) { directionalLightData_->intensity = intensity; }

    /// <summary>
    /// 平行光源の位置を設定
    /// </summary>
    /// <param name="position">ライト位置</param>
    void SetDirectionalLightPosition(const Vector3& position) { directionalLightData_->position = position; }

    /// <summary>
    /// シャドウマップ用の行列計算（カメラの視錐台に基づく）
    /// </summary>
    /// <param name="camera">カメラ</param>
    /// <param name="maxShadowDistance">最大シャドウ距離</param>
    void UpdateDirectionalLightShadowMatrices(const Camera* camera, float maxShadowDistance = 1000.0f);

    // ゲッター
    /// <summary>
    /// 平行光源データを取得
    /// </summary>
    /// <returns>平行光源データの参照</returns>
    const DirectionalLight& GetDirectionalLight() const { return *directionalLightData_; }

    // 自動更新制御
    /// <summary>
    /// 自動更新位置を設定
    /// </summary>
    /// <param name="enable">有効フラグ</param>
    void SetAutoUpdatePosition(bool enable) { autoUpdatePosition_ = enable; }

    /// <summary>
    /// シーン中心を設定
    /// </summary>
    /// <param name="center">中心座標</param>
    void SetSceneCenter(const Vector3& center) { sceneCenter_ = center; }

    /// <summary>
    /// 自動更新位置を取得
    /// </summary>
    /// <returns>自動更新フラグ</returns>
    bool GetAutoUpdatePosition() const { return autoUpdatePosition_; }

    /// <summary>
    /// シーン中心を取得
    /// </summary>
    /// <returns>中心座標</returns>
    Vector3 GetSceneCenter() const { return sceneCenter_; }

    // PointLight
    /// <summary>
    /// 点光源を設定
    /// </summary>
    /// <param name="position">位置</param>
    /// <param name="color">カラー</param>
    /// <param name="intensity">強度</param>
    /// <param name="radius">半径</param>
    /// <param name="decay">減衰率</param>
    /// <param name="enable">有効フラグ</param>
    /// <param name="index">インデックス</param>
    void SetPointLight(const Vector3& position, const Vector4& color, float intensity, float radius, float decay, bool enable, int index);

    /// <summary>
    /// 点光源のカラーを設定
    /// </summary>
    /// <param name="color">カラー</param>
    /// <param name="index">インデックス</param>
    void SetPointLightColor(const Vector4& color, int index) { pointLightData_[index].color = color; }

    /// <summary>
    /// 点光源の位置を設定
    /// </summary>
    /// <param name="position">位置</param>
    /// <param name="index">インデックス</param>
    void SetPointLightPos(const Vector3& position, int index) { pointLightData_[index].position = position; }

    /// <summary>
    /// 点光源の強度を設定
    /// </summary>
    /// <param name="intensity">強度</param>
    /// <param name="index">インデックス</param>
    void SetPointLightIntensity(float intensity, int index) { pointLightData_[index].intensity = intensity; }

    /// <summary>
    /// 点光源の半径を設定
    /// </summary>
    /// <param name="radius">半径</param>
    /// <param name="index">インデックス</param>
    void SetPointLightRadius(float radius, int index) { pointLightData_[index].radius = radius; }

    /// <summary>
    /// 点光源の減衰率を設定
    /// </summary>
    /// <param name="decay">減衰率</param>
    /// <param name="index">インデックス</param>
    void SetPointLightDecay(float decay, int index) { pointLightData_[index].decay = decay; }

    /// <summary>
    /// 点光源の有効化を設定
    /// </summary>
    /// <param name="enable">有効フラグ</param>
    /// <param name="index">インデックス</param>
    void SetPointLightEnable(bool enable, int index) { pointLightData_[index].enable = enable; }

    // SpotLight
    /// <summary>
    /// スポットライトを設定
    /// </summary>
    /// <param name="position">位置</param>
    /// <param name="direction">方向</param>
    /// <param name="color">カラー</param>
    /// <param name="intensity">強度</param>
    /// <param name="distance">距離</param>
    /// <param name="decay">減衰率</param>
    /// <param name="cosAngle">コーン角のコサイン値</param>
    /// <param name="enable">有効フラグ</param>
    /// <param name="index">インデックス</param>
    void SetSpotLight(const Vector3& position, const Vector3& direction, const Vector4& color, float intensity, float distance, float decay, float cosAngle, bool enable, int index);

    /// <summary>
    /// スポットライトのカラーを設定
    /// </summary>
    /// <param name="color">カラー</param>
    /// <param name="index">インデックス</param>
    void SetSpotLightColor(const Vector4& color, int index) { spotLightData_[index].color = color; }

    /// <summary>
    /// スポットライトの位置を設定
    /// </summary>
    /// <param name="position">位置</param>
    /// <param name="index">インデックス</param>
    void SetSpotLightPos(const Vector3& position, int index) { spotLightData_[index].position = position; }

    /// <summary>
    /// スポットライトの強度を設定
    /// </summary>
    /// <param name="intensity">強度</param>
    /// <param name="index">インデックス</param>
    void SetSpotLightIntensity(float intensity, int index) { spotLightData_[index].intensity = intensity; }

    /// <summary>
    /// スポットライトの方向を設定
    /// </summary>
    /// <param name="direction">方向</param>
    /// <param name="index">インデックス</param>
    void SetSpotLightDirection(const Vector3& direction, int index) { spotLightData_[index].direction = direction; }

    /// <summary>
    /// スポットライトの距離を設定
    /// </summary>
    /// <param name="distance">距離</param>
    /// <param name="index">インデックス</param>
    void SetSpotLightDistance(float distance, int index) { spotLightData_[index].distance = distance; }

    /// <summary>
    /// スポットライトの減衰率を設定
    /// </summary>
    /// <param name="decay">減衰率</param>
    /// <param name="index">インデックス</param>
    void SetSpotLightDecay(float decay, int index) { spotLightData_[index].decay = decay; }

    /// <summary>
    /// スポットライトのコーン角を設定
    /// </summary>
    /// <param name="cosAngle">コサイン値</param>
    /// <param name="index">インデックス</param>
    void SetSpotLightCosAngle(float cosAngle, int index) { spotLightData_[index].cosAngle = cosAngle; }

    /// <summary>
    /// スポットライトの有効化を設定
    /// </summary>
    /// <param name="enable">有効フラグ</param>
    /// <param name="index">インデックス</param>
    void SetSpotLightEnable(bool enable, int index) { spotLightData_[index].enable = enable; }


  private: // プライベートメンバ関数
    /// <summary>
    /// 平行光源データの生成
    /// </summary>
    void CreateDirectionalLightData();

    /// <summary>
    /// 点光源データの生成
    /// </summary>
    void CreatePointLightData();

    /// <summary>
    /// スポットライトデータの生成
    /// </summary>
    void CreateSpotLightData();

    /// <summary>
    /// LightConstants の生成
    /// </summary>
    void CreateLightConstants();

  private: // メンバ変数
    DX12Basic* m_dx12_;  ///< DirectX12基盤システムへのポインタ

    DirectionalLight* directionalLightData_;  ///< 平行光源データポインタ

    PointLight* pointLightData_;  ///< 点光源データ配列ポインタ

    SpotLight* spotLightData_;  ///< スポットライトデータ配列ポインタ

    LightConstants* lightConstantsData_;  ///< ライト定数データポインタ

    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;  ///< 平行光源リソース

    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;  ///< 点光源リソース

    Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;  ///< スポットライトリソース

    Microsoft::WRL::ComPtr<ID3D12Resource> lightConstantsResource_;  ///< ライト定数リソース

    uint32_t pointLightSrvIndex_;  ///< 点光源の SRV インデックス

    uint32_t spotLightSrvIndex_;  ///< スポットライトの SRV インデックス

    std::vector<uint32_t> pointLightIndexList_;  ///< 点光源インデックスリスト

    std::vector<uint32_t> spotLightIndexList_;  ///< スポットライトインデックスリスト

    // シャドウマップ自動更新関連
    bool autoUpdatePosition_ = true;  ///< 位置の自動更新フラグ
    Vector3 sceneCenter_ = { 0.0f, 0.0f, 0.0f };  ///< シーン中心位置
  };

} // namespace Tako