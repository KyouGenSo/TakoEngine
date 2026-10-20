#pragma once

#ifdef _DEBUG

#include "CameraAnimation.h"
#include "CameraKeyframe.h"
#include <vector>
#include "ImGuiManager.h"

namespace Tako {

  /// <summary>
  /// 補間カーブを視覚編集するエディター
  /// </summary>
  class CameraAnimationCurveEditor {
  public: //構造体
    enum class CurveType {
      POSITION_X,
      POSITION_Y,
      POSITION_Z,
      ROTATION_X,
      ROTATION_Y,
      ROTATION_Z,
      FOV,
      COUNT
    };

    enum class HandleType {
      NONE,
      LEFT,
      RIGHT,
      BOTH
    };

  public: //メンバー関数
    CameraAnimationCurveEditor();
    ~CameraAnimationCurveEditor();

    void Initialize(CameraAnimation* animation);
    void Draw();

    //==========================================================================
    //Setter
    //==========================================================================
    void SetActiveCurve(CurveType type) { activeCurve_ = type; }
    void SetCurveVisible(CurveType type, bool visible);
    void SetGridSnapEnabled(bool enable) { enableGridSnap_ = enable; }

  private: //非公開関数
    void DrawGraphArea();
    void DrawGrid();
    void DrawAxes();
    void DrawCurve(CurveType curveType);
    void DrawKeyPoint(int index, float x, float y, bool isSelected);
    /// <summary>
    /// 選択中キーフレームの区間が CUBIC_BEZIER のとき、タイミング曲線の制御点をドラッグ用ハンドルとして描画
    /// </summary>
    void DrawBezierHandles(CurveType curveType);
    void HandleMouseInput();
    void DrawCurveProperties();
    void DrawEasingPresets();

    /// <summary>
    /// 時間・値をグラフ上のスクリーン座標に変換
    /// </summary>
    /// <param name="time">時刻（秒）</param>
    /// <param name="value">カーブ値（成分の単位）</param>
    /// <returns>グラフ領域内のスクリーン座標（ピクセル）</returns>
    ImVec2 ValueToGraph(float time, float value) const;

    /// <summary>
    /// グラフ上のスクリーン座標を時間・値に変換
    /// </summary>
    /// <param name="pos">スクリーン座標（ピクセル）</param>
    /// <param name="time">出力。時刻（秒）</param>
    /// <param name="value">出力。カーブ値（成分の単位）</param>
    void GraphToValue(const ImVec2& pos, float& time, float& value) const;

    /// <summary>
    /// キーフレームから curveType に対応する成分を取得
    /// </summary>
    /// <param name="kf">対象キーフレーム</param>
    /// <param name="type">取り出す成分（位置/回転の各軸または FOV）</param>
    /// <returns>該当成分の値。type が不正なら0</returns>
    float GetCurveValue(const CameraKeyframe& kf, CurveType type) const;

    /// <summary>
    /// キーフレームの curveType に対応する成分へ値を設定
    /// </summary>
    /// <param name="kf">書き込み先キーフレーム</param>
    /// <param name="type">設定する成分（位置/回転の各軸または FOV）</param>
    /// <param name="value">設定値</param>
    void SetCurveValue(CameraKeyframe& kf, CurveType type, float value);

    /// <summary>
    /// 時間・値をグリッド間隔へ丸める
    /// </summary>
    /// <param name="time">入出力。時刻（秒）。X方向グリッド間隔へ丸める</param>
    /// <param name="value">入出力。カーブ値。Y方向グリッド間隔へ丸める</param>
    void SnapToGrid(float& time, float& value) const;

    /// <summary>
    /// 選んだキーフレームの添字
    /// </summary>
    /// <returns>未選択か、もう無ければ -1</returns>
    int SelectedIndex() const { return animation_->FindKeyframeIndex(selectedKeyId_); }

  private: //メンバー変数
    CameraAnimation* animation_ = nullptr;

    //グラフ設定
    ImVec2 graphPos_;
    ImVec2 graphSize_     = ImVec2(600, 300);
    float  timeRange_     = 10.0f;
    float  valueRangeMin_ = -10.0f;
    float  valueRangeMax_ = 10.0f;
    float  zoomX_         = 1.0f;
    float  zoomY_         = 1.0f;
    float  panX_          = 0.0f;
    float  panY_          = 0.0f;

    //カーブ設定
    CurveType activeCurve_                                      = CurveType::POSITION_X;
    bool      curveVisible_[static_cast<int>(CurveType::COUNT)];
    ImU32     curveColors_[static_cast<int>(CurveType::COUNT)];

    //選択状態
    uint32_t   selectedKeyId_       = 0;                 ///< 選んだキーフレームの id。0 は未選択
    int        selectedEasingIndex_ = 0;
    HandleType selectedHandle_      = HandleType::NONE;
    bool       isDragging_          = false;
    ImVec2     dragStartPos_;
    float      dragStartTime_;
    float      dragStartValue_;

    //グリッドスナップ
    bool  enableGridSnap_    = false;
    float gridSnapIntervalX_ = 0.1f;   ///< 秒
    float gridSnapIntervalY_ = 1.0f;

    //表示設定
    bool showGrid_        = true;
    bool showAxes_        = true;
    bool showTangents_    = true;  ///< ベジェ制御点ハンドルの表示
    bool showValues_      = true;
    int  curveResolution_ = 50;    ///< 1区間あたりの分割数
  };

} // namespace Tako

#endif // _DEBUG
