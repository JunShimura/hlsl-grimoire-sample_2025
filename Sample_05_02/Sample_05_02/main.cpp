#include "stdafx.h"
#include "system/system.h"

/// <summary>
/// ライト構造体
/// </summary>
struct Light
{
	// ディレクションライト用のメンバ
	Vector3 dirDirection;   // ライトの方向
	float pad0;
	Vector3 dirColor;       // ライトのカラー
	float pad1;

	// ライト構造体にポイントライト用のメンバ変数を追加
	Vector3 ptPosition;     // 位置
	float pad2;             // パディング
	Vector3 ptColor;        // カラー
	float ptRange;          // 影響範囲

	// step-1 ライト構造体にスポットライト用のメンバ変数を追加
	Vector3 spPosition;     // 位置
	float pad3;             // パディング
	Vector3 spColor;        // カラー
	float spRange;          // 影響範囲
	Vector3 spDirection;    // 射出方向
	float spAngle;          // 照射角度

	Vector3 eyePos;         // 視点の位置
	float pad4;

	Vector3 ambientLight;   // アンビエントライト
};
//////////////////////////////////////
//関数宣言
//////////////////////////////////////
void InitModel(Model& bgModel, Model& teapotModel, Model& lightModel, Light& light);
void InitDirectionLight(Light& light);
void InitPointLight(Light& light);
void InitAmbientLight(Light& light);

// HSV -> RGB 変換（h: 0..1）
static Vector3 HSVtoRGB(float h, float s, float v);

///////////////////////////////////////////////////////////////////
// ウィンドウプログラムのメイン関数
///////////////////////////////////////////////////////////////////
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	// ゲームの初期化
	InitGame(hInstance, hPrevInstance, lpCmdLine, nCmdShow, TEXT("Game"));

	g_camera3D->SetPosition({ 0.0f, 50.0f, 200.0f });
	g_camera3D->SetTarget({ 0.0f, 50.0f, 0.0f });

	//////////////////////////////////////
	// ここから初期化を行うコードを記述する
	//////////////////////////////////////

	// ライトのデータを作成する
	Light light;
	// ディレクションライトを初期化する
	InitDirectionLight(light);
	// ポイントライトを初期化する
	InitPointLight(light);
	// アンビエントライトを初期化する
	InitAmbientLight(light);

	// step-2 スポットライトのデータを初期化する
	// 初期座標は(0,50,0)にする
	light.spPosition = { 0.0f,50.0f,0.0f };
	// カラーを設定する（初期値）
	light.spColor = { 5.0f,5.0f,5.0f };
	// 初期方向
	light.spDirection = { 1.0f,-1.0f,1.0f };
	light.spDirection.Normalize();
	// 影響範囲を設定する
	light.spRange = 300.0f;
	// 照射角度を設定する（ラジアン）
	light.spAngle = Math::DegToRad(25.0f);

	// モデルを初期化する
	// モデルを初期化するための情報を構築する
	Model lightModel, bgModel, teapotModel;
	InitModel(bgModel, teapotModel, lightModel, light);

	//////////////////////////////////////
	// 初期化を行うコードを書くのはここまで！！！
	//////////////////////////////////////
	auto& renderContext = g_graphicsEngine->GetRenderContext();



	float f = 0.0f;
	Quaternion q = g_quatIdentity;
	// ここからゲームループ
	while (DispatchWindowMessage())
	{
		f += 0.0625f;

		// --- ここで f を使ってスポットライトカラーを色相環で回す ---
		// 色相を 0..1 に正規化（f をそのまま渡しても内部でラップします）
		float hue = f;
		// 彩度と明度は固定（彩度1、明度1）。出力をシーンに合わせてスケール（強さ）する。
		const float intensity = 5.0f;
		Vector3 col = HSVtoRGB(hue, 1.0f, 1.0f);
		// コンポーネントごとに代入（明るさを掛ける）
		light.spColor.x = col.x * intensity;
		light.spColor.y = col.y * intensity;
		light.spColor.z = col.z * intensity;
		// ------------------------------------------------------------

		// レンダリング開始
		g_engine->BeginFrame();
		//////////////////////////////////////
		// ここから絵を描くコードを記述する
		//////////////////////////////////////

		// step-3 コントローラー左スティックでスポットライトを移動させる
		// 左のアナログスティックで動かす
		light.spPosition.x -= g_pad[0]->GetLStickXF();
		if (g_pad[0]->IsPress(enButtonB))
		{
			// Bボタンが一緒に押されていたらY軸方向に動かす
			light.spPosition.y += g_pad[0]->GetLStickYF();
		}
		else
		{
			// Bボタンが押されていなかったらZ軸方向に動かす
			light.spPosition.z -= g_pad[0]->GetLStickYF();
		}

		// step-4 コントローラー右スティックでスポットライトを回転させる
		Quaternion qRotY;
		qRotY.SetRotationY(g_pad[0]->GetRStickXF() * 0.01f + sin(f/2.0f)/2.0f);

		// ライトの方向ベクトルをQuaternionで回転させる
		qRotY.Apply(light.spDirection);

		// X軸回りの回転クォータニオンを求める
		Vector3 rotAxis;
		rotAxis.Cross(g_vec3AxisY, light.spDirection);
		Quaternion qRotX;
		qRotX.SetRotation(rotAxis, g_pad[0]->GetRStickYF() * 0.01f);

		// 計算したクォータニオンでライトの方向を回す
		qRotX.Apply(light.spDirection);

		// スポットライトモデルの回転クォータニオンを求める
		Quaternion qRot;
		qRot.SetRotation(g_vec3Back, light.spDirection);

		// スポットライトモデルのワールド行列を更新する
		lightModel.UpdateWorldMatrix(light.spPosition, qRot, g_vec3One);

		// 背景モデルをドロー
		bgModel.Draw(renderContext);

		// スポットライトモデルをドロー
		lightModel.Draw(renderContext);
		
		//おまけ： 回るティーポットモデルをドロー
		q.SetRotationDegY(f * 10);
		teapotModel.UpdateWorldMatrix(Vector3(0, 15.f, 0), q, Vector3(1, 1, 1));
		teapotModel.Draw(renderContext);


		//////////////////////////////////////
		// 絵を描くコードを書くのはここまで！！！
		//////////////////////////////////////
		// レンダリング終了
		g_engine->EndFrame();
	}
	return 0;
}

