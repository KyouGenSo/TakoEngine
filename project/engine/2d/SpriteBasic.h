#include <d3d12.h>
#include <dxgi1_6.h>
#include<wrl.h>
#include <memory>
#include "DX12Basic.h"
#include "Matrix4x4.h"

namespace Tako {

  /// <summary>
  /// スプライト描画の基盤クラス。パイプライン、ルートシグネチャ、ビュープロジェクション行列を管理
  /// </summary>
  class SpriteBasic {
  private: // シングルトン設定

    ///< インスタンス
    static std::unique_ptr<SpriteBasic> instance_;

    SpriteBasic() = default;
    ~SpriteBasic() = default;
    SpriteBasic(SpriteBasic&) = delete;
    SpriteBasic& operator=(SpriteBasic&) = delete;

    friend struct std::default_delete<SpriteBasic>;

  public: //メンバー関数

    // ComPtr のエイリアス
    template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    /// <summary>
    /// インスタンスの取得
    /// </summary>
    static SpriteBasic* GetInstance();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dx12">DX12Basic のインスタンス</param>
    void Initialize(DX12Basic* dx12);

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// 共通描画設定
    /// </summary>
    void SetCommonRenderSetting();

    /// <summary>
    /// 画面サイズが変わったときに呼び出すコールバック関数
    /// </summary>
    /// <param name="size">新しい画面サイズ</param>
    void OnResize(const Vector2& size);

    //====================================
    //Getter
    //====================================
    DX12Basic* GetDX12Basic() { return dx12_; }
    const Matrix4x4& GetViewMatrix() { return viewMatrixSprite_; }
    const Matrix4x4& GetProjectionMatrix() { return projectionMatrixSprite_; }

  private: //非公開関数
    /// <summary>
    /// ルートシグネチャの作成
    /// </summary>
    void CreateRootSignature();

    /// <summary>
    /// パイプラインステートの生成
    /// </summary>
    void CreatePSO();

  private: //メンバー変数
    DX12Basic*                  dx12_;                       ///< DX12Basic クラスのインスタンス
    ComPtr<ID3D12RootSignature> rootSignature_;                ///< ルートシグネチャ
    ComPtr<ID3D12PipelineState> pipelineState_;                ///< パイプラインステート
    Matrix4x4                   viewMatrixSprite_       = {};  ///< ビュー行列
    Matrix4x4                   projectionMatrixSprite_ = {};  ///< プロジェクション行列
  };

} // namespace Tako