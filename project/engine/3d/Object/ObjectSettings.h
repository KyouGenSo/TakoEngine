#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Tako {

  class Material;

  /// <summary>
  /// 1 つの名前の描画設定。値の無い項目はコードで設定した値を使う
  /// </summary>
  struct ObjectOverrides {
    std::optional<std::string> material;        ///< resources/Materials のマテリアル名（全メッシュに使う）
    std::optional<uint32_t>    maskId;
    std::optional<int32_t>     sortingOrder;
    std::optional<bool>        castShadow;
    std::optional<bool>        transparent;
    std::shared_ptr<Material>  loadedMaterial;  ///< material を読んだもの（保存しない。読めなければ nullptr）
  };

  /// <summary>
  /// Object3d の名前ごとの描画設定。同じ名前の Object3d は描画時にこの設定を当てる。
  ///
  /// resources/Json/ObjectSettings.json
  ///   { "Boss": { "Material": "Hologram", "MaskId": 1, "SortingOrder": 0, "CastShadow": false, "Transparent": true } }
  /// </summary>
  class ObjectSettings {
  private: // シングルトン設定
    static std::unique_ptr<ObjectSettings> instance_;

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~ObjectSettings() = default;
    ObjectSettings(ObjectSettings&) = delete;
    ObjectSettings& operator=(ObjectSettings&) = delete;

    friend struct std::default_delete<ObjectSettings>;

  public:
    explicit ObjectSettings(Token) {}

  public: //メンバー関数
    static ObjectSettings* GetInstance();

    /// <summary>
    /// ファイルを読む（無ければ設定なし）。MaterialLibrary の初期化後に呼ぶ
    /// </summary>
    void Load();

    void Finalize();

    bool Save() const;

    /// <summary>
    /// Save が書く内容（JSON 文字列）。値の無い名前は書かない
    /// </summary>
    std::string Serialize() const;

    /// <summary>
    /// Serialize の内容で全ての設定を置き換える
    /// </summary>
    /// <returns>読めなければ false（設定は今のまま）</returns>
    bool Apply(const std::string& text);

    /// <summary>
    /// 名前の設定を書き換える（無ければ作る）。書き換えたら MarkChanged を呼ぶ
    /// </summary>
    ObjectOverrides& Edit(const std::string& name);

    /// <summary>
    /// マテリアルを読み直し、Object3d に次の描画で設定を当て直させる
    /// </summary>
    void MarkChanged();

    //=========================================
    //Getter
    //=========================================
    const ObjectOverrides* Find(std::string_view name) const;

    /// <summary>
    /// 設定のある名前（値の無い名前も含む）
    /// </summary>
    std::vector<std::string> GetNames() const;

    /// <summary>
    /// 設定が変わるたびに増える番号
    /// </summary>
    uint32_t GetVersion() const { return version_; }

  private: //メンバー変数
    std::map<std::string, ObjectOverrides, std::less<>> entries_;
    uint32_t                                             version_ = 1;  ///< Object3d は 0 から始めるので、最初の描画で必ず当てる
  };

} // namespace Tako
