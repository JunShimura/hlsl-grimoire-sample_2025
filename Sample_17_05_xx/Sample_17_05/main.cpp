#include "stdafx.h"
#include "system/system.h"
#include <time.h>
#include <Windows.h>
#include <sstream>
#include <fstream>
#include "tkFile/TkmFile.h"

// Forward declaration
static std::string BuildTkmMaterialCSV(const std::string& tkmFilePath);
static void OutputTkmMaterialInfo(const std::string& tkmFilePath);
static void CreateGlassCubeTkmFile();

// 指定した .tkm ファイルを解析して、マテリアル一覧を返す
static std::string BuildTkmMaterialList(const std::string& tkmFilePath)
{
	std::stringstream ss;
	TkmFile tkm;
	tkm.Load(tkmFilePath.c_str());
	ss << "TKM: " << tkmFilePath << "\n";
	const auto& meshes = tkm.GetMeshParts();
	for (size_t mi = 0; mi < meshes.size(); ++mi) {
		const auto& mesh = meshes[mi];
		ss << "  Mesh[" << mi << "] materials: " << mesh.materials.size() << "\n";
		for (size_t matIdx = 0; matIdx < mesh.materials.size(); ++matIdx) {
			const auto& mat = mesh.materials[matIdx];
			ss << "    Material[" << matIdx << "]: ";
			ss << "albedo=" << (mat.albedoMapFileName.empty() ? "(none)" : mat.albedoMapFileName) << ", ";
			ss << "normal=" << (mat.normalMapFileName.empty() ? "(none)" : mat.normalMapFileName) << ", ";
			ss << "specular=" << (mat.specularMapFileName.empty() ? "(none)" : mat.specularMapFileName) << ", ";
			ss << "reflection=" << (mat.reflectionMapFileName.empty() ? "(none)" : mat.reflectionMapFileName) << ", ";
			ss << "refraction=" << (mat.refractionMapFileName.empty() ? "(none)" : mat.refractionMapFileName) << "\n";
		}
	}
	ss << "\n";
	return ss.str();
}

// 指定した .tkm のマテリアル情報をメッセージとCSVで出力するユーティリティ
static void OutputTkmMaterialInfo(const std::string& tkmFilePath)
{
	// メッセージボックスで一覧を表示
	std::string materialList = BuildTkmMaterialList(tkmFilePath);
	// MessageBoxA(nullptr, materialList.c_str(), "TKM Material List", MB_OK);

	// CSV を出力（カレントパス）
	std::string csv = BuildTkmMaterialCSV(tkmFilePath);
	// ファイル名を作成（sample.tkm -> sample_materials.csv）
	std::string path = tkmFilePath;
	auto pos = path.find_last_of("\\/");
	std::string fname = (pos == std::string::npos) ? path : path.substr(pos + 1);
	auto dot = fname.find_last_of('.');
	std::string base = (dot == std::string::npos) ? fname : fname.substr(0, dot);
	std::string outName = base + "_materials.csv";
	std::ofstream ofs(outName, std::ios::binary);
	if (ofs) ofs << csv;
}

