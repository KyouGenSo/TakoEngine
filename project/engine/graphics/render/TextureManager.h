#pragma once
#include<string>
#include<vector>
#include<unordered_map>
#include<unordered_set>
#include<wrl.h>
#include <d3d12.h>
#include <memory>
#include"DirectXTex.h"

#include "GpuResource.h"

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// テクスチャの読み込みと管理を行うシングルトンクラス。テクスチャのキャッシュと SRV 管理を担当
  /// </summary>
  class TAKO_API TextureManager {
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
      std::string                            fileName;
      DirectX::TexMetadata                   metadata;                  ///< format は色の画像なら _SRGB（リソースは TYPELESS で作る）
      GpuResource                            resource;
      Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
      uint32_t                               srvIndex             = 0;  ///< シーン用。色の画像は GPU がリニアに直して読む
      uint32_t                               rawSrvIndex          = 0;  ///< 表示用（ToRawSRV で初めて頼まれたときに作る）
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
    /// テクスチャファイルの読み込み（読めなければ記録を 1 回出して何もしない）
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
    /// アップロード用の中間リソースを解放し、SetColorTexture で設定を変えた画像を読み直す（転送は次のフレームの先頭）。
    /// フレームの EndDraw（GPU 待機）より後に呼ぶこと
    /// </summary>
    void EndFrame();

    /// <summary>
    /// 表示用の描き先（UI・ImGui。ガンマのまま描く）で使う、ファイルの値のまま読む SRV。
    /// 色の画像でない・TextureManager の物でない番号は、そのまま返す
    /// </summary>
    uint32_t ToRawSRV(uint32_t srvIndex);

    /// <summary>
    /// 色の画像か（false ならノイズ・マスクなどのデータとして、リニアに直さずに読む）。
    /// プロジェクトの画像は resources/Json/TextureSettings.json で決まる（既定は true）
    /// </summary>
    bool IsColorTexture(const std::string& fileName) const;

    /// <summary>
    /// 色の画像かを決めて設定ファイルに書く。読み込み済みなら、このフレームの後で同じ SRV の番号のまま読み直す。
    /// EngineResources の画像は変えない
    /// </summary>
    void SetColorTexture(const std::string& fileName, bool isColor);

    //============================================================
    //Getter
    //============================================================
    // ファイル名で引く Getter は、読んでいなければその場で読む。読めなければ EngineResources の white.png を返す

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

    size_t GetLoadedTextureCount() const { return textureData_.size(); }

    bool IsLoaded(const std::string& fileName) const { return textureData_.contains(fileName); }

    const std::string& GetDirectoryPath() const { return directoryPath_; }

    /// <summary>
    /// LoadTexture に渡すキーから実ファイルのパスを求める（EngineResources/ 始まりはそのまま、それ以外はテクスチャディレクトリ基準）
    /// </summary>
    /// <param name="fileName">テクスチャのキー</param>
    /// <returns>作業ディレクトリ基準のファイルパス</returns>
    std::string GetFilePath(const std::string& fileName) const;

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

  private: //非公開関数

    /// <summary>
    /// 読み込み済みのデータ。無ければ読み、読めなければ white.png（それも無ければ nullptr）
    /// </summary>
    const TextureData* Acquire(const std::string& fileName);

    /// <summary>
    /// data.fileName の画像を設定どおりの形式で読み、リソースを作って転送を積む（SRV は作らない）
    /// </summary>
    /// <returns>読めなければ false（data は変えない）</returns>
    bool ReadTexture(TextureData& data);

    /// <summary>
    /// data の SRV（作ってあれば raw の SRV も）を、今のリソースで作り直す
    /// </summary>
    void CreateViews(TextureData& data);

    void CreateSRV(uint32_t srvIndex, const TextureData& data, DXGI_FORMAT format);

    /// <summary>
    /// COPY_DEST 状態のテクスチャリソースを生成する
    /// </summary>
    /// <param name="metaData">テクスチャのメタデータ（サイズ、フォーマット等）</param>
    GpuResource MakeTextureResource(const DirectX::TexMetadata& metaData);

    /// <summary>
    /// 中間バッファ経由の転送を積み、テクスチャを読み取り状態へ遷移させる
    /// </summary>
    /// <param name="texture">転送先のテクスチャリソース</param>
    /// <param name="mipImages">転送するミップマップ画像データ</param>
    /// <returns>アップロード用の中間バッファリソース（転送完了まで保持必要）</returns>
    [[nodiscard]]
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(GpuResource& texture, const DirectX::ScratchImage& mipImages);

  private: //メンバー変数

    DX12Basic* dx12_ = nullptr;  ///< DX12Basic クラスのインスタンス

    std::string directoryPath_;  ///< テクスチャ格納ディレクトリ

    std::unordered_map<std::string, TextureData> textureData_;     ///< テクスチャデータ配列
    std::unordered_map<uint32_t, TextureData*>   bySrv_;           ///< srvIndex から引く（unordered_map の要素は再ハッシュでも動かない）
    std::unordered_set<std::string>              failedFiles_;     ///< 読めなかったキー（毎フレーム読み直さない）
    std::unordered_set<std::string>              dataTextures_;    ///< 色の画像でないキー（TextureSettings.json の sRGB: false）
    std::vector<std::string>                     pendingReloads_;  ///< EndFrame で読み直すキー

  };

} // namespace Tako
