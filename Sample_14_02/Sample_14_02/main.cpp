#include "stdafx.h"
#include "system/system.h"
#include "RenderingEngine.h"
#include "ModelRender.h"

// 関数宣言
void InitRootSignature(RootSignature& rs);

///////////////////////////////////////////////////////////////////
// ウィンドウプログラムのメイン関数
///////////////////////////////////////////////////////////////////
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine, int nCmdShow)
{
    // ゲームの初期化
    InitGame(hInstance, hPrevInstance, lpCmdLine, nCmdShow, TEXT("Game"));

    //////////////////////////////////////
    // ここから初期化を行うコードを記述する
    //////////////////////////////////////
    // ルートシグネチャを作成
    RootSignature rootSignature;
    InitRootSignature(rootSignature);

    //レンダリングパイプラインを初期化
    myRenderer::RenderingEngine renderingEngine;
    renderingEngine.Init();

    // 背景モデルのレンダラーを初期化
    myRenderer::ModelRender bgModelRender;
    bgModelRender.InitDeferredRendering(renderingEngine, "Assets/modelData/bg/bg.tkm", true);

    // step-1 ティーポットモデルの描画処理を初期化
    myRenderer::ModelInitDataFR modelInitData;
    modelInitData.m_tkmFilePath = "Assets/modelData/teapot.tkm";
    modelInitData.m_fxFilePath = "Assets/shader/sample.fx";

    //【注目】メインレンダリングターゲットのスナップショットテクスチャを拡張SRVに指定する
    modelInitData.m_expandShaderResoruceView[0] = &renderingEngine.GetMainRenderTargetSnapshotDrawnOpacity();

    // 拡張定数バッファ用の構造体（16バイトアライメント）
    struct AlphaParam
    {
        float alpha;
        float padding[3];
    };
    AlphaParam alphaParam;
    alphaParam.alpha = 1.0f;  // 初期値

    // 拡張定数バッファを設定
    modelInitData.m_expandConstantBuffer = &alphaParam;
    modelInitData.m_expandConstantBufferSize = sizeof(AlphaParam);

    myRenderer::ModelRender teapotModelRender;

    //フォワードレンダリングの描画パスで実行されるように初期化する
    teapotModelRender.InitForwardRendering(renderingEngine, modelInitData);
    teapotModelRender.SetShadowCasterFlag(true);

    // ティーポットの回転角度を初期化（ラジアン）
    float teapotRotationAngle = 0.0f;

    // 透明度アニメーション用の変数
    float alphaTimer = 0.0f;  // 経過時間（秒）
    const float alphaCycleDuration = 10.0f;  // 1サイクル10秒（5秒で透明→不透明、5秒で不透明→透明）

    //////////////////////////////////////
    // 初期化を行うコードを書くのはここまで！！！
    //////////////////////////////////////
    auto& renderContext = g_graphicsEngine->GetRenderContext();

    // ここからゲームループ
    while (DispatchWindowMessage())
    {
        // レンダリング開始
        g_engine->BeginFrame();
        g_camera3D->MoveForward(g_pad[0]->GetLStickYF());
        g_camera3D->MoveRight(g_pad[0]->GetLStickXF());
        g_camera3D->MoveUp(g_pad[0]->GetRStickYF());

        //////////////////////////////////////
        // ここから絵を描くコードを記述する
        //////////////////////////////////////

        bgModelRender.Draw();

        // step-2 ティーポットモデルを描画
        // 毎秒0.25回転（4秒で1回転）させる
        // 60FPS想定: 1フレームあたり π/120 ラジアン回転
        // 計算式: (2π × 0.25回転/秒) / 60フレーム/秒 = π/120 ラジアン/フレーム
        teapotRotationAngle += Math::PI / 120.0f;

        // 回転クォータニオンを作成
        Quaternion rotY;
        rotY.SetRotationY(teapotRotationAngle);

        // ティーポットのワールド行列を更新
        teapotModelRender.UpdateWorldMatrix({ 0.0f, 20.0f, 0.0f }, rotY, g_vec3One);

        // 透明度アニメーション（10秒周期）
        alphaTimer += 1.0f / 60.0f;  // 60FPS想定で時間を進める
        if (alphaTimer >= alphaCycleDuration)
        {
            alphaTimer = 0.0f;
        }

        // 0～5秒: 透明(0.0)→不透明(1.0)
        // 5～10秒: 不透明(1.0)→透明(0.0)
        float alpha;
        if (alphaTimer < alphaCycleDuration / 2.0f)
        {
            // 0～5秒: 0.0 → 1.0
            alpha = alphaTimer / (alphaCycleDuration / 2.0f);
        }
        else
        {
            // 5～10秒: 1.0 → 0.0
            alpha = 1.0f - (alphaTimer - alphaCycleDuration / 2.0f) / (alphaCycleDuration / 2.0f);
        }

        // 拡張定数バッファのアルファ値を更新
        alphaParam.alpha = alpha;

        teapotModelRender.Draw();

        // レンダリングパイプラインを実行
        renderingEngine.Execute(renderContext);

        /////////////////////////////////////////
        // 絵を描くコードを書くのはここまで！！！
        //////////////////////////////////////
        // レンダリング終了
        g_engine->EndFrame();
    }

    return 0;
}

// ルートシグネチャの初期化
void InitRootSignature(RootSignature& rs)
{
    rs.Init(D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP);
}