// GlassCube.tkm ファイルを作成
static void CreateGlassCubeTkmFile()
{
	// アセットフォルダを作成確認
	std::string modelPath = "Assets/modelData/";
	CreateDirectoryA(modelPath.c_str(), NULL);

	// 出力ファイルパス
	std::string outputPath = "Assets/modelData/GlassCube.tkm";

	FILE* fp = fopen(outputPath.c_str(), "wb");
	if (fp == nullptr) {
		MessageBoxA(nullptr, "ファイルの作成に失敗しました。", "エラー", MB_OK);
		return;
	}

	// ヘッダの作成
	uint8_t version = 100;
	uint8_t isFlatShading = 0;
	uint16_t numMeshParts = 1;

	fwrite(&version, sizeof(version), 1, fp);
	fwrite(&isFlatShading, sizeof(isFlatShading), 1, fp);
	fwrite(&numMeshParts, sizeof(numMeshParts), 1, fp);

	// メッシュパーツヘッダ
	uint32_t numMaterial = 1;
	uint32_t numVertex = 24;  // 立方体: 6面 * 4頂点
	uint8_t indexSize = 4;    // 32ビットインデックス
	uint8_t pad[3] = { 0, 0, 0 };

	fwrite(&numMaterial, sizeof(numMaterial), 1, fp);
	fwrite(&numVertex, sizeof(numVertex), 1, fp);
	fwrite(&indexSize, sizeof(indexSize), 1, fp);
	fwrite(pad, sizeof(pad), 1, fp);

	// マテリアル情報
	auto writeTextureName = [fp](const std::string& name) {
		uint32_t len = static_cast<uint32_t>(name.length());
		fwrite(&len, sizeof(len), 1, fp);
		if (len > 0) {
			fwrite(name.c_str(), len + 1, 1, fp);
		}
	};

	// 硬コードされたマップデータのファイル名
	std::string albedoMapName = "test.DDS";
	std::string normalMapName = "";
	std::string specularMapName = "spec.DDS";
	std::string reflectionMapName = "";
	std::string refractionMapName = "";

	writeTextureName(albedoMapName);
	writeTextureName(normalMapName);
	writeTextureName(specularMapName);
	writeTextureName(reflectionMapName);
	writeTextureName(refractionMapName);

	// 立方体の頂点データ
	// 1辺の長さが100.0で、中心はcenterで指定した座標
	float halfSize = 20.0f;
	// 立方体の中心点（ここを変更することで立方体の生成位置をずらせる）
	Vector3 center = { 50.0f, 25.0f, 50.0f };

	// 頂点データ構造: pos[3], normal[3], uv[2], weights[4], indices[4]
	struct VertexData {
		float pos[3];
		float normal[3];
		float uv[2];
		float weights[4];
		int16_t indices[4];
	};

	std::vector<VertexData> vertices;

	// 立方体の6面を定義
	// +X面 (右)
	vertices.push_back({ {halfSize, -halfSize, -halfSize}, {1,0,0}, {0,0}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ {halfSize,  halfSize, -halfSize}, {1,0,0}, {1,0}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ {halfSize,  halfSize,  halfSize}, {1,0,0}, {1,1}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ {halfSize, -halfSize,  halfSize}, {1,0,0}, {0,1}, {1,0,0,0}, {-1,-1,-1,-1} });

	// -X面 (左)
	vertices.push_back({ {-halfSize, -halfSize,  halfSize}, {-1,0,0}, {0,0}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ {-halfSize,  halfSize,  halfSize}, {-1,0,0}, {1,0}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ {-halfSize,  halfSize, -halfSize}, {-1,0,0}, {1,1}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ {-halfSize, -halfSize, -halfSize}, {-1,0,0}, {0,1}, {1,0,0,0}, {-1,-1,-1,-1} });

	// +Y面 (上)
	vertices.push_back({ {-halfSize,  halfSize, -halfSize}, {0,1,0}, {0,0}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ { halfSize,  halfSize, -halfSize}, {0,1,0}, {1,0}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ { halfSize,  halfSize,  halfSize}, {0,1,0}, {1,1}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ {-halfSize,  halfSize,  halfSize}, {0,1,0}, {0,1}, {1,0,0,0}, {-1,-1,-1,-1} });

	// -Y面 (下)
	vertices.push_back({ {-halfSize, -halfSize,  halfSize}, {0,-1,0}, {0,0}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ { halfSize, -halfSize,  halfSize}, {0,-1,0}, {1,0}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ { halfSize, -halfSize, -halfSize}, {0,-1,0}, {1,1}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ {-halfSize, -halfSize, -halfSize}, {0,-1,0}, {0,1}, {1,0,0,0}, {-1,-1,-1,-1} });

	// +Z面 (前)
	vertices.push_back({ {-halfSize, -halfSize,  halfSize}, {0,0,1}, {0,0}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ {-halfSize,  halfSize,  halfSize}, {0,0,1}, {1,0}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ { halfSize,  halfSize,  halfSize}, {0,0,1}, {1,1}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ { halfSize, -halfSize,  halfSize}, {0,0,1}, {0,1}, {1,0,0,0}, {-1,-1,-1,-1} });

	// -Z面 (後ろ)
	vertices.push_back({ { halfSize, -halfSize, -halfSize}, {0,0,-1}, {0,0}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ { halfSize,  halfSize, -halfSize}, {0,0,-1}, {1,0}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ {-halfSize,  halfSize, -halfSize}, {0,0,-1}, {1,1}, {1,0,0,0}, {-1,-1,-1,-1} });
	vertices.push_back({ {-halfSize, -halfSize, -halfSize}, {0,0,-1}, {0,1}, {1,0,0,0}, {-1,-1,-1,-1} });

	// 頂点データを書き込み（中心点分のオフセットを加算する）
	for (auto vertex : vertices) {
		vertex.pos[0] += center.x;
		vertex.pos[1] += center.z;
		vertex.pos[2] += center.y;
		fwrite(&vertex, sizeof(VertexData), 1, fp);
	}

	// インデックスデータ (ポリゴン数 = 12, インデクス数 = 36)
	int numPolygon = 12;
	fwrite(&numPolygon, sizeof(numPolygon), 1, fp);

	// 立方体のインデックス (3ds Max では1から開始するので、+1してから書き込む)
	// 注意: 四角形は必ず同じ対角線(n〜n+2)で2つの三角形に分割する。
	//       +X,-X面は頂点の巻き順が法線と一致しているためそのままだが、
	//       +Y,-Y,+Z,-Z面は頂点の巻き順が法線と逆になっているため、三角形の頂点順序を反転させている。
	uint32_t indices[] = {
		// +X面
		0,1,2,  0,2,3,
		// -X面
		4,5,6,  4,6,7,
		// +Y面（巻き順反転）
		8,10,9,  8,11,10,
		// -Y面（巻き順反転）
		12,14,13,  12,15,14,
		// +Z面（巻き順反転）
		16,18,17,  16,19,18,
		// -Z面（巻き順反転）
		20,22,21,  20,23,22
	};

	// TkmFile::LoadIndexBuffer は読み込み時に「index - 1」を行う（3ds Max形式が1始まりのため）。
	// このため、0始まりで定義したインデックスは+1してから書き込む必要がある。
	for (uint32_t idx : indices) {
		uint32_t writeIdx = idx + 1;
		fwrite(&writeIdx, sizeof(uint32_t), 1, fp);
	}

	fclose(fp);
	MessageBoxA(nullptr, "GlassCube.tkm ファイルを作成しました。", "成功", MB_OK);
}

