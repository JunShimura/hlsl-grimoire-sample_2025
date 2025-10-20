#include "stdafx.h"
#include "system/system.h"

// step-1 ディレクションライト用の構造体を定義する
struct DirectionLight
{
    Vector3 direction; // ライトの方向
    float pad;       // パディング
    Vector3 color;    // ライトの色
};

///////////////////////////////////////////////////////////////////
// ウィンドウプログラムのメイン関数
///////////////////////////////////////////////////////////////////
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    // ゲームの初期化
    InitGame(hInstance, hPrevInstance, lpCmdLine, nCmdShow, TEXT("Game"));

    //////////////////////////////////////
    // ここから初期化を行うコードを記述する
    //////////////////////////////////////

    g_camera3D->SetPosition({ 0.0f, 0.0f, 100.0f });
    g_camera3D->SetTarget({ 0.0f, 0.0f, 0.0f });

    // step-2 ディレクションライトのデータを作成する
    DirectionLight directionLig;
    
    // ライトは斜め上からあたっている
    directionLig.direction = { -1.0f, -1.0f, -1.0f };
    directionLig.direction.Normalize();

	// ライトの色は灰色
	directionLig.color = { 1.0f, 1.0f, 1.0f };

    // step-3 モデルを初期化する
    // モデルを初期化するための情報を構築する
	ModelInitData modelInitData;
	modelInitData.m_tkmFilePath = "Assets/modelData/teapot.tkm";

	// 使用するシェーダーファイルパスを指定する
	modelInitData.m_fxFilePath = "Assets/shader/sample.fx";

    // ディレクションライトの情報をディスクリプターヒープに定数バッファとして
    // 登録するためにモデルも初期化情報として渡す
	modelInitData.m_expandConstantBuffer = &directionLig;
	modelInitData.m_expandConstantBufferSize = sizeof(directionLig);

	// 初期化情報を使ってモデルを初期化する
	Model model;
	model.Init(modelInitData);

    //////////////////////////////////////
    // 初期化を行うコードを書くのはここまで！！！
    //////////////////////////////////////
    auto& renderContext = g_graphicsEngine->GetRenderContext();

    // ここからゲームループ
    while (DispatchWindowMessage())
    {
        // レンダリング開始
        g_engine->BeginFrame();
        //////////////////////////////////////
        // ここから絵を描くコードを記述する
        //////////////////////////////////////

        // step-4 モデルをドローする
		model.Draw(renderContext);
        //////////////////////////////////////
        // 絵を描くコードを書くのはここまで！！！
        //////////////////////////////////////
        // レンダリング終了
        g_engine->EndFrame();
    }
    return 0;
}
