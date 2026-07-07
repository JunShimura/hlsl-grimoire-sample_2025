# Sample 16_03 から 16_04 へのアップグレード解説

本ドキュメントは、Sample_16_03（基本的なTBR実装）から Sample_16_04（リファクタリング版）への変更点を説明します。

## 目次
1. [概要](#概要)
2. [主要な変更点](#主要な変更点)
3. [構造体の変更](#構造体の変更)
4. [クラスの導入](#クラスの導入)
5. [レンダリングパイプラインの変更](#レンダリングパイプラインの変更)
6. [実装の詳細比較](#実装の詳細比較)

---

## 概要

### Sample_16_03（Before）
- **アーキテクチャ**: 手続き型（プロシージャル）
- **処理フロー**: G-Buffer生成 → ライトカリング → ディファードライティング
- **コード構成**: main.cpp内に全て記述、関数で分割
- **特徴**: シンプルで理解しやすいが、責任が分散している

### Sample_16_04（After）
- **アーキテクチャ**: クラスベース（オブジェクト指向）
- **処理フロー**: ZPrepass → ライトカリング → フォワードレンダリング
- **コード構成**: LightCullingクラスで管理、より整理されている
- **特徴**: 拡張性が高く、保守性が向上

### 根本的な違い

| 項目 | 16_03 | 16_04 |
|------|-------|-------|
| **ライティング方式** | ディファードライティング（G-Bufferベース） | フォワードレンダリング（ライトカリングベース） |
| **ライトカリング実装** | グローバル変数・関数で実装 | `LightCullingクラス`で実装 |
| **パイプライン** | G-Buffer → ディファードライティング | ZPrepass → ライトカリング → フォワード |
| **コード整理** | 分散型 | 集約型 |

---

## 主要な変更点

### 1. Light構造体の変更

#### 16_03版
```cpp
struct Light
{
	DirectionalLight directionLights[ NUM_DIRECTION_LIGHT];
	PointLight pointLights[MAX_POINT_LIGHT];
	Matrix mViewProjInv;
	Vector3 eyePos;
	float specPow;
	int numPointLight;
};

struct LightCullingCameraData
{
	Matrix mProj;           // プロジェクション行列
	Matrix mProjInv;        // 逆プロジェクション行列
	Matrix mCameraRot;      // カメラ回転
	Vector4 screenParam;    // スクリーン情報（Near, Far, Width, Height）
};
```

#### 16_04版
```cpp
struct Light
{
	DirectionalLight directionLights[ NUM_DIRECTION_LIGHT];
	PointLight pointLights[MAX_POINT_LIGHT];
	Matrix mViewProjInv;
	Vector4 screenParam;     // ← screenParamがLight構造体に統合
	Vector3 eyePos;
	float specPow;
	int numPointLight;
};
```

**変更の理由**: 
- `screenParam`をLight構造体に統合
- シェーダーへ渡すデータの一元管理
- 定数バッファの管理が簡潔になる

### 2. LightCullingクラスの導入

#### 16_03では個別に初期化
```cpp
// step-1 コンピュートシェーダーをロード
Shader csLightCulling;
csLightCulling.LoadCS("Assets/shader/lightCulling.fx", "CSMain");

// step-2 パイプラインステート
PipelineState lightCullingPipelineState;
InitPipelineState(rootSignature, lightCullingPipelineState, csLightCulling);

// step-3 UAV作成
RWStructuredBuffer pointLightNoListInTileUAV;
pointLightNoListInTileUAV.Init(sizeof(int), MAX_POINT_LIGHT * NUM_TILE, nullptr);

// step-4 定数バッファ
ConstantBuffer cameraParamCB;
cameraParamCB.Init(sizeof(lightCullingCameraData), &lightCullingCameraData);

// ... 複数の初期化コード
```

#### 16_04ではクラスで統一
```cpp
class LightCulling
{
private:
	// 内部管理：
	Shader m_cs;
	PipelineState m_pipelineState;
	RWStructuredBuffer m_pointLightNoListInTileUAV;
	ConstantBuffer m_cameraDataGPU;
	ConstantBuffer m_lightGPU;
	DescriptorHeap m_descriptorHeap;

public:
	void Init(RootSignature& rs, Light& light, RenderTarget& depthRT);
	void Dispatch(RenderContext& renderContext);
	RWStructuredBuffer& GetPointLightNoListInTileUAV();
};
```

**メリット**：
- リソース管理が一箇所に集約
- 初期化・ディスパッチ処理が明確
- 後から機能追加が容易
- コードの再利用性が向上

---

## 構造体の変更

### Light構造体のメモリレイアウト比較

#### 16_03版（個別の定数バッファ）
```
定数バッファ1（LightCullingCameraData）:
  mProj       [64 bytes]
  mProjInv    [64 bytes]
  mCameraRot  [64 bytes]
  screenParam [16 bytes]

定数バッファ2（Light）:
  directionLights   [64 × 4 = 256 bytes]
  pointLights       [48 × 1000 bytes]
  mViewProjInv      [64 bytes]
  eyePos            [12 bytes]
  specPow           [4 bytes]
  numPointLight     [4 bytes]
```

#### 16_04版（統合Light構造体）
```
定数バッファ1（LightCullingCameraData）:
  mProj       [64 bytes]  ※ UAV出力用のカメラデータのみ
  mProjInv    [64 bytes]
  mCameraRot  [64 bytes]

定数バッファ2（Light）:
  directionLights   [64 × 4 = 256 bytes]
  pointLights       [48 × 1000 bytes]
  mViewProjInv      [64 bytes]
  screenParam       [16 bytes]  ← 統合
  eyePos            [12 bytes]
  specPow           [4 bytes]
  numPointLight     [4 bytes]
```

**統合による効果**:
- シェーダーで参照する定数バッファが減少
- ディレクティブ・ライティングと共通データを統一管理

---

## クラスの導入

### LightCullingクラスの構造

```cpp
class LightCulling
{
private:
	struct alignas(16) LightCullingCameraData
	{
		Matrix mProj;       
		Matrix mProjInv;    
		Matrix mCameraRot;  
	};

	RootSignature* m_rootSignature;
	Shader m_cs;
	PipelineState m_pipelineState;
	RWStructuredBuffer m_pointLightNoListInTileUAV;
	LightCullingCameraData m_cameraDataCPU;
	ConstantBuffer m_cameraDataGPU;
	ConstantBuffer m_lightGPU;
	DescriptorHeap m_descriptorHeap;
	Light* m_light;

public:
	void Init(RootSignature& rs, Light& light, RenderTarget& depthRT);
	void Dispatch(RenderContext& renderContext);
	RWStructuredBuffer& GetPointLightNoListInTileUAV();
};
```

### 初期化処理（Init関数）

```cpp
void Init(RootSignature& rs, Light& light, RenderTarget& depthRT)
{
	m_light = &light;
	m_rootSignature = &rs;

	// 1. コンピュートシェーダーのロード
	m_cs.LoadCS("Assets/shader/lightCulling.fx", "CSMain");

	// 2. パイプラインステートの作成
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rs.Get();
	psoDesc.CS = CD3DX12_SHADER_BYTECODE(m_cs.GetCompiledBlob());
	m_pipelineState.Init(psoDesc);

	// 3. UAV（ポイントライトリスト）の作成
	m_pointLightNoListInTileUAV.Init(
		sizeof(int),
		MAX_POINT_LIGHT * NUM_TILE,
		nullptr
	);

	// 4. カメラデータ（CPU側）の設定
	m_cameraDataCPU.mProj = g_camera3D->GetProjectionMatrix();
	m_cameraDataCPU.mProjInv.Inverse(g_camera3D->GetProjectionMatrix());
	m_cameraDataCPU.mCameraRot = g_camera3D->GetCameraRotation();

	// 5. GPU側定数バッファの作成
	m_cameraDataGPU.Init(sizeof(m_cameraDataCPU), &m_cameraDataCPU);
	m_lightGPU.Init(sizeof(light), &light);

	// 6. ディスクリプタヒープの構築
	m_descriptorHeap.RegistShaderResource(0, depthRT.GetRenderTargetTexture());
	m_descriptorHeap.RegistUnorderAccessResource(0, m_pointLightNoListInTileUAV);
	m_descriptorHeap.RegistConstantBuffer(0, m_cameraDataGPU);
	m_descriptorHeap.RegistConstantBuffer(1, m_lightGPU);
	m_descriptorHeap.Commit();
}
```

### ディスパッチ処理（Dispatch関数）

```cpp
void Dispatch(RenderContext& renderContext)
{
	renderContext.SetComputeRootSignature(*m_rootSignature);
	m_lightGPU.CopyToVRAM(*m_light);  // ライト情報を最新化
	renderContext.SetComputeDescriptorHeap(m_descriptorHeap);
	renderContext.SetPipelineState(m_pipelineState);
	renderContext.Dispatch(
		FRAME_BUFFER_W / TILE_WIDTH,
		FRAME_BUFFER_H / TILE_HEIGHT,
		1
	);
}
```

**クラス導入のメリット**:
1. **責任の一元化**: ライトカリング関連の全機能がクラス内で管理
2. **再利用性**: 他のシーンでもLightCullingクラスを使用可能
3. **拡張性**: キャッシング、デバッグ機能の追加が容易
4. **テスト容易性**: ユニットテストが書きやすい

---

## レンダリングパイプラインの変更

### 16_03のパイプライン（ディファードレンダリング）

```
┌─────────────────────────────┐
│      G-Buffer Generation    │
│  (G-Buffer Pass)            │
│                             │
│ - Albedo (RGBA8)            │
│ - Normal (RGBA16F)          │
│ - Depth (R32F)              │
└──────────────┬──────────────┘
			   │
			   ↓
┌─────────────────────────────┐
│   Light Culling             │
│  (Compute Shader)           │
│                             │
│ タイルごとにライト割り当て  │
│ → PointLightListUAV出力     │
└──────────────┬──────────────┘
			   │
			   ↓
┌─────────────────────────────┐
│  Deferred Lighting          │
│  (Full-screen Quad)         │
│                             │
│ G-Bufferと                  │
│ PointLightListを参照        │
│ → 最終色を計算              │
└──────────────┬──────────────┘
			   │
			   ↓
		[Frame Buffer]
```

**特徴**:
- G-Bufferに複数の情報を格納
- 後処理でライティング計算
- テクスチャ帯域幅多用（高価）

### 16_04のパイプライン（フォワードレンダリング + TBR）

```
┌─────────────────────────────┐
│      Z-Prepass              │
│  (Depth-only pass)          │
│                             │
│ - Depth RT生成              │
│ - 深度値をタイルごと保存    │
└──────────────┬──────────────┘
			   │
			   ↓
┌─────────────────────────────┐
│   Light Culling             │
│  (Compute Shader)           │
│                             │
│ Depthを参照してタイルの    │
│ Z範囲を計算                 │
│ → PointLightListUAV出力     │
└──────────────┬──────────────┘
			   │
			   ↓
┌─────────────────────────────┐
│  Forward Rendering          │
│  (Model Drawing)            │
│                             │
│ ポイントライトリストを参照  │
│ → 各ピクセルで必要なライトのみ使用
│ → 最終色を計算              │
└──────────────┬──────────────┘
			   │
			   ↓
		[Frame Buffer]
```

**特徴**:
- 2つのパス構成
- ZPrepassで事前にDepth情報を生成
- Forward passではライトカリング結果を活用
- メモリ帯域幅削減

---

## 実装の詳細比較

### 初期化フェーズ

#### 16_03（分散型）
```cpp
// main関数内に直接記述

// 1. コンピュートシェーダーのロード
Shader csLightCulling;
csLightCulling.LoadCS("Assets/shader/lightCulling.fx", "CSMain");

// 2. パイプラインステート
PipelineState lightCullingPipelineState;
InitPipelineState(rootSignature, lightCullingPipelineState, csLightCulling);

// 3. UAV
RWStructuredBuffer pointLightNoListInTileUAV;
pointLightNoListInTileUAV.Init(sizeof(int), MAX_POINT_LIGHT * NUM_TILE, nullptr);

// 4-5. 定数バッファ（複数）
LightCullingCameraData lightCullingCameraData;
// ... 初期化コード
ConstantBuffer cameraParamCB;
cameraParamCB.Init(sizeof(lightCullingCameraData), &lightCullingCameraData);

ConstantBuffer lightCB;
lightCB.Init(sizeof(light), &light);

// 5. ディスクリプタヒープ
DescriptorHeap lightCullingDescriptorHeap;
// ... 登録とコミット

// 6. ディファードライティング用スプライト
Sprite defferdLightingSpr;
InitDefferedLightingSprite(defferdLightingSpr, gbuffers, ...);
```

#### 16_04（クラス型）
```cpp
// main関数内（シンプル）

// ライトカリングの初期化（クラス内で全て管理）
LightCulling lightCulling;
lightCulling.Init(rootSignature, light, zprepass.GetDepthRenderTarget());

// モデル初期化時にライトカリング結果を渡す
InitModel(teapotModel, bgModel, light, lightCulling.GetPointLightNoListInTileUAV());
```

### ゲームループフェーズ

#### 16_03（明示的な多ステップ）
```cpp
while (DispatchWindowMessage())
{
	// ライト更新
	for (int i = 0; i < light.numPointLight; i++)
	{
		// ライト位置更新
	}

	// G-Bufferレンダリング
	RenderGBuffer(renderContext, gbuffers, ...);

	// ライトカリング
	renderContext.SetComputeRootSignature(rootSignature);
	lightCB.CopyToVRAM(light);
	renderContext.SetComputeDescriptorHeap(lightCullingDescriptorHeap);
	renderContext.SetPipelineState(lightCullingPipelineState);
	renderContext.Dispatch(FRAME_BUFFER_W / TILE_WIDTH, ...);

	// リソースバリア
	renderContext.TransitionResourceState(...);

	// ディファードライティング
	DefferedLighting(renderContext, defferdLightingSpr);

	// リソースバリア解除
	renderContext.TransitionResourceState(...);
}
```

#### 16_04（クラスメソッドで統一）
```cpp
while (DispatchWindowMessage())
{
	// ライト更新
	for (int i = 0; i < light.numPointLight; i++)
	{
		// ライト位置更新
	}

	// step-3: ZPrepass → ライトカリング → フォワードレンダリング
	// zprepass.Dispatch(renderContext);
	lightCulling.Dispatch(renderContext);
	// teapotModel.Draw(renderContext);
}
```

**16_04では**:
- `Dispatch()`内部で全ての処理を統括
- リソースバリアもクラス内で管理可能
- コードが簡潔で読みやすい

### モデル初期化の違い

#### 16_03（スプライトベース）
```cpp
void InitDefferedLightingSprite(
	Sprite& defferedSprite,
	RenderTarget* gbuffers[],
	int numGbuffer,
	Light& light,
	IShaderResource& srv)
{
	SpriteInitData spriteInitData;
	spriteInitData.m_width = FRAME_BUFFER_W;
	spriteInitData.m_height = FRAME_BUFFER_H;
	spriteInitData.m_expandShaderResoruceView = &srv;  // ← UAV参照

	// G-Bufferテクスチャを設定
	for (int i = 0; i < numGbuffer; i++)
	{
		spriteInitData.m_textures[i] = &gbuffers[i]->GetRenderTargetTexture();
	}

	spriteInitData.m_fxFilePath = "Assets/shader/defferedLighting.fx";
	spriteInitData.m_expandConstantBuffer = &light;
	spriteInitData.m_expandConstantBufferSize = sizeof(light);
	defferedSprite.Init(spriteInitData);
}
```

#### 16_04（モデルベース、フォワードパス）
```cpp
void InitModel(
	Model& teapotModel, 
	Model& bgModel, 
	Light& light, 
	IShaderResource& pointLightNoListInTileUAV)  // ← UAV参照
{
	// ティーポット
	ModelInitData teapotModelInitData;
	teapotModelInitData.m_tkmFilePath = "Assets/modelData/teapot.tkm";
	teapotModelInitData.m_fxFilePath = "Assets/shader/model.fx";  // フォワードシェーダー
	teapotModelInitData.m_expandConstantBuffer = &light;
	teapotModelInitData.m_expandConstantBufferSize = sizeof(light);
	teapotModelInitData.m_expandShaderResoruceView[0] = &pointLightNoListInTileUAV;
	teapotModel.Init(teapotModelInitData);

	// 背景
	// 同様...
}
```

**変更点**:
- ディファード用の全画面スプライトから、通常のモデル描画へ
- フォワードレンダリングなので、各モデルで個別にライティング計算
- ポイントライトリストをシェーダーリソースとして参照

---

## シェーダーの変更

### 16_03：defferedLighting.fx（全画面ライティング）
```hlsl
Texture2D<float4> albedo : register(t0);
Texture2D<float4> normal : register(t1);
Texture2D<float> depth : register(t2);
StructuredBuffer<int> pointLightNoListInTile : register(t3);

cbuffer Light : register(b0)
{
	// ディレクションライト
	// ポイントライト配列
	// その他
};

float4 main(PSInput input) : SV_TARGET
{
	// G-Bufferから値を読み取り
	float4 albedoColor = albedo.Load(...);
	float4 normalColor = normal.Load(...);
	float depthValue = depth.Load(...);

	// 逆投影でワールド座標を復元
	// ライトカリング結果を参照
	// ライティング計算
	return finalColor;
}
```

### 16_04：model.fx（フォワードシェーダー）
```hlsl
StructuredBuffer<int> pointLightNoListInTile : register(t0);

cbuffer Light : register(b0)
{
	// ディレクションライト
	// ポイントライト配列
	// screenParam（Near, Far, Width, Height）← 新規
	// その他
};

float4 main(PSInput input) : SV_TARGET
{
	// 通常のフォワードレンダリング
	// ピクセルのZ値と画面座標からタイルを計算
	// ライトカリング結果を参照
	// 必要なライトのみを使用してライティング計算
	return finalColor;
}
```

---

## パフォーマンス比較

| 指標 | 16_03 | 16_04 |
|------|-------|-------|
| **メモリ帯域幅** | 高い（G-Buffer複数参照） | 低い（フォワード最適化） |
| **GPU命令数** | 中程度（全画面処理） | 少ない（タイル化最適化） |
| **キャッシュ効率** | 低い（Z参照パターン複雑） | 高い（タイル単位で局所性向上） |
| **フレームレート** | 標準 | 改善（特に高ライト数時） |
| **スケーラビリティ** | ライト数に鈍感 | ライト数に敏感だが、TBRで最適化 |

---

## まとめ

### 16_03 → 16_04での改善点

1. **アーキテクチャの改良**
   - ディファードレンダリング → フォワード + TBR
   - ZPrepassでの事前深度生成で効率化

2. **コード構成の改善**
   - グローバル変数・関数の分散 → LightCullingクラスで集約
   - 責任の明確化とメンテナンス性向上

3. **メモリ管理の効率化**
   - screenParamをLight構造体に統合
   - 定数バッファの数を削減

4. **パフォーマンス向上**
   - メモリ帯域幅削減
   - タイル化による局所性向上
   - キャッシュ効率改善

5. **拡張性の向上**
   - クラスベース設計で機能追加が容易
   - 他のシーンへの再利用が簡単

### 推奨される活用方法

- **学習用**: 16_03で基本概念を理解
- **実装用**: 16_04のクラス設計をテンプレートとして活用
- **発展用**: 16_04をベースに、shadow mapping、MSAA対応など拡張