/// <summary>
/// モデルを初期化
/// </summary>
/// <param name="bgModel"></param>
/// <param name="teapotModel"></param>
/// <param name="lightModel"></param>
/// <param name="light"></param>
void InitModel(Model& bgModel, Model& teapotModel, Model& lightModel, Light& light)
{
	ModelInitData bgModelInitData;
	bgModelInitData.m_tkmFilePath = "Assets/modelData/bg.tkm";

	// 使用するシェーダーファイルパスを設定する
	bgModelInitData.m_fxFilePath = "Assets/shader/sample.fx";

	// ディレクションライトの情報を定数バッファとしてディスクリプタヒープに登録するために
	// モデルの初期化情報として渡す
	bgModelInitData.m_expandConstantBuffer = &light;
	bgModelInitData.m_expandConstantBufferSize = sizeof(light);

	// 初期化情報を使ってモデルを初期化する
	bgModel.Init(bgModelInitData);

	ModelInitData teapotModelInitData;
	teapotModelInitData.m_tkmFilePath = "Assets/modelData/teapot.tkm";

	// 使用するシェーダーファイルパスを設定する
	teapotModelInitData.m_fxFilePath = "Assets/shader/sample.fx";

	// ディレクションライトの情報を定数バッファとしてディスクリプタヒープに登録するために
	// モデルの初期化情報として渡す
	teapotModelInitData.m_expandConstantBuffer = &light;
	teapotModelInitData.m_expandConstantBufferSize = sizeof(light);

	// 初期化情報を使ってモデルを初期化する
	teapotModel.Init(teapotModelInitData);

	teapotModel.UpdateWorldMatrix(
		{ 0.0f, 20.0f, 0.0f },
		g_quatIdentity,
		g_vec3One
	);

	ModelInitData lightModelInitData;
	lightModelInitData.m_tkmFilePath = "Assets/modelData/light.tkm";

	// 使用するシェーダーファイルパスを設定する
	lightModelInitData.m_fxFilePath = "Assets/shader/other/light.fx";
	lightModelInitData.m_expandConstantBuffer = &light;
	lightModelInitData.m_expandConstantBufferSize = sizeof(light);

	// 初期化情報を使ってモデルを初期化する
	lightModel.Init(lightModelInitData);
}

void InitDirectionLight(Light& light)
{
	// ライトは右側から当たっている
	light.dirDirection.x = 1.0f;
	light.dirDirection.y = -1.0f;
	light.dirDirection.z = -1.0f;
	light.dirDirection.Normalize();

	// ライトのカラーは白
	light.dirColor.x = 0.0625f;
	light.dirColor.y = 0.0625f;
	light.dirColor.z = 0.0625f;

	// 視点の位置を設定する
	light.eyePos = g_camera3D->GetPosition();
}

void InitPointLight(Light& light)
{
	// ポイントライトの座標を設定する
	light.ptPosition.x = 0.0f;
	light.ptPosition.y = 50.0f;
	light.ptPosition.z = 50.0f;

	// ポイントライトのカラーを設定する
	light.ptColor.x = 0.0f;
	light.ptColor.y = 0.0f;
	light.ptColor.z = 0.0f;

	// ポイントライトの影響範囲を設定する
	light.ptRange = 100.0f;
}

void InitAmbientLight(Light& light)
{
	// アンビエントライト
	light.ambientLight.x = 0.125f;
	light.ambientLight.y = 0.125f;
	light.ambientLight.z = 0.125f;
}

/// <summary>
/// HSV を RGB に変換する（h in [0,1]）
/// </summary>
static Vector3 HSVtoRGB(float h, float s, float v)
{
	// h を 0..1 にラップ
	h = h - floorf(h);
	if (h < 0.0f) h += 1.0f;

	float hh = h * 6.0f;
	int i = (int)hh;
	float f = hh - i;
	float p = v * (1.0f - s);
	float q = v * (1.0f - s * f);
	float t = v * (1.0f - s * (1.0f - f));

	float r = 0.0f, g = 0.0f, b = 0.0f;
	switch (i)
	{
	case 0: r = v; g = t; b = p; break;
	case 1: r = q; g = v; b = p; break;
	case 2: r = p; g = v; b = t; break;
	case 3: r = p; g = q; b = v; break;
	case 4: r = t; g = p; b = v; break;
	case 5: r = v; g = p; b = q; break;
	default: r = v; g = p; b = q; break;
	}

	return Vector3{ r, g, b };
}
