#pragma once
#include "GpuResource.h"
#include "Vector2.h"
#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>

struct stbtt_fontinfo;

namespace Tako {

  /// <summary>
  /// フォントファイル 1 つと、使った字だけを距離場（SDF）にして足していくアトラス
  /// </summary>
  class TAKO_API Font {
  public: //定数
    static constexpr const char* kDirectory  = "resources/Fonts/";
    static constexpr const char* kDefaultKey = "EngineResources/Fonts/NotoSansJP-Regular.otf";  ///< キーが空、または読めないときに使う
    static constexpr float       kBakeSize   = 48.0f;                                           ///< アトラスに焼く字の大きさ（em の px）
    static constexpr int         kSpread     = 6;                                               ///< 輪郭の外と内に持つ距離（アトラスの px）。縁取りの太さの上限になる
    static constexpr int         kAtlasSize  = 2048;

  private: //定数
    static constexpr int kMaxNewGlyphsPerFrame = 16;  ///< 1 フレームに距離場を作る字数（全フォントの合計）。残りは次のフレームに回す。1 字は Release で平均 0.14ms、Debug で 1.1ms（130 字で計測）

  public: //構造体
    /// <summary>
    /// 字 1 つの、アトラス上の位置と字組みの値。長さは em を 1 とした値
    /// </summary>
    struct Glyph {
      int     index   = 0;     ///< フォント内のグリフ番号（カーニングを引くのに使う）
      Vector2 uvMin   = {};
      Vector2 uvMax   = {};
      Vector2 offset  = {};    ///< ベースライン上のペン位置から四角の左上まで（下が正）
      Vector2 size    = {};    ///< 四角の大きさ。空白など形の無い字は 0
      float   advance = 0.0f;
    };

  public: //メンバー関数
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    /// <summary>
    /// キーのフォント。初めてのキーはここで読み、読めなければ既定のフォントを返す
    /// </summary>
    /// <param name="key">kDirectory からのパス（EngineResources/ 始まりはそのまま）。空なら既定のフォント</param>
    /// <returns>既定のフォントも読めなければ nullptr</returns>
    static Font* Get(const std::string& key);

    /// <summary>
    /// 1 フレームに作る字数の上限を戻す
    /// </summary>
    static void BeginFrame();

    /// <summary>
    /// 読み込んだフォントを全部解放する
    /// </summary>
    static void Finalize();

    /// <summary>
    /// 字を引く。初めての字は距離場を作ってアトラスへ足す
    /// </summary>
    /// <returns>このフレームの上限を超えていて作れなかったら nullptr（次のフレームに引き直す）</returns>
    const Glyph* FindGlyph(char32_t codepoint);

    /// <summary>
    /// アトラスに足した字を GPU へ送る（足していなければ何もしない）。この字を描く前に呼ぶ
    /// </summary>
    void Upload();

    //===========================================================
    //Getter
    //===========================================================
    /// <summary>
    /// left の次に right を置くときに、送りへ足す値（em を 1 とした値）
    /// </summary>
    float GetKerning(const Glyph& left, const Glyph& right) const;

    /// <summary>
    /// SDF の値 0〜1 が表す距離（アトラスの UV）
    /// </summary>
    static float GetUnitRange();

    uint32_t GetSrvIndex() const { return srvIndex_; }
    float GetAscent() const { return ascent_; }
    float GetDescent() const { return descent_; }
    float GetLineGap() const { return lineGap_; }

  private: //非公開関数
    Font();

    /// <summary>
    /// ファイルを読み、アトラスのテクスチャを作る
    /// </summary>
    /// <returns>読めなければ false</returns>
    bool Load(const std::string& key);

  private: //メンバー変数
    //フォント
    std::vector<unsigned char>      fileData_;         ///< info_ が指し続けるので、フォントと同じ間持つ
    std::unique_ptr<stbtt_fontinfo> info_;
    float                           emScale_  = 0.0f;  ///< フォントの単位 → em を 1 とした値
    float                           ascent_   = 0.0f;  ///< ベースラインから上（em を 1 とした値、正）
    float                           descent_  = 0.0f;  ///< ベースラインから下（em を 1 とした値、負）
    float                           lineGap_  = 0.0f;

    //字
    std::unordered_map<char32_t, Glyph> glyphs_;

    //アトラス
    GpuResource                            atlas_;
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer_;              ///< アトラス全面の写し。字はここへ書いてからまとめて送る
    unsigned char*                         pixels_         = nullptr;  ///< uploadBuffer_ の常駐マップ先
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT     footprint_      = {};
    uint32_t                               srvIndex_       = 0;
    bool                                   isUploadNeeded_ = false;
    bool                                   isFull_         = false;

    //アトラスの次に置く位置（棚に左から並べ、埋まったら次の棚）
    int penX_      = 1;
    int penY_      = 1;
    int rowHeight_ = 0;
  };

} // namespace Tako