// CSV 向けに値をエスケープして引用符で囲む
static std::string EscapeCsv(const std::string& v)
{
	std::string out = v;
	// ダブルクォートを2つにする
	size_t pos = 0;
	while ((pos = out.find('"', pos)) != std::string::npos) {
		out.insert(pos, "\"");
		pos += 2;
	}
	return std::string("\"") + out + "\"";
}

// 指定した .tkm を CSV 形式で返す（ヘッダー含む）
static std::string BuildTkmMaterialCSV(const std::string& tkmFilePath)
{
	std::stringstream ss;
	ss << "TKM Path,MeshIndex,MaterialIndex,Albedo,Normal,Specular,Reflection,Refraction\n";
	TkmFile tkm;
	tkm.Load(tkmFilePath.c_str());
	const auto& meshes = tkm.GetMeshParts();
	for (size_t mi = 0; mi < meshes.size(); ++mi) {
		const auto& mesh = meshes[mi];
		for (size_t matIdx = 0; matIdx < mesh.materials.size(); ++matIdx) {
			const auto& mat = mesh.materials[matIdx];
			ss << EscapeCsv(tkmFilePath) << "," << mi << "," << matIdx << ",";
			ss << EscapeCsv(mat.albedoMapFileName) << "," << EscapeCsv(mat.normalMapFileName) << ",";
			ss << EscapeCsv(mat.specularMapFileName) << "," << EscapeCsv(mat.reflectionMapFileName) << ",";
			ss << EscapeCsv(mat.refractionMapFileName) << "\n";
		}
	}
	return ss.str();
}

