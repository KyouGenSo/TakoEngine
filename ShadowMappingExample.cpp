// Shadow Mapping使用例
// このファイルは実装例を示すためのもので、実際のプロジェクトに組み込む際の参考用です

#include "Object3dBasic.h"
#include "Object3d.h"

void ShadowMappingRenderExample()
{
    // Object3dBasicのインスタンス取得
    Object3dBasic* object3dBasic = Object3dBasic::GetInstance();
    
    // ライトの設定
    Vector3 lightDirection = {0.5f, -1.0f, 0.3f}; // ライトの方向
    Vector4 lightColor = {1.0f, 1.0f, 1.0f, 1.0f}; // ライトの色
    Vector3 lightPosition = {0.0f, 10.0f, -5.0f}; // シャドウマップ用のライト位置
    
    // DirectionalLightの設定
    object3dBasic->SetDirectionalLight(lightDirection, lightColor, 0, 1.0f);
    object3dBasic->SetDirectionalLightPosition(lightPosition);
    object3dBasic->SetDirectionalLightShadowDistance(20.0f); // シャドウの範囲
    
    // シャドウを有効化
    object3dBasic->EnableShadow(true);
    
    // Object3dBasicの更新（ライトのシャドウ行列を計算）
    object3dBasic->Update();
    
    // レンダリングフロー：
    
    // 1. シャドウマップ生成パス
    object3dBasic->BeginShadowMapRender();
    {
        // シャドウキャスターのオブジェクトをここでレンダリング
        // 例：地面、キューブ、その他の3Dオブジェクト
        // object->Draw(); // 各オブジェクトのDraw()を呼び出し
    }
    object3dBasic->EndShadowMapRender();
    
    // 2. メインレンダリングパス（シャドウ適用）
    object3dBasic->SetCommonRenderSetting();
    {
        // 通常のオブジェクトをレンダリング（シャドウが適用される）
        // 例：地面、キューブ、その他の3Dオブジェクト
        // object->Draw(); // 各オブジェクトのDraw()を呼び出し
    }
}

// 基本的なシャドウマップセットアップ関数（自動位置計算版）
void SetupBasicShadowMapping()
{
    Object3dBasic* object3dBasic = Object3dBasic::GetInstance();
    
    // 基本的なライト設定（位置は自動計算）
    object3dBasic->SetDirectionalLight(
        {0.5f, -1.0f, 0.3f},    // 方向
        {1.0f, 1.0f, 1.0f, 1.0f}, // 色
        0,                       // ライトタイプ（Lambertian）
        1.0f                     // 強度
    );
    
    // シャドウの影響範囲設定（オプション、デフォルトは30.0f）
    object3dBasic->SetDirectionalLightShadowDistance(25.0f);
    
    // シーン中心の設定（オプション、デフォルトは(0,0,0)）
    object3dBasic->SetSceneCenter({0.0f, 0.0f, 0.0f});
    
    // シャドウマップを有効化
    object3dBasic->EnableShadow(true);
    
    // 自動位置更新を有効（デフォルトでtrueなので省略可能）
    // object3dBasic->SetAutoUpdatePosition(true);
}

// 手動位置設定版（従来の方法）
void SetupManualShadowMapping()
{
    Object3dBasic* object3dBasic = Object3dBasic::GetInstance();
    
    // 自動位置更新を無効化
    object3dBasic->SetAutoUpdatePosition(false);
    
    // ライト設定
    object3dBasic->SetDirectionalLight(
        {0.5f, -1.0f, 0.3f},
        {1.0f, 1.0f, 1.0f, 1.0f}, 
        0, 1.0f
    );
    
    // 手動でライト位置を設定
    object3dBasic->SetDirectionalLightPosition({0.0f, 10.0f, -5.0f});
    
    object3dBasic->SetDirectionalLightShadowDistance(25.0f);
    object3dBasic->EnableShadow(true);
}

// デバッグ用：シャドウを無効化する関数
void DisableShadowMapping()
{
    Object3dBasic* object3dBasic = Object3dBasic::GetInstance();
    object3dBasic->EnableShadow(false);
}

/*
使用方法（新版 - 自動位置計算）：

1. 初期化時にSetupBasicShadowMapping()を呼び出し（位置の手動設定不要！）
2. 各フレームのレンダリングでShadowMappingRenderExample()のパターンを使用
3. デバッグ時にはDisableShadowMapping()でシャドウを無効化可能

簡単な使用例：
```cpp
// 最短セットアップ（これだけでシャドウマップが動作）
Object3dBasic* obj3d = Object3dBasic::GetInstance();
obj3d->SetDirectionalLight({0.5f, -1.0f, 0.3f}, {1.0f, 1.0f, 1.0f, 1.0f}, 0, 1.0f);
obj3d->EnableShadow(true);
// SetDirectionalLightPosition()の呼び出し不要！

// オプション設定
obj3d->SetDirectionalLightShadowDistance(20.0f);  // シャドウ範囲変更
obj3d->SetSceneCenter({5.0f, 0.0f, 5.0f});       // シーン中心変更
```

従来の手動設定も可能：
```cpp
obj3d->SetAutoUpdatePosition(false);  // 自動更新を無効化
obj3d->SetDirectionalLightPosition({0.0f, 10.0f, -5.0f});  // 手動位置設定
```

注意事項：
- シャドウキャスター（影を落とすオブジェクト）とシャドウレシーバー（影を受けるオブジェクト）の
  両方で同じオブジェクトをレンダリングする必要があります
- ライトの方向を変更すると、位置も自動的に更新されます
- パフォーマンスを重視する場合は、シャドウキャスターの数を制限してください
*/