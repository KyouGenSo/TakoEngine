#pragma once
#include <chrono>
#include <cstdint>
#include <d3d12.h>
#include <d3d12shader.h>
#include <dxcapi.h>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <wrl.h>

namespace Tako {

  enum class ShaderStage : uint8_t { Vertex, Pixel, Compute };

  /// <summary>
  /// コンパイル単位の識別子。file はファイル名だけならエンジンのシェーダーフォルダ、パスなら作業ディレクトリ基準
  /// </summary>
  struct ShaderKey {
    std::wstring              file;
    ShaderStage               stage = ShaderStage::Vertex;
    std::wstring              entry = L"main";
    std::vector<std::wstring> defines;  ///< "NAME" または "NAME=VALUE"

    bool operator==(const ShaderKey&) const = default;
  };

  /// <summary>
  /// コンパイル済みシェーダー。ホットリロードで中身が差し替わってもアドレスは変わらない
  /// </summary>
  class Shader {
  public: //メンバー関数
    //=====================================
    //Getter
    //=====================================
    const ShaderKey& GetKey() const { return key_; }
    D3D12_SHADER_BYTECODE GetBytecode() const { return { blob_->GetBufferPointer(), blob_->GetBufferSize() }; }
    ID3D12ShaderReflection* GetReflection() const { return reflection_.Get(); }

    /// <summary>
    /// 再コンパイルのたびに増える番号。PSO が古いかどうかの判定に使う
    /// </summary>
    uint32_t GetVersion() const { return version_; }

  private: //メンバー変数
    friend class ShaderLibrary;

    ShaderKey                                      key_;
    Microsoft::WRL::ComPtr<IDxcBlob>               blob_;
    Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection_;
    uint32_t                                       version_ = 0;

    //ホットリロード
    std::vector<std::filesystem::path>           sources_;      ///< 本体と include したファイル
    std::vector<std::filesystem::file_time_type> sourceTimes_;  ///< コンパイル時点の sources_ の更新時刻
  };

  /// <summary>
  /// DXC によるシェーダーのコンパイルとキャッシュ。Debug ではファイルの更新を検出して再コンパイルする
  /// </summary>
  class ShaderLibrary {
  private: //定数
    static constexpr std::chrono::milliseconds kReloadCheckInterval{ 500 };

  private: //構造体
    /// <summary>
    /// 既定のインクルードハンドラへ委譲しつつ、読んだファイルを記録する
    /// </summary>
    class RecordingIncludeHandler : public IDxcIncludeHandler {
    public:
      HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR fileName, IDxcBlob** includeSource) override;
      HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override;
      ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
      ULONG STDMETHODCALLTYPE Release() override { return 1; }

      Microsoft::WRL::ComPtr<IDxcIncludeHandler> inner;
      std::vector<std::filesystem::path>*        loaded = nullptr;
    };

  public: //メンバー関数
    /// <summary>
    /// DXC コンパイラを生成する
    /// </summary>
    void Initialize();

    /// <summary>
    /// コンパイル済みシェーダーを返す。初めてのキーならコンパイルする
    /// </summary>
    /// <param name="key">コンパイル単位</param>
    const Shader* Get(const ShaderKey& key);

    /// <summary>
    /// エントリポイント main・define なしのシェーダーを返す
    /// </summary>
    const Shader* Get(std::wstring_view file, ShaderStage stage) { return Get(ShaderKey{ std::wstring(file), stage }); }

    /// <summary>
    /// 本体か include 先が更新されたシェーダーを再コンパイルする。失敗したものは前の結果を使い続ける
    /// </summary>
    /// <returns>1 つでも差し替えたら true</returns>
    bool ReloadChangedShaders();

  private: //非公開関数
    /// <summary>
    /// コンパイルして shader に書き込む。失敗時は shader を変更しない
    /// </summary>
    bool Compile(Shader& shader);

    std::filesystem::path ResolvePath(const std::wstring& file) const;

  private: //メンバー変数
    Microsoft::WRL::ComPtr<IDxcUtils>       utils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3>   compiler_;
    RecordingIncludeHandler                 includeHandler_;
    std::filesystem::path                   engineShaderDir_;  ///< ファイル名だけのキーを探すフォルダ

    std::unordered_map<std::wstring, std::unique_ptr<Shader>> shaders_;          ///< キーを連結した文字列で引く
    std::chrono::steady_clock::time_point                     lastReloadCheck_{};
  };

} // namespace Tako
