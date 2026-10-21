#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "MaterialPropertyLayout.h"

namespace Tako {

  class DX12Basic;
  class Material;
  class ShaderAsset;

  /// <summary>
  /// データファイルから作るシェーダーとマテリアルの置き場。C++ を書かずにシェーダーを追加して描画物に使えるようにする。
  ///
  /// EngineResources/shaders/*.shader.json と resources/Shaders/*.shader.json（起動時に全て読む）
  ///   { "Name": "Hologram", "Fallback": "Lit", "Queue": 3000,
  ///     "Forward": { "VS": "Object3D.VS.hlsl", "PS": "resources/Shaders/Hologram.PS.hlsl", "Blend": "Add", "Cull": "None", "DepthTest": true, "DepthWrite": false },
  ///     "Properties": { "speed": [0, 10] } }
  ///   Forward があれば描画物のシェーダー（定数は gMaterial、書いていないパスは Fallback から借りる）、
  ///   "PostEffect": { "PS": "...", "UsesDepth": false } があればポストエフェクト（定数は gParam）として登録する。
  ///   "Defaults": { "color": [1, 1, 1, 1] } は値の初期値。Forward も PostEffect も無ければ、登録済みの同名シェーダーに範囲と初期値だけを足す。
  ///   ファイル名だけのシェーダーはエンジンのもの、パス付きは作業ディレクトリ基準
  ///
  /// resources/Materials/{name}.mat.json
  ///   { "Shader": "Hologram", "Queue": 3000, "Blend": "Add", "Cull": "None", "DepthTest": true, "DepthWrite": false,
  ///     "Values": { "color": [0, 1, 1, 0.5] }, "Textures": { "gTexture": "uvChecker.png" } }
  ///   Shader 以外は省略でき、省略した描画設定はシェーダーの既定、テクスチャは白になる
  /// </summary>
  class MaterialLibrary {
  private: // シングルトン設定
    static std::unique_ptr<MaterialLibrary> instance_;

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~MaterialLibrary() = default;
    MaterialLibrary(MaterialLibrary&) = delete;
    MaterialLibrary& operator=(MaterialLibrary&) = delete;

    friend struct std::default_delete<MaterialLibrary>;

  public:
    explicit MaterialLibrary(Token) {}

  private: //構造体
    struct Asset {
      std::shared_ptr<Material>                     material;
      std::shared_ptr<const MaterialPropertyLayout> layout;    ///< 値を書いたときのレイアウト（ホットリロードで変わったら名前で移す）
      std::map<std::string, std::string>            textures;  ///< テクスチャ名 → ファイル（保存用）
    };

    struct ShaderInfo {
      std::map<std::string, std::pair<float, float>> ranges;       ///< .shader.json の Properties
      nlohmann::json                                 defaults;     ///< .shader.json の Defaults（値の初期値）
      std::shared_ptr<const MaterialPropertyLayout>  layout;
      uint32_t                                       version = 0;  ///< layout を作ったときの PS のバージョン
    };

  public: //メンバー関数
    static MaterialLibrary* GetInstance();

    /// <summary>
    /// エンジンとゲームの .shader.json を読み、シェーダーとポストエフェクトを登録する。Renderer と PostEffectManager の初期化後に呼ぶ
    /// </summary>
    void Initialize(DX12Basic* dx12);

    void Finalize();

    /// <summary>
    /// シェーダーのホットリロードで定数の並びが変わったマテリアルへ、値を名前で移す（Debug）
    /// </summary>
    void Update();

    /// <summary>
    /// resources/Materials/{name}.mat.json を読む。読み込み済みなら同じマテリアルを返す（複数の描画物で共有する）
    /// </summary>
    /// <returns>読めなければ nullptr</returns>
    std::shared_ptr<Material> Load(const std::string& name);

    /// <summary>
    /// shaderName の既定値で新しいマテリアルを作る（保存するまでファイルは無い）
    /// </summary>
    /// <returns>シェーダーが無ければ nullptr</returns>
    std::shared_ptr<Material> Create(const std::string& name, const std::string& shaderName);

    bool Save(const std::string& name) const;

    /// <summary>
    /// Save が書く内容（JSON 文字列）
    /// </summary>
    std::string Serialize(const std::string& name) const;

    /// <summary>
    /// .mat.json の内容をマテリアルに当てはめる（シェーダーが変われば値は名前で引き継ぐ）
    /// </summary>
    bool Apply(const std::string& name, const std::string& text);

    /// <summary>
    /// マテリアルの値の編集 UI を描く（Debug のみ）
    /// </summary>
    /// <returns>値を変えたら true</returns>
    bool DrawValues(const std::string& name);

    /// <summary>
    /// シェーダーの Forward パスの PS から作る値の並び
    /// </summary>
    std::shared_ptr<const MaterialPropertyLayout> GetLayout(const ShaderAsset& shader);

    //=========================================
    //Getter
    //=========================================
    /// <summary>
    /// resources/Materials にあるマテリアルの名前
    /// </summary>
    std::vector<std::string> GetMaterialFileNames() const;

    /// <summary>
    /// 読み込み済み（または作成済み）のマテリアル（無ければ nullptr）
    /// </summary>
    std::shared_ptr<Material> Find(const std::string& name) const;

  private: //非公開関数
    void LoadShaderFile(const std::string& path);

    /// <summary>
    /// 定数を 0 → シェーダーの Defaults → previous の順で入れ直す（asset.layout の大きさで確保し直す）
    /// </summary>
    void ResetValues(Asset& asset, const nlohmann::json& previous);

    /// <summary>
    /// レイアウトに無いテクスチャを外し、レイアウトにあって未設定のものを白にする
    /// </summary>
    void FitTextures(Asset& asset) const;

  private: //メンバー変数
    DX12Basic*                        dx12_    = nullptr;
    std::map<std::string, Asset>      assets_;             ///< マテリアル名ごと
    std::map<std::string, ShaderInfo> shaders_;            ///< シェーダー名ごと
  };

} // namespace Tako
