# Tile-Based Rendering (TBR) ライトカリングの詳細解説

## 目次
1. [TBRの意味](#tbrの意味)
2. [TBRの仕組み](#tbrの仕組み)
3. [ライトの空間判定方法](#ライトの空間判定方法)
4. [具体例：1タイル区画の詳細解析](#具体例1タイル区画の詳細解析)
5. [複数タイル展開用バッファ](#複数タイル展開用バッファ)
6. [シェーダー構造](#シェーダー構造)
7. [実装フロー](#実装フロー)

---

## TBRの意味

### Tile-Based Rendering とは

**TBR（Tile-Based Rendering）** は、スクリーン空間を一定サイズの小さな矩形（**タイル**）に分割し、各タイルに対して必要なライトのみをフィルタリングするレンダリング手法です。

- **タイル**: 画面を分割する小さな矩形領域
  - 本サンプル：16×16ピクセルの正方形
- **目的**: 多数のポイントライトの中から、現在のピクセルに影響するライトだけを効率的に特定する

###従来の全光源計算との違い

| 計算方法 | 処理内容 | 特徴 |
|---------|--------|------|
| **全光源方式** | ピクセルごとに全ポイントライトの影響を計算 | 多数の計算冗長性。GPU負荷が高い |
| **TBR方式** | 各タイルに必要なライトだけをリスト化、ピクセルは共有リストを参照 | 不要なライト計算をスキップ。効率的 |

---

## TBRの仕組み

### フェーズ分類

TBRは2つのフェーズで構成されます：

```
フェーズ1: ライトカリング（コンピュートシェーダー）
   └─ 各タイルに対して、影響するポイントライトを特定し、リスト化

フェーズ2: ライティング（ピクセルシェーダー）
   └─ G-Bufferのデータとリスト化されたライトを使用して最終色を計算
```

### フェーズ1: ライトカリング処理の流れ

```
1. スクリーン全体を16×16ピクセルのタイルに分割
   例：1920×1080の場合
	   タイル幅数 = 1920 / 16 = 120
	   タイル高さ数 = 1080 / 16 = 67.5 → 68
	   総タイル数 = 120 × 68 = 8,160個

2. コンピュートシェーダーをディスパッチ
   スレッドグループ数 = タイルの数
   → 各スレッドグループが1タイルを担当

3. 各スレッドグループ内で：
   a) タイルのZ値範囲を計算
   b) 全ポイントライトをチェック
   c) タイルと球（ポイントライト）の交差判定
   d) 交差したライトのみをリストに追加

4. リスト化されたデータをUAV（Unordered Access View）に出力
```

### コード実装（Sample_16_03）の対応部分

```cpp
// step-3 タイルごとのポイントライトの番号のリストを出力するUAVを初期化
RWStructuredBuffer pointLightNoListInTileUAV;
pointLightNoListInTileUAV.Init(
	sizeof(int),                 // 1要素 = ポイントライト番号（4バイト整数）
	MAX_POINT_LIGHT * NUM_TILE,  // 全タイル × 最大ライト数
	nullptr
);

// step-6 コンピュートシェーダーをディスパッチ
renderContext.Dispatch(
	FRAME_BUFFER_W / TILE_WIDTH,    // X: 120
	FRAME_BUFFER_H / TILE_HEIGHT,   // Y: 68
	1                               // Z: 1
);
```

---

## ライトの空間判定方法

### タイルとポイントライトの交差判定

ライトカリングでは、**球体（ポイントライト）とフラスタム（タイルの3D空間範囲）の交差判定**を実行します。

#### 1. タイルの3D空間範囲（フラスタム）を計算

タイルはスクリーン上の2D矩形ですが、3D空間では**ビューフラスタムの一部**になります。

```
スクリーン座標系：
┌─────────────────────┐
│ タイル(0,0) 16×16px │
│ ┌──────────┐        │
│ │          │        │
│ └──────────┘        │
│                     │
└─────────────────────┘

↓ 逆投影変換

3D空間（ビュー座標系）：
	 /
	/│ Far平面
   / │
  /  ├─ フラスタム
 /   │
/    │
─────┼─ Near平面
	 │
```

#### 2. 球体との交差判定手順

**ポイントライト**は球体として表現されます：
- 中心座標：ビュー空間での位置
- 半径：ライトの影響範囲（range）

交差判定の流れ：

```
1. ポイントライトの中心座標をビュー座標系に変換
   position_view = light.positionInView

2. タイルのフラスタムコーンと球の距離を計算

3. 距離判定：
   if (球とフラスタムが接触) {
	   → このタイルにこのライトを追加
   }
```

### コードでの座標変換

```cpp
// ゲームループ内：ポイントライト位置をビュー空間に変換
Matrix mView = g_camera3D->GetViewMatrix();
for (int i = 0; i < light.numPointLight; i++)
{
	auto& pt = light.pointLights[i];
	qRot.Apply(pt.position);           // ワールド座標で回転
	pt.positionInView = pt.position;
	mView.Apply(pt.positionInView);    // ビュー座標に変換
}

// 定数バッファで送信
lightCB.Init(sizeof(light), &light);
```

---

## 具体例：1タイル区画の詳細解析

### 例：スクリーンのタイル座標(10, 5)の場合

#### ステップ1：スクリーン座標の計算

```
タイル座標 (TileX=10, TileY=5)
タイルサイズ = 16×16ピクセル

スクリーン上の矩形：
左上：(10×16, 5×16) = (160, 80)
右下：(11×16, 6×16) = (176, 96)
```

#### ステップ2：正規化座標への変換

```
フレームバッファサイズ：1920×1080

正規化座標（-1.0～1.0）：
左上：
  x = (160 / 1920) × 2 - 1 = -0.833
  y = (1080 - 80) / 1080 × 2 - 1 = 0.852

右下：
  x = (176 / 1920) × 2 - 1 = -0.816
  y = (1080 - 96) / 1080 × 2 - 1 = 0.822
```

#### ステップ3：ビュー座標系への逆投影

逆プロジェクション行列を使用して、スクリーン矩形を3D空間に展開：

```
逆プロジェクション行列適用：
  vec4 posNear = inverse(projMatrix) × vec4(x, y, 0, 1);
  vec4 posFar  = inverse(projMatrix) × vec4(x, y, 1, 1);

結果：
タイルの3D空間範囲（ビュー座標系）
Near平面（z = near）での範囲：
  x: -0.15～-0.10
  y: 0.12～0.18

Far平面（z = far）での範囲：
  x: -15.5～-10.2
  y: 12.4～18.6

Z軸範囲：z = near～far（例：0.1～1000.0）
```

#### ステップ4：このタイルに含まれるライトを判定

```
タイルのフラスタムと各ポイントライトを比較：

Light 0: position = (50, 20, 100)、range = 50
  距離 = sqrt((50-(-5))^2 + (20-15)^2 + (100-500)^2)
	   ≈ 407.6
  距離 > range ？→ YES → 対象外

Light 15: position = (-5, 15, 80)、range = 50
  距離 = sqrt((-5-(-5))^2 + (15-15)^2 + (80-500)^2)
	   ≈ 420
  距離 > range ？→ YES → 対象外

Light 234: position = (-8, 18, 120)、range = 50
  距離 ≈ 50以下 ？→ YES → このタイルのリストに追加 ✓

...（全1000個のライトをチェック）

結果：このタイルに関連するライト = [234, 567, 789, ...] （例：12個）
```

#### ステップ5：リストをUAVに出力

```
ライトカリング用UAV構造：
メモリレイアウト = ポイントライト番号の配列

タイル(10, 5)の出力位置：
タイルインデックス = 10 + 5 × (1920/16) = 10 + 5 × 120 = 610

UAVメモリ：
位置600-610：[前のタイルのライトリスト...]
位置610-622：[234, 567, 789, ...]  ← タイル(10,5)のライトリスト
位置622-...: [次のタイルのライトリスト...]
```

---

## 複数タイル展開用バッファ

### バッファ設計の全体構造

#### 1. **ライトカリングUAV**（RWStructuredBuffer）

```cpp
RWStructuredBuffer pointLightNoListInTileUAV;
pointLightNoListInTileUAV.Init(
	sizeof(int),                 // 各要素のサイズ
	MAX_POINT_LIGHT * NUM_TILE,  // 総要素数
	nullptr
);
```

**パラメータの意味：**

| 要素 | 値 | 説明 |
|------|-----|------|
| `sizeof(int)` | 4バイト | 1つのライト番号は32bit符号付き整数 |
| `MAX_POINT_LIGHT` | 1000 | 最大ポイントライト数（本サンプル） |
| `NUM_TILE` | (1920/16)×(1080/16)=8160 | 総タイル数 |
| **総バッファサイズ** | 1000×8160×4 = **32.64MB** | 最大データ量（全タイルに全ライト） |

#### 2. メモリレイアウト

```
バッファの構造（概念図）：

┌─────────────────────────────────────────┐
│  タイル0のライトリスト                   │
│  [234, 567, 789, -1, -1, ...]           │  
│  最大1000要素（4000バイト）             │
├─────────────────────────────────────────┤
│  タイル1のライトリスト                   │
│  [100, 202, -1, -1, ...]                │
├─────────────────────────────────────────┤
│  タイル2のライトリスト                   │
│  [500, 600, 700, 800, -1, ...]          │
├─────────────────────────────────────────┤
│  ...                                     │
│  （中略：タイル3～8158）                 │
│  ...                                     │
├─────────────────────────────────────────┤
│  タイル8159のライトリスト                │
│  [1, 2, 3, 4, 5, ...]                   │
└─────────────────────────────────────────┘

※ -1は「リスト終端」を示すセンチネル値
```

#### 3. 各タイルの開始位置計算

```cpp
// ピクセルシェーダーで使用
uint tileX = pixelX / TILE_WIDTH;           // 例：pixelX=234 → tileX=14
uint tileY = pixelY / TILE_HEIGHT;          // 例：pixelY=142 → tileY=8

uint tileIndex = tileX + tileY * (FRAME_BUFFER_W / TILE_WIDTH);
				// = 14 + 8 × 120 = 974

uint listStartPos = tileIndex * MAX_POINT_LIGHT;
				  // = 974 × 1000 = 974,000

// リストから順番にライト番号を読み込み
for (uint i = 0; i < MAX_POINT_LIGHT; i++) {
	int lightIndex = pointLightNoListInTileUAV[listStartPos + i];
	if (lightIndex < 0) break;  // リスト終端

	PointLight light = pointLights[lightIndex];
	// ライティング計算...
}
```

### 補助バッファ

#### 4. **カメラ情報の定数バッファ**

```cpp
struct LightCullingCameraData
{
	Matrix mProj;           // プロジェクション行列
	Matrix mProjInv;        // 逆プロジェクション行列（重要！）
	Matrix mCameraRot;      // カメラ回転行列
	Vector4 screenParam;    // x:Near, y:Far, z:幅, w:高さ
};

ConstantBuffer cameraParamCB;
cameraParamCB.Init(sizeof(lightCullingCameraData), &lightCullingCameraData);
```

**用途：** スクリーン座標をビュー空間に逆投影

#### 5. **ライト定数バッファ**

```cpp
struct Light
{
	DirectionalLight directionLights[NUM_DIRECTION_LIGHT];
	PointLight pointLights[MAX_POINT_LIGHT];  // 全ライト情報
	Matrix mViewProjInv;
	Vector3 eyePos;
	float specPow;
	int numPointLight;
};

ConstantBuffer lightCB;
lightCB.Init(sizeof(light), &light);
```

---

## シェーダー構造

### ライトカリングコンピュートシェーダー（疑似コード）

```hlsl
// lightCulling.fx - CSMain

// 定数バッファ
cbuffer CameraParam : register(b0)
{
	float4x4 mProj;
	float4x4 mProjInv;
	float4x4 mCameraRot;
	float4 screenParam;  // x:Near, y:Far, z:Width, w:Height
};

cbuffer LightParam : register(b1)
{
	DirectionalLight directionLights[4];
	PointLight pointLights[1000];
	float4x4 mViewProjInv;
	float3 eyePos;
	float specPow;
	int numPointLight;
};

// シェーダーリソース
Texture2D<float> depthTexture : register(t0);

// アンオーダードアクセスビュー
RWStructuredBuffer<int> pointLightNoListInTile : register(u0);

// グループ共有メモリ
groupshared int groupLightList[1024];  // 1024 = グループ内のスレッド数
groupshared uint groupLightCount;

[numthreads(16, 16, 1)]  // 16×16 = 256スレッド/グループ
void CSMain(
	uint3 groupId : SV_GroupID,
	uint3 dispatchThreadId : SV_DispatchThreadID,
	uint3 groupThreadId : SV_GroupThreadID,
	uint threadIndexInGroup : SV_GroupIndex)
{
	// ========== ステップ1: グループの初期化 ==========
	if (threadIndexInGroup == 0) {
		groupLightCount = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	// ========== ステップ2: タイルのZ値範囲を計算 ==========

	// 各スレッドがタイル内の1ピクセルを担当
	uint pixelX = dispatchThreadId.x;
	uint pixelY = dispatchThreadId.y;

	// デプステクスチャから深度値を読み込み
	float depth = depthTexture[uint2(pixelX, pixelY)];

	// タイル全体の最小・最大深度を計算（グループ共有メモリ使用）
	// 省略：各スレッドの深度値からタイルの深度範囲を算出

	// ========== ステップ3: フラスタムコーンを計算 ==========

	// タイルの4つのコーナー座標（正規化座標）
	float2 corners[4] = {
		float2(pixelX / screenParam.z, pixelY / screenParam.w),
		float2((pixelX + TILE_WIDTH) / screenParam.z, pixelY / screenParam.w),
		float2(pixelX / screenParam.z, (pixelY + TILE_HEIGHT) / screenParam.w),
		float2((pixelX + TILE_WIDTH) / screenParam.z, (pixelY + TILE_HEIGHT) / screenParam.w)
	};

	// 各コーナーをビュー座標に逆投影
	float3 corners3D[4];
	for (int i = 0; i < 4; i++) {
		float4 posNDC = float4(corners[i] * 2.0 - 1.0, 1.0, 1.0);
		float4 posView = mul(posNDC, mProjInv);
		corners3D[i] = posView.xyz / posView.w;
	}

	// ========== ステップ4: 各ライトをチェック ==========

	uint localThreadIndex = threadIndexInGroup;

	// スレッドグループ内で複数ライトを処理
	for (int lightIdx = localThreadIndex; lightIdx < numPointLight; lightIdx += 256)
	{
		PointLight light = pointLights[lightIdx];

		// 球とフラスタムの交差判定
		bool intersects = SphereIntersectsFrustum(
			light.positionInView,
			light.range,
			corners3D
		);

		if (intersects) {
			// リストに追加（アトミック操作）
			uint count;
			InterlockedAdd(groupLightCount, 1, count);
			if (count < 1024) {
				groupLightList[count] = lightIdx;
			}
		}
	}

	GroupMemoryBarrierWithGroupSync();

	// ========== ステップ5: UAVに出力 ==========

	// タイルのグローバルインデックス
	uint tileIdx = groupId.x + groupId.y * (FRAME_BUFFER_W / TILE_WIDTH);
	uint uavStartPos = tileIdx * MAX_POINT_LIGHT;

	// グループ共有メモリからUAVにコピー
	for (uint i = localThreadIndex; i < groupLightCount; i += 256) {
		pointLightNoListInTile[uavStartPos + i] = groupLightList[i];
	}

	// リスト終端マーク
	if (localThreadIndex == 0 && groupLightCount < MAX_POINT_LIGHT) {
		pointLightNoListInTile[uavStartPos + groupLightCount] = -1;
	}
}

// ========== 交差判定関数 ==========

bool SphereIntersectsFrustum(float3 sphereCenter, float sphereRadius, float3 corners[4])
{
	// フラスタムコーンの軸方向を計算
	float3 coneAxis = normalize(
		(corners[0] + corners[1] + corners[2] + corners[3]) / 4.0 - sphereCenter
	);

	// 球がコーン内に含まれるか判定
	for (int i = 0; i < 4; i++) {
		float3 toCorner = corners[i] - sphereCenter;
		float distToAxis = length(cross(toCorner, coneAxis));

		if (distToAxis < sphereRadius) {
			return true;
		}
	}

	return false;
}
```

### ピクセルシェーダー（ディファードライティング）内での使用

```hlsl
// defferedLighting.fx - PSMain

Texture2D<float4> albedoTexture : register(t0);
Texture2D<float4> normalTexture : register(t1);
Texture2D<float> depthTexture : register(t2);

StructuredBuffer<int> pointLightNoListInTile : register(t3);

cbuffer Light : register(b0)
{
	DirectionalLight directionLights[4];
	PointLight pointLights[1000];
	float4x4 mViewProjInv;
	float3 eyePos;
	float specPow;
	int numPointLight;
};

float4 PSMain(
	float4 screenPos : SV_POSITION,
	float2 uv : TEXCOORD0) : SV_TARGET
{
	// G-Bufferから情報を読み込み
	float4 albedo = albedoTexture.Sample(samplerState, uv);
	float4 normal = normalTexture.Sample(samplerState, uv);
	float depth = depthTexture.Sample(samplerState, uv);

	// ========== ステップ1: タイルのライトリストを取得 ==========

	uint pixelX = (uint)screenPos.x;
	uint pixelY = (uint)screenPos.y;

	uint tileX = pixelX / TILE_WIDTH;
	uint tileY = pixelY / TILE_HEIGHT;

	uint tileIdx = tileX + tileY * (FRAME_BUFFER_W / TILE_WIDTH);
	uint listStartPos = tileIdx * MAX_POINT_LIGHT;

	// ========== ステップ2: リスト化されたライトのみを処理 ==========

	float4 diffuse = float4(0, 0, 0, 0);

	for (uint i = 0; i < MAX_POINT_LIGHT; i++) {
		int lightIdx = pointLightNoListInTile[listStartPos + i];
		if (lightIdx < 0) break;  // リスト終端

		PointLight light = pointLights[lightIdx];

		// ライティング計算（省略）
		float3 lightDir = normalize(light.positionInView - pixelPos);
		float3 lightColor = light.color * SomeLightingFunction(
			normal.xyz,
			lightDir,
			light.range
		);

		diffuse += float4(lightColor, 0);
	}

	// 最終色
	return albedo * diffuse;
}
```

---

## 実装フロー

### 初期化フェーズ

```
メイン処理（main.cpp）
├─ step-1: ライトカリングコンピュートシェーダーをロード
│   csLightCulling.LoadCS("Assets/shader/lightCulling.fx", "CSMain");
│
├─ step-2: パイプラインステート作成
│   lightCullingPipelineState.Init(...);
│
├─ step-3: UAVバッファ作成（各タイルのライトリスト出力先）
│   pointLightNoListInTileUAV.Init(sizeof(int), MAX_POINT_LIGHT * NUM_TILE, ...);
│
├─ step-4: 定数バッファ作成
│   │─ カメラパラメータ（逆プロジェクション行列含む）
│   │   cameraParamCB.Init(sizeof(lightCullingCameraData), ...);
│   │
│   └─ ライト情報（全ポイントライト）
│       lightCB.Init(sizeof(light), ...);
│
├─ step-5: ディスクリプタヒープ作成
│   lightCullingDescriptorHeap.RegistShaderResource(0, depthRT);     // 入力
│   lightCullingDescriptorHeap.RegistUnorderAccessResource(0, UAV);  // 出力
│   lightCullingDescriptorHeap.RegistConstantBuffer(0, cameraParamCB);
│   lightCullingDescriptorHeap.RegistConstantBuffer(1, lightCB);
│   lightCullingDescriptorHeap.Commit();
│
└─ ディファードライティングスプライト初期化
	defferdLightingSpr.Init(...);
```

### ゲームループフェーズ

```
各フレーム処理
├─ レンダリング開始
│
├─ G-Buffer生成
│   RenderGBuffer(...);  // アルベド、法線、深度を出力
│
├─ ライトカリング実行 ★★★重要★★★
│   │
│   ├─ ポイントライト位置をビュー座標に変換
│   │   for (i = 0; i < numPointLight; i++) {
│   │       pt.positionInView = ビュー座標に変換
│   │   }
│   │   lightCB.CopyToVRAM(light);  // GPU側にコピー
│   │
│   ├─ コンピュートシェーダーディスパッチ
│   │   renderContext.SetPipelineState(lightCullingPipelineState);
│   │   renderContext.Dispatch(120, 68, 1);  // 各タイルごと
│   │
│   ├─ リソースバリア（UAV → SRV）
│   │   TransitionResourceState(
│   │       UAV,
│   │       UNORDERED_ACCESS → PIXEL_SHADER_RESOURCE
│   │   );
│   │
│   └─ [このフェーズの成果]
│       pointLightNoListInTileUAV に各タイルのライトリストが格納される
│
├─ ディファードライティング実行
│   DefferedLighting(...);  // ピクセルシェーダー内でリスト参照
│
├─ リソースバリア復元（SRV → UAV）
│   TransitionResourceState(
│       UAV,
│       PIXEL_SHADER_RESOURCE → UNORDERED_ACCESS
│   );
│
└─ フレーム終了
```

---

## 性能の詳細分析

### ボトルネック分析

#### パターン1: 多くのライトが小さな領域に集中

```
タイルAの計算量：
  ├─ 全ライト(1000個)チェック  （必須）
  ├─ 交差判定計算 × 1000
  └─ 実際に関連するライト：950個

→ ほぼ全ライトをリスト化
→ 全光源方式との差がない
```

**対策:** ライトをグループ化する空間分割ツリー（BVH/Octree）を使用

#### パターン2: タイル内の深度差が大きい場合

```
深度分析：
  min_depth = 0.5
  max_depth = 500.0

フラスタム計算の精度低下
  → 交差判定の誤判定増加
  → 不要なライトもリスト化
```

**対策:** タイルをZ方向でも分割（3D Tiling）

### メモリ使用量の最適化

```
現在のバッファ構成：

【定額メモリ】
├─ UAVバッファ：1000 × 8160 × 4bytes ≈ 32.64 MB
├─ 定数バッファ（カメラ）：256 + 256 + 64 + 16 = 592 bytes
├─ 定数バッファ（ライト）：
│   4×(16+12) + 1000×(12+12+12+4) = 40,064 bytes
└─ グループ共有メモリ：1024 × 4 ≈ 4 KB

【削減案】
→ MAX_POINT_LIGHT を動的に設定
  例：500個に削減 → UAVサイズ ≈ 16.32 MB
→ リスト構造を圧縮
  例：負の数でリスト終端 ＆ ＤＬＴ（差分長エンコード）
```

---

## まとめ図

```
【TBRライトカリングの全体構図】

入力
├─ G-Buffer (深度テクスチャ)
├─ ポイントライト（ワールド座標）
├─ ビューマトリックス
└─ プロジェクション行列

	  ↓

[コンピュートシェーダー]
  - 画面を16×16タイルに分割
  - 各タイル = 1つのスレッドグループ
  - 全ポイントライト（1000個）をチェック
  - 球体 × フラスタム交差判定
  - 関連ライトをリスト化

	  ↓

[ライトリストUAV]
  タイル0: [ライト番号, ライト番号, ..., -1]
  タイル1: [ライト番号, ライト番号, ..., -1]
  ...
  タイル8159: [ライト番号, ライト番号, ..., -1]

	  ↓

[ピクセルシェーダー]
  - 各ピクセルのタイル座標を計算
  - 対応するタイルのリストを参照
  - リスト内のライトのみでライティング計算

	  ↓

出力
 └─ 最終的にはライティング済みのカラー
```

---

## 参考資料

- **対応コード**: Sample_16_03/main.cpp (lines 1-461)
- **シェーダーファイル**: Assets/shader/lightCulling.fx
- **関連クラス**: 
  - RWStructuredBuffer
  - ConstantBuffer
  - DescriptorHeap
  - RenderContext::Dispatch()

---

このドキュメントが、TBRライトカリングの理解に役立つことを祈っています！
