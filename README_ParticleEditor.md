# パーティクルエディター使用方法

## 概要
TakoEngineのパーティクルエディターは、GPUパーティクルシステムのエミッターをリアルタイムで編集、保存、管理するための統合ツールです。

## 起動方法

### 1. コード側での初期化
```cpp
// シーンクラスなどで初期化
void GameScene::Initialize() {
    // EmitterManagerの作成
    emitterManager_ = std::make_unique<EmitterManager>(GPUParticle::GetInstance());

    // DebugUIManagerへの登録
    DebugUIManager::GetInstance()->SetEmitterManager(emitterManager_.get());
}
```

### 2. エディターの表示
- デバッグビルドでImGuiのメニューバーから「Tools」→「Particle Editor」を選択

## 主な機能

### エミッター管理（Emittersタブ）
- **新規作成**: 3種類のエミッター（Sphere、Box、Triangle）を作成
- **リスト表示**: 現在アクティブなエミッターの一覧
- **削除・複製**: 選択したエミッターの削除や複製

### プロパティ編集（Propertiesタブ）
- **基本設定**: 位置、パーティクル数、発生頻度
- **範囲設定**: スケール、速度、寿命の範囲
- **色設定**: 開始色と終了色のグラデーション
- **型固有設定**: 各エミッタータイプ特有のパラメータ

### プリセット管理（Presetsタブ）
- **個別保存**: 単一エミッターをプリセットとして保存
- **個別読み込み**: プリセットから新しいエミッターを作成
- **シーン保存**: すべてのエミッターをまとめて保存
- **シーン読み込み**: 保存したシーンの復元

## 使用例

### 基本的なワークフロー
1. Particle Editorを開く
2. Emittersタブで新しいエミッターを作成
3. Propertiesタブで詳細を調整
4. Presetsタブでプリセットとして保存

### コードからの使用
```cpp
// プリセットの読み込み
emitterManager_->LoadPreset("Fire", "PlayerFireEffect");

// シーン全体の読み込み
emitterManager_->LoadScenePreset("SampleScene");

// エミッターの更新
emitterManager_->Update();
```

## Tips
- エフェクトの微調整は実行中にリアルタイムで確認可能
- プリセットを活用して、一貫性のあるエフェクトを維持
- グループ機能で関連するエミッターをまとめて管理