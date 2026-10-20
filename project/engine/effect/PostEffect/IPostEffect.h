#pragma once
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <wrl.h>

#include "CommandContext.h"
#include "PostEffectStruct.h"

namespace Tako {

  struct Vector4;
  class DX12Basic;

  /// <summary>
  /// ポストエフェクト基底インターフェースクラス
  /// Bloom、Vignette、RadialBlur など各種ポストエフェクトの共通インターフェースを定義
  /// 各エフェクトはこのクラスを継承し、Apply()メソッドで独自の画像処理を実装
  /// シェーダーは入力テクスチャを gTexture、パラメータを gParam という名前で受ける
  /// </summary>
  class IPostEffect
  {
  public: //メンバー関数
    /// <summary>
    /// デストラクタ
    /// </summary>
    virtual ~IPostEffect() = default;

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dx12">DirectX12基盤</param>
    /// <param name="shaderName">シェーダー名（shaderName.PS.hlsl を全画面描画に使う）</param>
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

    /// <summary>
    /// SetGenericParam で受け付けるパラメータの現在値（プロファイル保存用。並び順がそのまま保存順になる）
    /// </summary>
    virtual std::vector<EffectParam> GetGenericParams() const { return {}; }

  protected: //構造体

    // ComPtr のエイリアス
    template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

  protected: //メンバー関数

    /// <summary>
    /// FullScreen.VS と指定した PS で全画面を描くパイプライン
    /// </summary>
    /// <param name="psFile">ピクセルシェーダーのファイル名</param>
    GraphicsPipelineDesc MakeFullScreenPipeline(std::wstring_view psFile) const;

    /// <summary>
    /// 描画先とパイプラインを設定し、入力テクスチャを gTexture に渡す。追加の値を渡してから DrawFullScreen を呼ぶ
    /// </summary>
    /// <param name="pipeline">全画面描画のパイプライン</param>
    /// <param name="outputRtv">描画先の RTV</param>
    /// <param name="inputSrvIndex">入力テクスチャの SRV インデックス</param>
    /// <returns>追加の値を渡す窓口</returns>
    CommandContext::ShaderBinder BeginFullScreenPass(const GraphicsPipelineDesc& pipeline, D3D12_CPU_DESCRIPTOR_HANDLE outputRtv, uint32_t inputSrvIndex);

    /// <summary>
    /// shaderName.PS のパイプラインで BeginFullScreenPass する
    /// </summary>
    CommandContext::ShaderBinder BeginFullScreenPass(D3D12_CPU_DESCRIPTOR_HANDLE outputRtv, uint32_t inputSrvIndex) { return BeginFullScreenPass(GetPipeline(), outputRtv, inputSrvIndex); }

    /// <summary>
    /// 全画面三角形を描く
    /// </summary>
    void DrawFullScreen();

    /// <summary>
    /// shaderName.PS の全画面描画パイプライン（初めて使うときに作る）
    /// </summary>
    const GraphicsPipelineDesc& GetPipeline();

  protected: //メンバー変数

    DX12Basic*           dx12_       = nullptr;  ///< DirectX12基盤システムへの参照
    std::string          shaderName_;              ///< 使用するシェーダーのファイル名（拡張子なし）
    GraphicsPipelineDesc pipeline_{};              ///< GetPipeline が作る shaderName.PS の全画面描画
  };

} // namespace Tako
