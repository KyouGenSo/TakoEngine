#pragma once
#include<string>
#include<vector>
#include<unordered_map>
#include<wrl.h>
#include <d3d12.h>
#include <memory>
#include"DirectXTex.h"

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// テクスチャの読み込みと管理を行うシングルトンクラス。テクスチャのキャッシュと SRV 管理を担当
  /// </summary>
  class TextureManager {
  private: // シングルトン設定
    static std::unique_ptr<TextureManager> instance_;

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~TextureManager() = default;

    friend struct std::default_delete<TextureManager>;

  public:
    explicit TextureManager(Token) {}
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

  private: //構造体

    /// <summary>
    /// テクスチャデータ構造体
    /// </summary>
    struct TextureData
    {
      std::string fileName;
      DirectX::TexMetadata metadata;
      Microsoft::WRL::ComPtr<ID3D12Resource> resource;
      Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
      uint32_t srvIndex;
      D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle;
      D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle;
    };

  public: //メンバー関数

    /// <summary>
    /// インスタンスの取得
    /// </summary>
    static TextureManager* GetInstance();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dx12">DX12Basic のインスタンス</param>
    /// <param name="directoryPath">テクスチャ格納ディレクトリのパス</param>
    void Initialize(DX12Basic* dx12, const std::string& directoryPath);

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// テクスチャファイルの読み込み
    /// </summary>
    /// <param name="fileName">読み込むテクスチャファイルの名前</param>
    void LoadTexture(const std::string& fileName);

    /// <summary>
    /// エンジン用デフォルトテクスチャの読み込み（EngineResources/Texture/ 配下から）
    /// </summary>
    /// <param name="fileName">エンジン用テクスチャファイルの名前</param>
    void LoadEngineDefault(const std::string& fileName);

    /// <summary>
    /// テクスチャの個別解放。SRV インデックスを返却しエントリを削除する。
    /// 呼び出し側が全使用者（キャッシュ済み srvIndex 含む）の不使用を保証し、フレーム境界（EndDraw 後）で呼ぶこと。
    /// 解放された index は次の Allocate で即再利用される
    /// </summary>
    /// <param name="fileName">解放するテクスチャファイルの名前（LoadTexture に渡したキー）</param>
    void Unload(const std::string& fileName);

    /// <summary>
    /// 全テクスチャのアップロード用中間リソースを解放する。
    /// ロードを行ったフレームの EndDraw（GPU 待機）より後に呼ぶこと
    /// </summary>
    void ReleaseIntermediateResources();

    //============================================================
    //Getter
    //============================================================
    /// <summary>
    /// テクスチャのインデックスから GPU ハンドルを取得
    /// </summary>
    /// <param name="fileName">テクスチャファイルの名前</param>
    /// <returns>SRV の GPU ディスクリプタハンドル</returns>
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle(const std::string& fileName);

    /// <summary>
    /// メタデータを取得
    /// </summary>
    /// <param name="fileName">テクスチャファイルの名前</param>
    /// <returns>テクスチャのメタデータ</returns>
    const DirectX::TexMetadata& GetMetaData(const std::string& fileName);

    /// <summary>
    /// メタデータを取得
    /// </summary>
    /// <param name="srvIndex">テクスチャファイルのSRVIndex</param>
    /// <returns>テクスチャのメタデータ</returns>
    const DirectX::TexMetadata& GetMetaData(uint32_t srvIndex);

    /// <summary>
    /// srvIndex を取得
    /// </summary>
    /// <param name="fileName">テクスチャファイルの名前</param>
    /// <returns>SRV インデックス</returns>
    uint32_t GetSRVIndex(const std::string& fileName);

    /// <summary>
    /// SRVIndex からテクスチャのファイル名を取得
    /// </summary>
    /// <param name="srvIndex">テクスチャの SRV インデックス</param>
    /// <returns>テクスチャファイル名（未登録の場合は空文字）</returns>
    const std::string& GetFileName(uint32_t srvIndex);

    /// <summary>
    /// ロード済みテクスチャのファイル名一覧を取得（ソート済み）。
    /// エディタのテクスチャ選択 UI などで使用する。
    /// </summary>
    /// <returns>ロード済みテクスチャファイル名のソート済みリスト</returns>
    std::vector<std::string> GetLoadedTextureFileNames() const;

    /// <summary>
    /// エンジン用デフォルトテクスチャの GPU ハンドル取得
    /// </summary>
    /// <param name="fileName">エンジン用テクスチャファイルの名前</param>
    /// <returns>SRV の GPU ディスクリプタハンドル</returns>
    D3D12_GPU_DESCRIPTOR_HANDLE GetEngineDefaultSRVGPUHandle(const std::string& fileName);

    /// <summary>
    /// エンジン用デフォルトテクスチャのメタデータ取得
    /// </summary>
    /// <param name="fileName">エンジン用テクスチャファイルの名前</param>
    /// <returns>テクスチャのメタデータ</returns>
    const DirectX::TexMetadata& GetEngineDefaultMetaData(const std::string& fileName);

    /// <summary>
    /// エンジン用デフォルトテクスチャの SRV インデックス取得
    /// </summary>
    /// <param name="fileName">エンジン用テクスチャファイルの名前</param>
    /// <returns>SRV インデックス</returns>
    uint32_t GetEngineDefaultSRVIndex(const std::string& fileName);

  private: //メンバー変数

    DX12Basic* dx12_ = nullptr;  ///< DX12Basic クラスのインスタンス

    std::string directoryPath_;  ///< テクスチャ格納ディレクトリ

    std::unordered_map<std::string, TextureData> textureData_;  ///< テクスチャデータ配列

  };

} // namespace Tako