const int NUM_DIRECTIONAL_LIGHT = 4; // ディレクションライトの数

/// <summary>
/// ディレクションライト
/// </summary>
struct DirectionalLight
{
	Vector3 direction;  // ライトの方向
	float pad0;         // パディング
	Vector4 color;      // ライトのカラー
};

/// <summary>
/// ライト構造体
/// </summary>
struct Light
{
	DirectionalLight directionalLight[NUM_DIRECTIONAL_LIGHT]; // ディレクションライト
	Vector3 eyePos;             // カメラの位置
	float specPow;              // スペキュラの絞り
	Vector3 ambinetLight;       // 環境光
};

#define USE_UNITY_CHAN 0
#define USE_NINJA 1
#define USE_ROBO 2

#define USE_MODEL 2

///////////////////////////////////////////////////////////////////
// ウィンドウプログラムのメイン関数
///////////////////////////////////////////////////////////////////
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	// ゲームの初期化
	InitGame(hInstance, hPrevInstance, lpCmdLine, nCmdShow, TEXT("Game"));

	srand(time(nullptr));
	//////////////////////////////////////
	// ここから初期化を行うコードを記述する
	//////////////////////////////////////

	// 3Dモデルを作成
	Model model, bgModel, glassCubeModel;
	ModelInitData initData;

	// GlassCube.tkmファイルを作成
	CreateGlassCubeTkmFile();

	initData.m_tkmFilePath = "Assets/modelData/sample.tkm";
	initData.m_fxFilePath = "Assets/shader/NoAnimModel_PBR.fx";

	// materialのリストを出力
	OutputTkmMaterialInfo(initData.m_tkmFilePath);

	model.Init(initData);

	initData.m_tkmFilePath = "Assets/modelData/bg/bg.tkm";
	bgModel.Init(initData);

	// materialのリストを出力
	OutputTkmMaterialInfo(initData.m_tkmFilePath);

	initData.m_tkmFilePath = "Assets/modelData/GlassCube.tkm";
	glassCubeModel.Init(initData);

	// materialのリストを出力
	OutputTkmMaterialInfo(initData.m_tkmFilePath);



	//////////////////////////////////////
	// 初期化を行うコードを書くのはここまで！！！
	//////////////////////////////////////
	auto& renderContext = g_graphicsEngine->GetRenderContext();

	g_camera3D->SetPosition(0.0f, 50.0f, 120.0f);
	g_camera3D->SetTarget(0.0f, 50.0f, 200.0f);

	g_graphicsEngine->RegistModelToRaytracingWorld(model);
	g_graphicsEngine->RegistModelToRaytracingWorld(bgModel);
	g_graphicsEngine->RegistModelToRaytracingWorld(glassCubeModel);
	g_graphicsEngine->BuildRaytracingWorld(renderContext);

	// ここからゲームループ
	while (DispatchWindowMessage())
	{
		// レンダリング開始
		g_engine->BeginFrame();

		if (g_pad[0]->IsPress(enButtonLB1))
		{
			g_camera3D->MoveUp(g_pad[0]->GetLStickYF());
		}
		else
		{
			g_camera3D->MoveForward(-g_pad[0]->GetLStickYF());
		}
		g_camera3D->MoveRight(g_pad[0]->GetLStickXF());
		Quaternion qRotX, qRotY;
		qRotX.SetRotationX(g_pad[0]->GetRStickYF() * -0.005f);
		g_camera3D->RotateOriginTarget(qRotX);
		qRotY.SetRotationY(g_pad[0]->GetRStickXF() * 0.005f);
		g_camera3D->RotateOriginTarget(qRotY);

		//////////////////////////////////////
		// ここから絵を描くコードを記述する
		//////////////////////////////////////
		g_graphicsEngine->DispatchRaytracing(renderContext);

		//////////////////////////////////////
		// 絵を描くコードを書くのはここまで！！！
		//////////////////////////////////////
		// レンダリング終了
		g_engine->EndFrame();
	}
	return 0;
}

