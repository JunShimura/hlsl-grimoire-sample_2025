#include "stdafx.h"
#include "system/system.h"
#include "Bitmap.h"
#include "sub.h"
#include <commdlg.h>


///////////////////////////////////////////////////////////////////
// ウィンドウプログラムのメイン関数
///////////////////////////////////////////////////////////////////
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    // ゲームの初期化
    InitGame(hInstance, hPrevInstance, lpCmdLine, nCmdShow, TEXT("Game"));

    //////////////////////////////////////
    //  ここから初期化を行うコードを記述する
    //////////////////////////////////////

    // step-1 画像データをメインメモリ上にロードする
    Bitmap imagebmp;
	imagebmp.Load("Assets/image/original2.bmp");

    // step-2 画像データをグラフィックスメモリに送るために構造化バッファーを作成
    StructuredBuffer inputImageBmpSB;
	inputImageBmpSB.Init(
        imagebmp.GetPixelSizeInBytes(), // 1画素のサイズ
        imagebmp.GetNumPixel(),         // ピクセル数
        imagebmp.GetImageAddress()      // 画像データのアドレス
    );

    // step-3 モノクロ化した画像を受け取るための読み書き可能な構造化バッファーを作成
	RWStructuredBuffer outputImageBmpRWSB;
	outputImageBmpRWSB.Init(
        imagebmp.GetPixelSizeInBytes(), // 1画素のサイズ
        imagebmp.GetNumPixel(),         // ピクセル数
        imagebmp.GetImageAddress()      // 画像データのアドレス
    );

    // step-4 入力データと出力データをディスクリプタヒープに登録する
    DescriptorHeap ds;
	ds.RegistShaderResource(0, inputImageBmpSB); // t0に入力用の構造化バッファーを登録
	ds.RegistUnorderAccessResource(0, outputImageBmpRWSB); // u0に出力用の構造化バッファーを登録
	ds.Commit();

    // コンピュートシェーダーのロード
    Shader cs;
    cs.LoadCS("Assets/shader/sample.fx", "CSMain");

    RootSignature rs;
    InitRootSignature(rs, cs);

    PipelineState pipelineState;
    InitPipelineState(rs, pipelineState, cs);

    //////////////////////////////////////
    // 初期化を行うコードを書くのはここまで！！！
    //////////////////////////////////////
    auto& renderContext = g_graphicsEngine->GetRenderContext();

    // ここからゲームループ
    while (DispatchWindowMessage())
    {
        // フレーム開始
        g_engine->BeginFrame();

        //////////////////////////////////////
        // ここからDirectComputeへのディスパッチ命令
        //////////////////////////////////////
        // step-5 ディスパッチコールを実行する
		renderContext.SetComputeRootSignature(rs);
		renderContext.SetPipelineState(pipelineState);
		renderContext.SetComputeDescriptorHeap(ds);

		// ピクセル数は1024×768=786432
        // 4つのスレッドを生成するコンピュートシェーダーなので
		// 786432÷4=196608個のスレッドグループを生成する必要がある
		renderContext.Dispatch((UINT)(1024*768/4) , 1, 1);

        // フレーム終了
        g_engine->EndFrame();

        // step-6 モノクロにした画像を保存
        imagebmp.Copy(outputImageBmpRWSB.GetResourceOnCPU());

        // 保存ファイル名をユーザーに入力してもらう（保存ダイアログを表示）
        wchar_t filePath[MAX_PATH] = L"Assets\\image\\monochrome_test.bmp";
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_hWnd;
        ofn.lpstrFile = filePath;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = L"Bitmap Files (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        ofn.lpstrDefExt = L"bmp";
        ofn.lpstrTitle = L"保存先を選択してください";

        if (GetSaveFileNameW(&ofn))
        {
            // wchar_t のファイルパスをマルチバイト文字列に変換して保存
            char mbFilePath[MAX_PATH] = {0};
            int ret = WideCharToMultiByte(CP_ACP, 0, filePath, -1, mbFilePath, MAX_PATH, NULL, NULL);
            if (ret > 0)
            {
                imagebmp.Save(mbFilePath);
                MessageBox(nullptr, L"保存しました", L"通知", MB_OK);
            }
            else
            {
                MessageBox(nullptr, L"ファイル名の変換に失敗しました", L"エラー", MB_OK);
            }
        }
        else
        {
            MessageBox(nullptr, L"保存がキャンセルされました", L"通知", MB_OK);
        }

        // デストロイ
        DestroyWindow(g_hWnd);
    }
    return 0;
}
