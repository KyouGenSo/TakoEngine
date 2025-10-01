#pragma once
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <chrono>
#include "imgui.h"

class DebugUIManager {
public:
    // ログタイプ
    enum class LogType {
        Info,
        Warning,
        Error
    };

    // ログ構造体
    struct LogEntry {
        LogType type;
        std::string message;
        std::string timestamp;
    };
    
    // ゲームオブジェクトデバッグ情報
    struct GameObjectDebugInfo {
        std::string name;                    // オブジェクト名
        std::function<void()> drawImGuiFunc; // DrawImGui関数
    };

private:
    // シングルトン
    static DebugUIManager* instance_;
    DebugUIManager() = default;
    ~DebugUIManager() = default;
    DebugUIManager(const DebugUIManager&) = delete;
    DebugUIManager& operator=(const DebugUIManager&) = delete;

public:
    // インスタンス取得
    static DebugUIManager* GetInstance();
    
    // 初期化・終了処理
    void Initialize();
    void Finalize();
    
    // 更新・描画
    void Update();
    void Draw();
    
    // コンソールログ機能
    void AddLog(const std::string& message, LogType type = LogType::Info);
    void ClearLogs();
    const std::vector<LogEntry>& GetLogs() const { return consoleLogs_; }
    
    // デバッグ情報登録システム
    void RegisterDebugInfo(const std::string& category, std::function<void()> callback);
    void UnregisterDebugInfo(const std::string& category);
    void ClearDebugInfo();
    
    // ゲームオブジェクト登録システム
    void RegisterGameObject(const std::string& name, std::function<void()> drawImGuiFunc);
    void UnregisterGameObject(const std::string& name);
    void ClearGameObjects();
    
    // ウィンドウ表示フラグ
    void SetWindowVisible(const std::string& windowName, bool visible);
    bool IsWindowVisible(const std::string& windowName) const;
    
    // Scene情報登録用の特別なインターフェース
    void SetSceneName(const std::string& sceneName) { currentSceneName_ = sceneName; }
    const std::string& GetSceneName() const { return currentSceneName_; }

private:
    // 各ウィンドウの描画
    void DrawMainMenuBar();
    void DrawSceneHierarchy();
    void DrawInspector();
    void DrawConsole();
    void DrawPerformance();
    void DrawGameViewport();
    void DrawEngineStatus();
    void DrawInputDebug();
    void DrawShadowSettings();
    void DrawCollisionDebug();
    
    // タイムスタンプ生成
    std::string GetCurrentTimestamp();

private:
    // コンソールログ
    std::vector<LogEntry> consoleLogs_;
    int maxConsoleLogs_ = 1000;
    bool showInfo_ = true;
    bool showWarning_ = true;
    bool showError_ = true;
    
    // ウィンドウ表示フラグ
    std::unordered_map<std::string, bool> windowVisibility_;
    
    // デバッグ情報コールバック
    std::unordered_map<std::string, std::function<void()>> debugInfoCallbacks_;
    
    // ゲームオブジェクト情報
    std::vector<GameObjectDebugInfo> gameObjects_;
    int selectedObjectIndex_ = -1;  // 選択されたオブジェクトのインデックス
    
    // パフォーマンス計測
    float fpsHistory_[100] = {0};
    int fpsHistoryIndex_ = 0;
    
    // 現在のシーン名
    std::string currentSceneName_ = "Unknown";
    
    // アプリケーション終了フラグへのポインタ
    bool* pEndFlag_ = nullptr;
    
public:
    // 終了フラグ設定
    void SetEndFlagPtr(bool* pEndFlag) { pEndFlag_ = pEndFlag; }

    // デバッグフラグ設定
    void SetDebugFlagPtr(bool* pIsDebug) { pIsDebug_ = pIsDebug; }

private:
    // デバッグカメラ有効フラグへのポインタ
    bool* pIsDebug_ = nullptr;
};