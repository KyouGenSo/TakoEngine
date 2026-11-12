#pragma once
#include <cstdint>
#include <d3d12.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <wrl.h>

namespace Tako {

  class Shader;

  /// <summary>
  /// シェーダー内のリソース名を FNV-1a でハッシュした識別子
  /// </summary>
  using ShaderParamId = uint32_t;

  constexpr ShaderParamId MakeShaderParamId(std::string_view name)
  {
    uint32_t hash = 2166136261u;
    for (char c : name) {
      hash = (hash ^ static_cast<uint8_t>(c)) * 16777619u;
    }
    return hash;
  }

  constexpr ShaderParamId operator""_id(const char* name, size_t length)
  {
    return MakeShaderParamId(std::string_view(name, length));
  }

  /// <summary>
  /// シェーダーが使うリソース 1 つと、それを受けるルートパラメータ
  /// </summary>
  struct ShaderParameter {
    enum class Kind : uint8_t { Cbv, Srv, Uav };

    ShaderParamId           id = 0;
    std::string             name;
    Kind                    kind = Kind::Cbv;
    uint32_t                shaderRegister = 0;
    uint32_t                registerSpace = 0;
    uint32_t                descriptorCount = 1;  ///< SRV/UAV の配列要素数（テーブルの連続ディスクリプタ数）
    D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
  };

  /// <summary>
  /// パイプラインを構成するシェーダー群のリフレクションから作るルートシグネチャの内容。
  /// CBV はルート CBV、SRV/UAV はディスクリプタ 1 つ分のテーブル、サンプラーは Common.hlsli の固定 static sampler
  /// </summary>
  class ShaderBindingLayout {
  public: //定数
    static constexpr uint32_t kMaxParameters = 64;  ///< 束縛済みかどうかを 64bit マスクで持つ上限

  public: //メンバー関数
    /// <summary>
    /// シェーダー群が使うリソースを集めてレイアウトを作る。同名・同レジスタを複数ステージが使えば 1 つにまとめる
    /// </summary>
    /// <param name="shaders">VS/PS または CS</param>
    /// <param name="usesInputAssembler">頂点バッファ入力を使うか</param>
    ShaderBindingLayout(std::span<const Shader* const> shaders, bool usesInputAssembler);

    /// <summary>
    /// ルートシグネチャをシリアライズする（同じ内容なら同じバイト列になる）
    /// </summary>
    Microsoft::WRL::ComPtr<ID3DBlob> Serialize() const;

    //=========================================
    //Setter
    //=========================================
    void SetRootSignature(ID3D12RootSignature* rootSignature) { rootSignature_ = rootSignature; }

    //=========================================
    //Getter
    //=========================================
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_; }
    std::span<const ShaderParameter> GetParameters() const { return parameters_; }

    /// <summary>
    /// 全パラメータを束縛し終えたときのマスク
    /// </summary>
    uint64_t GetRequiredMask() const { return parameters_.size() >= 64 ? ~0ull : (1ull << parameters_.size()) - 1; }

  private: //メンバー変数
    std::vector<ShaderParameter> parameters_;  ///< 並び順がルートパラメータ番号
    bool                         usesInputAssembler_ = false;
    ID3D12RootSignature*         rootSignature_      = nullptr;  ///< PipelineCache が内容ごとに共有して所有する
  };

} // namespace Tako
