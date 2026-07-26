#pragma once
#include <cstdint>
#include <d3d12.h>
#include <initializer_list>
#include <string>
#include <variant>
#include <wrl.h>

#include "PostEffectStruct.h"

namespace Tako {

  struct Vector4;
  class DX12Basic;

  /// <summary>
  /// ポストエフェクト基底インターフェースクラス
  /// Bloom、Vignette、RadialBlur など各種ポストエフェクトの共通インターフェースを定義
  /// 各エフェクトはこのクラスを継承し、Apply()メソッドで独自の画像処理を実装
  /// ルートシグネチャと PSO の管理、ImGui デバッグ UI 統合をサポート
  /// </summary>
  class IPostEffect
  {
  public: //定数
    // 全派生エフェクト共通のルートパラメータ番号。BuildRootSignature は宣言リスト順がスロット順になるため、
    // 各派生は先頭を入力テクスチャ SRV、次をパラメータ CBV の順で宣言する（固有パラメータは 2 以降）
    static constexpr uint32_t kInputTextureParam = 0;  ///< t0: 入力テクスチャ
    static constexpr uint32_t kParameterCbvParam = 1;  ///< b0: エフェクトパラメータ

  public: //メンバー関数
    /// <summary>
    /// デストラクタ
    /// </summary>
    virtual ~IPostEffect() = default;

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dx12">DirectX12基盤</param>
    /// <param name="shaderName">シェーダー名</param>
    virtual void Initialize(DX12Basic* dx12, const std::string& shaderName);

    /// <summary>
    /// エフェクトを適用
    /// </summary>
    /// <param name="inputSrvIndex">入力テクスチャの SRV インデックス</param>
    /// <param name="outputRtvHandle">出力先の RTV ハンドル</param>
    /// <param name="depthSrvIndex">深度バッファの SRV インデックス</param>
    /// <param name="clearColor">出力先の RTV のクリアカラー</param>
    virtual void Apply(
      uint32_t                    inputSrvIndex,
      D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle,
      [[maybe_unused]] uint32_t   depthSrvIndex,
      [[maybe_unused]] const Vector4& clearColor
    ) = 0;

    /// <summary>
    /// ImGui でデバッグ UI を描画
    /// </summary>
    virtual void DrawImgui() = 0;

    /// <summary>
    /// 汎用パラメータを設定
    /// </summary>
    /// <param name="param">エフェクトパラメータ</param>
    /// <returns>設定成功の場合 true</returns>
    virtual bool SetGenericParam(const EffectParam& param) { param; return false; }

    /// <summary>
    /// 深度バッファが必要か判定
    /// </summary>
    /// <returns>必要な場合 true</returns>
    virtual bool RequiresDepthBuffer() const { return false; }

  protected: //構造体

    // ComPtr のエイリアス
    template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    /// <summary>
    /// サンプラー補間モード
    /// </summary>
    enum class SamplerFilterMode { Linear, Point };

    /// <summary>
    /// サンプラーアドレスモード (U/V に適用。W は常に WRAP)
    /// </summary>
    enum class SamplerAddressMode { Wrap, Clamp };

    /// <summary>
    /// ルートパラメータ指定。宣言順がそのままルートパラメータのスロット順になる
    /// </summary>
    struct RootParam {
      enum Kind { SrvTable, Cbv } kind;  ///< SRV ディスクリプタテーブル or CBV
      uint32_t shaderRegister;           ///< t#/b# のレジスタ番号
    };

  protected: //メンバー関数

    /// <summary>
    /// ルートシグネチャを作成
    /// </summary>
    virtual void CreateRootSignature() = 0;

    /// <summary>
    /// パイプラインステートオブジェクトを作成
    /// </summary>
    virtual void CreatePSO() = 0;

    /// <summary>
    /// 指定したルートパラメータ列と静的サンプラー1個で rootSignature_ を構築
    /// </summary>
    void BuildRootSignature(std::initializer_list<RootParam> params,
                            SamplerFilterMode filter = SamplerFilterMode::Linear,
                            SamplerAddressMode addrUV = SamplerAddressMode::Wrap);

    /// <summary>
    /// FullScreen.VS + shaderName_.PS の全画面描画 PSO を pipelineState_ に構築
    /// </summary>
    void BuildFullScreenPSO();

    /// <summary>
    /// 全画面1パスを描画する (RTV設定→RS/PSO→トポロジ→CBV(b0)→SRVテーブル(t0)→Draw)。
    /// 単一入力テクスチャのポストエフェクトパス共通処理
    /// </summary>
    void DrawFullScreenPass(ID3D12RootSignature* rootSig, ID3D12PipelineState* pso,
                            D3D12_CPU_DESCRIPTOR_HANDLE outputRtv,
                            D3D12_GPU_VIRTUAL_ADDRESS cbvAddress, uint32_t inputSrvIndex);

  protected: //メンバー変数

    DX12Basic*                  dx12_        = nullptr;  ///< DirectX12基盤システムへの参照
    std::string                 shaderName_;               ///< 使用するシェーダーのファイル名（拡張子なし）
    ComPtr<ID3D12RootSignature> rootSignature_;            ///< このエフェクト用のルートシグネチャ
    ComPtr<ID3D12PipelineState> pipelineState_;            ///< このエフェクト用のパイプラインステート
  };

} // namespace Tako
