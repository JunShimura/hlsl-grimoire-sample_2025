# RenderingEngine の Execute 関数 詳細解説

## はじめに

この解説では、`RenderingEngine.cpp` の `Execute` 関数について、DirectX やレンダリングエンジンの初心者でも理解できるように、詳しく説明します。

---

## 目次

1. [DirectX とレンダリングエンジンとは](#1-directx-とレンダリングエンジンとは)
2. [Execute 関数が実行されるまでの準備段階](#2-execute-関数が実行されるまでの準備段階)
3. [Execute 関数の実行内容](#3-execute-関数の実行内容)
4. [まとめ](#4-まとめ)

---

## 1. DirectX とレンダリングエンジンとは

### DirectX とは？

**DirectX** は、Windows 上でゲームやマルチメディアアプリケーションを開発するための Microsoft が提供する技術です。その中でも **DirectX 12** は、GPU（グラフィックスカード）を使って3D画像を高速に描画するための API（プログラムを操作するための命令セット）です。

簡単に言うと、「3D のキャラクターや背景をモニターに映すための道具箱」のようなものです。

### レンダリングエンジンとは？

**レンダリングエンジン** は、DirectX のような低レベルな API を使いやすくラッピング（包み込む）して、複雑な描画処理を自動化・整理するシステムです。

例えば、ゲームでキャラクターを描画する際に、以下のような複雑な処理が必要です：
- 影を描く
- 光の反射を計算する
- 半透明なオブジェクトを正しく描く
- 画面のぼかしやグロー効果を加える

レンダリングエンジンは、これらの処理を **適切な順番で効率的に実行** するための「制御塔」の役割を果たします。

---

## 2. Execute 関数が実行されるまでの準備段階

`Execute` 関数（描画を実際に行う関数）が実行される前に、レンダリングエンジンは多くの準備作業を行います。これは **`Init()` 関数** で実行されます。

### Init() 関数の役割（準備段階）

```cpp
void RenderingEngine::Init()
{
	InitZPrepassRenderTarget();
	InitMainRenderTarget();
	InitGBuffer();
	InitMainRTSnapshotRenderTarget();
	InitCopyMainRenderTargetToFrameBufferSprite();
	InitShadowMapRender();
	InitDeferredLighting();
	m_postEffect.Init(m_mainRenderTarget, m_zprepassRenderTarget);
}
```

この `Init()` 関数では、以下の8つの初期化処理を行います。各処理を詳しく見ていきましょう。

---

### 2.1 InitZPrepassRenderTarget（深度の事前取得の準備）

**ZPrepass（ゼットプリパス）** とは、3D オブジェクトの「奥行き情報（深度）」だけを先に取得する処理です。

#### なぜ必要なのか？

通常、3D 空間では手前のオブジェクトが奥のオブジェクトを隠します。しかし、何も考えずに描画すると、遠くのオブジェクトを描いた後に手前のオブジェクトを描くという無駄な作業が発生します。

ZPrepass を行うことで、「どのピクセルに何が見えるか」を事前に判定でき、**無駄な描画を減らして処理を高速化** できます。

```cpp
void RenderingEngine::InitZPrepassRenderTarget()
{
	m_zprepassRenderTarget.Create(
		g_graphicsEngine->GetFrameBufferWidth(),   // 画面の幅
		g_graphicsEngine->GetFrameBufferHeight(),  // 画面の高さ
		1,
		1,
		DXGI_FORMAT_R32G32_FLOAT,     // カラー情報の形式（ここでは深度情報を格納）
		DXGI_FORMAT_D32_FLOAT         // 深度情報の形式（32bit の浮動小数点数）
	);
}
```

- **レンダーターゲット**: GPU が描画結果を書き込むメモリ領域
- **DXGI_FORMAT_D32_FLOAT**: 深度を表す32bit の数値形式

---

### 2.2 InitMainRenderTarget（メイン描画先の準備）

**メインレンダーターゲット** は、最終的な画像を描画するためのキャンバスです。

```cpp
void RenderingEngine::InitMainRenderTarget()
{
	m_mainRenderTarget.Create(
		g_graphicsEngine->GetFrameBufferWidth(),
		g_graphicsEngine->GetFrameBufferHeight(),
		1,
		1,
		DXGI_FORMAT_R32G32B32A32_FLOAT,  // RGBA（赤・緑・青・透明度）各32bit
		DXGI_FORMAT_UNKNOWN              // 深度バッファは使わない
	);
}
```

- **DXGI_FORMAT_R32G32B32A32_FLOAT**: 色情報を非常に高い精度で保存（HDR：High Dynamic Range）
- この高精度により、明るい部分や暗い部分の情報を失わずに保持できます

---

### 2.3 InitGBuffer（G-Buffer の準備）

**G-Buffer（Geometry Buffer）** は、「ディファードレンダリング」という高度な描画技法で使用するデータの保管庫です。

#### ディファードレンダリングとは？

通常のレンダリング（フォワードレンダリング）では、各オブジェクトを描画する際に光の計算も同時に行います。しかし、複数のライト（光源）がある場合、計算量が膨大になります。

**ディファードレンダリング** では、以下のように処理を2段階に分けます：
1. **第1段階**: オブジェクトの情報（色、法線、位置など）だけを G-Buffer に保存
2. **第2段階**: 保存した情報を使って、まとめて光の計算を行う

これにより、**複数の光源があっても効率的に処理** できます。

```cpp
void RenderingEngine::InitGBuffer()
{
	int frameBuffer_w = g_graphicsEngine->GetFrameBufferWidth();
	int frameBuffer_h = g_graphicsEngine->GetFrameBufferHeight();

	// アルベドカラー（物体本来の色）を出力用のレンダリングターゲット
	m_gBuffer[enGBufferAlbedo].Create(
		frameBuffer_w,
		frameBuffer_h,
		1,
		1,
		DXGI_FORMAT_R32G32B32A32_FLOAT,  // 高精度なカラー情報
		DXGI_FORMAT_D32_FLOAT            // 深度情報
	);

	// 法線（表面の向き）出力用のレンダリングターゲット
	m_gBuffer[enGBufferNormal].Create(
		frameBuffer_w,
		frameBuffer_h,
		1,
		1,
		DXGI_FORMAT_R8G8B8A8_UNORM,  // 8bit × 4チャンネル（メモリ効率重視）
		DXGI_FORMAT_UNKNOWN
	);

	// 金属度と滑らかさマップ出力用のレンダリングターゲット
	m_gBuffer[enGBufferMetalSmooth].Create(
		frameBuffer_w,
		frameBuffer_h,
		1,
		1,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_UNKNOWN
	);

	// ワールド座標（3D空間上の位置）出力用のレンダリングターゲット
	m_gBuffer[enGBufferWorldPos].Create(
		frameBuffer_w,
		frameBuffer_h,
		1,
		1,
		DXGI_FORMAT_R32G32B32A32_FLOAT,  // 位置情報は高精度が必要
		DXGI_FORMAT_UNKNOWN
	);

	// シャドウパラメータ出力用のレンダリングターゲット
	m_gBuffer[enGBUfferShadowParam].Create(
		frameBuffer_w,
		frameBuffer_h,
		1,
		1,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_UNKNOWN
	);
}
```

#### G-Buffer が保存する情報

| バッファ名 | 保存する情報 | 用途 |
|-----------|-------------|------|
| **Albedo** | 物体本来の色（テクスチャ色） | 基本色の情報 |
| **Normal** | 表面の向き（法線ベクトル） | 光の反射計算に使用 |
| **WorldPos** | 3D空間上の位置座標 | ライティング計算に使用 |
| **MetalSmooth** | 金属度と滑らかさ | 物理ベースレンダリング（PBR）用 |
| **ShadowParam** | 影のパラメータ | 影の描画に使用 |

---

### 2.4 InitMainRTSnapshotRenderTarget（スナップショットの準備）

**スナップショット** は、特定のタイミングでの画像を保存しておくための領域です。

```cpp
void RenderingEngine::InitMainRTSnapshotRenderTarget()
{
	for (auto& snapshotRt : m_mainRTSnapshots)
	{
		snapshotRt.Create(
			g_graphicsEngine->GetFrameBufferWidth(),
			g_graphicsEngine->GetFrameBufferHeight(),
			1,
			1,
			DXGI_FORMAT_R8G8B8A8_UNORM,   // 通常の8bitカラー
			DXGI_FORMAT_UNKNOWN
		);
	}
}
```

#### なぜスナップショットが必要？

例えば、半透明なガラスを描画する際に、その背後に何が描かれているかを知る必要があります。スナップショットを取ることで、**過去の描画状態を参照** できます。

---

### 2.5 InitCopyMainRenderTargetToFrameBufferSprite（最終出力の準備）

**フレームバッファ** は、実際にモニターに表示される最終的な画像を保持するメモリです。

```cpp
void RenderingEngine::InitCopyMainRenderTargetToFrameBufferSprite()
{
	SpriteInitData spriteInitData;

	// メインレンダーターゲットをテクスチャとして使用
	spriteInitData.m_textures[0] = &m_mainRenderTarget.GetRenderTargetTexture();

	// 解像度はフレームバッファーと同じ
	spriteInitData.m_width = g_graphicsEngine->GetFrameBufferWidth();
	spriteInitData.m_height = g_graphicsEngine->GetFrameBufferHeight();

	// 2D描画用のシェーダーを指定
	spriteInitData.m_fxFilePath = "Assets/shader/preset/sprite.fx";
	spriteInitData.m_colorBufferFormat[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	// スプライトを初期化（2D画像を描画するオブジェクト）
	m_copyMainRtToFrameBufferSprite.Init(spriteInitData);
}
```

この処理では、**2D スプライト（平面画像）** として画像をコピーする準備を行っています。

---

### 2.6 InitShadowMapRender（影描画の準備）

**シャドウマップ** は、影を描画するための深度情報を保存したテクスチャです。

```cpp
void RenderingEngine::InitShadowMapRender()
{
	// シャドウマップの描画処理の初期化
	for (auto& shadowMapRender : m_shadowMapRenders)
	{
		shadowMapRender.Init();
	}
}
```

#### 影の描画の仕組み

1. **光源の視点から** シーンを見て、深度情報を記録（シャドウマップの作成）
2. 実際の描画時に、各ピクセルが「光源から見えるか？」を判定
3. 見えない場合は影として描画

#### シャドウマップの複数用意について

このエンジンでは、**非常に多くのシャドウマップ** を用意しています。具体的には以下の構造になっています：

```cpp
// MyRenderer.h で定義されている定数
const int NUM_DEFERRED_LIGHTING_DIRECTIONAL_LIGHT = 4;  // ライトの数
const int NUM_SHADOW_MAP = 3;                           // 1つのライトあたりのシャドウマップ数

// RenderingEngine.h のメンバ変数
shadow::ShadowMapRender m_shadowMapRenders[NUM_DEFERRED_LIGHTING_DIRECTIONAL_LIGHT];
// つまり、4つのライト用のシャドウマップレンダラーを用意
```

**シャドウマップの合計数**：
```
4つのライト × 各ライト3枚のシャドウマップ = 合計12枚のシャドウマップ
```

#### なぜライトごとに複数のシャドウマップが必要なのか？

##### 1. **複数のライト対応**

このエンジンでは、最大4つのディレクショナルライト（平行光源）からの影を描画できます。

```
ライト0: 太陽光（メイン）
  ├─ 近景用シャドウマップ
  ├─ 中景用シャドウマップ
  └─ 遠景用シャドウマップ

ライト1: 補助光源
  ├─ 近景用シャドウマップ
  ├─ 中景用シャドウマップ
  └─ 遠景用シャドウマップ

ライト2: 地面からの照り返し
  ├─ 近景用シャドウマップ
  ├─ 中景用シャドウマップ
  └─ 遠景用シャドウマップ

ライト3: 追加の光源
  ├─ 近景用シャドウマップ
  ├─ 中景用シャドウマップ
  └─ 遠景用シャドウマップ
```

各ライトが独立して影を落とすため、それぞれに専用のシャドウマップが必要です。

##### 2. **カスケードシャドウマップ（1つのライトに3枚）**

1つのライトに対して3枚のシャドウマップを使用する理由は、**カスケードシャドウマップ** という技術を使用しているためです。

**カスケードシャドウマップとは？**

カメラからの距離に応じて、異なる解像度・範囲のシャドウマップを使い分ける技術です。

```
【カメラからの距離】        【使用するシャドウマップ】     【特徴】

近景（0m～50m）     →    近景用シャドウマップ      高解像度、狭い範囲
												  ↓
											鮮明で詳細な影

中景（50m～200m）   →    中景用シャドウマップ      中解像度、中程度の範囲
												  ↓
											そこそこ鮮明な影

遠景（200m～1000m） →    遠景用シャドウマップ      低解像度、広い範囲
												  ↓
											ぼんやりした影
```

##### 3. **なぜ1枚のシャドウマップではダメなのか？**

**問題1: 解像度の不足**

広範囲をカバーする1枚のシャドウマップで高解像度を維持しようとすると、メモリ使用量が莫大になります。

```
例: 1000m × 1000m の範囲を 4096 × 4096 ピクセルで記録する場合
→ 1ピクセルあたり約24cm（精度が低い！）
```

**問題2: 近くの影がギザギザに見える**

プレイヤーの足元の影など、カメラに近い影が粗く見えてしまいます。

**カスケードシャドウマップの解決策**

```
近景用: 50m × 50m を 2048 × 2048 で記録
→ 1ピクセルあたり約2.4cm（高精度！）

中景用: 200m × 200m を 2048 × 2048 で記録
→ 1ピクセルあたり約9.7cm（そこそこ精度）

遠景用: 1000m × 1000m を 2048 × 2048 で記録
→ 1ピクセルあたり約48cm（遠くなので許容範囲）
```

これにより、**メモリ使用量を抑えつつ、近くの影は鮮明に、遠くの影は控えめに描画** できます。

#### シャドウマップの実際の使用例

```cpp
// RenderingEngine.cpp の InitDeferredLighting() より

// 各ライトの各エリア（近・中・遠）のシャドウマップをテクスチャとして設定
for (int i = 0; i < NUM_DEFERRED_LIGHTING_DIRECTIONAL_LIGHT; i++)  // 4つのライト
{
	for (int areaNo = 0; areaNo < NUM_SHADOW_MAP; areaNo++)       // 3つのエリア
	{
		spriteInitData.m_textures[texNo++] = &m_shadowMapRenders[i].GetShadowMap(areaNo);
	}
}
```

この処理により、ディファードライティング時に **12枚すべてのシャドウマップ** を参照できるようになります。

#### まとめ：シャドウマップの構造

```
【レンダリングエンジン全体】
├─ ShadowMapRender[0] （ライト0用）
│   ├─ シャドウマップ[0] （近景）
│   ├─ シャドウマップ[1] （中景）
│   └─ シャドウマップ[2] （遠景）
│
├─ ShadowMapRender[1] （ライト1用）
│   ├─ シャドウマップ[0] （近景）
│   ├─ シャドウマップ[1] （中景）
│   └─ シャドウマップ[2] （遠景）
│
├─ ShadowMapRender[2] （ライト2用）
│   ├─ シャドウマップ[0] （近景）
│   ├─ シャドウマップ[1] （中景）
│   └─ シャドウマップ[2] （遠景）
│
└─ ShadowMapRender[3] （ライト3用）
	├─ シャドウマップ[0] （近景）
	├─ シャドウマップ[1] （中景）
	└─ シャドウマップ[2] （遠景）

合計: 4 × 3 = 12枚のシャドウマップ
```

**例え**: 映画撮影で、4つの照明（メインライト、サイドライト、バックライトなど）があり、各照明について「近くの影」「中くらいの影」「遠くの影」を別々に記録する感じです。

---

### 2.7 InitDeferredLighting（ディファードライティングの準備）

**ディファードライティング** は、G-Buffer に保存された情報を使って、まとめて光の計算を行う処理です。

```cpp
void RenderingEngine::InitDeferredLighting()
{
	int frameBuffer_w = g_graphicsEngine->GetFrameBufferWidth();
	int frameBuffer_h = g_graphicsEngine->GetFrameBufferHeight();

	// 太陽光（ディレクショナルライト1）の設定
	m_deferredLightingCB.m_light.directionalLight[0].color.x = 4.8f;  // 赤成分
	m_deferredLightingCB.m_light.directionalLight[0].color.y = 4.8f;  // 緑成分
	m_deferredLightingCB.m_light.directionalLight[0].color.z = 4.8f;  // 青成分

	m_deferredLightingCB.m_light.directionalLight[0].direction.x = 1.0f;
	m_deferredLightingCB.m_light.directionalLight[0].direction.y = -1.0f;  // 上から下へ
	m_deferredLightingCB.m_light.directionalLight[0].direction.z = -1.0f;
	m_deferredLightingCB.m_light.directionalLight[0].direction.Normalize();  // 正規化
	m_deferredLightingCB.m_light.directionalLight[0].castShadow = true;  // 影を落とす

	// 第2のディレクショナルライト（別角度からの太陽光）
	m_deferredLightingCB.m_light.directionalLight[1].color.x = 4.8f;
	m_deferredLightingCB.m_light.directionalLight[1].color.y = 4.8f;
	m_deferredLightingCB.m_light.directionalLight[1].color.z = 4.8f;

	m_deferredLightingCB.m_light.directionalLight[1].direction.x = -1.0f;
	m_deferredLightingCB.m_light.directionalLight[1].direction.y = -1.0f;
	m_deferredLightingCB.m_light.directionalLight[1].direction.z = -1.0f;
	m_deferredLightingCB.m_light.directionalLight[1].direction.Normalize();
	m_deferredLightingCB.m_light.directionalLight[1].castShadow = true;

	// 地面からの照り返し（反射光）
	m_deferredLightingCB.m_light.directionalLight[2].color.x = 0.8f;
	m_deferredLightingCB.m_light.directionalLight[2].color.y = 0.8f;
	m_deferredLightingCB.m_light.directionalLight[2].color.z = 0.8f;

	m_deferredLightingCB.m_light.directionalLight[2].direction.x = -1.0f;
	m_deferredLightingCB.m_light.directionalLight[2].direction.y = 1.0f;  // 下から上へ
	m_deferredLightingCB.m_light.directionalLight[2].direction.z = -1.0f;
	m_deferredLightingCB.m_light.directionalLight[2].direction.Normalize();

	// 環境光（全体的な明るさ）
	m_deferredLightingCB.m_light.ambinetLight.x = 0.2f;
	m_deferredLightingCB.m_light.ambinetLight.y = 0.2f;
	m_deferredLightingCB.m_light.ambinetLight.z = 0.2f;

	m_deferredLightingCB.m_light.eyePos = g_camera3D->GetPosition();  // カメラ位置
	m_deferredLightingCB.m_light.specPow = 5.0f;  // スペキュラ（光沢）の強さ

	// ディファードライティング用のスプライトを初期化
	SpriteInitData spriteInitData;

	spriteInitData.m_width = frameBuffer_w;
	spriteInitData.m_height = frameBuffer_h;

	// G-Buffer のテクスチャを設定
	int texNo = 0;
	for (auto& gBuffer : m_gBuffer)
	{
		spriteInitData.m_textures[texNo++] = &gBuffer.GetRenderTargetTexture();
	}

	// ディファードライティング用のシェーダーを指定
	spriteInitData.m_fxFilePath = "Assets/shader/preset/DeferredLighting.fx";
	spriteInitData.m_expandConstantBuffer = &m_deferredLightingCB;
	spriteInitData.m_expandConstantBufferSize = sizeof(m_deferredLightingCB);

	// シャドウマップをテクスチャとして追加
	for (int i = 0; i < NUM_DEFERRED_LIGHTING_DIRECTIONAL_LIGHT; i++)
	{
		for (int areaNo = 0; areaNo < NUM_SHADOW_MAP; areaNo++)
		{
			spriteInitData.m_textures[texNo++] = &m_shadowMapRenders[i].GetShadowMap(areaNo);
		}
	}

	spriteInitData.m_colorBufferFormat[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;

	// スプライトを作成
	m_diferredLightingSprite.Init(spriteInitData);
}
```

#### ライトの種類

- **ディレクショナルライト**: 太陽のような無限遠の光源（平行光）
- **環境光（アンビエントライト）**: 周囲全体を照らす基本的な明るさ
- **スペキュラ**: 物体表面での光の反射（テカリ）

---

### 2.8 m_postEffect.Init（ポストエフェクトの準備）

**ポストエフェクト** は、描画が完了した画像に対して追加で行う画像処理です。

```cpp
m_postEffect.Init(m_mainRenderTarget, m_zprepassRenderTarget);
```

ポストエフェクトの例：
- **ブルーム（グロー）**: 明るい部分を光らせる
- **被写界深度（DOF）**: 手前や奥をぼかす
- **モーションブラー**: 動きのブレを表現
- **色調補正**: 色味を調整

---

## 3. Execute 関数の実行内容

準備が整ったら、毎フレーム（1/60秒ごと）に `Execute()` 関数が呼ばれ、実際の描画が行われます。

```cpp
void RenderingEngine::Execute(RenderContext& rc)
{
	// 1. シャドウマップへの描画
	RenderToShadowMap(rc);

	// 2. ZPrepass
	ZPrepass(rc);

	// 3. G-Bufferへのレンダリング
	RenderToGBuffer(rc);

	// 4. ディファードライティング
	DeferredLighting(rc);

	// 5. ディファードライティングが終わった時点でスナップショットを撮影
	SnapshotMainRenderTarget(rc, EnMainRTSnapshot::enDrawnOpacity);

	// 6. フォワードレンダリング
	ForwardRendering(rc);

	// 7. ポストエフェクトを実行
	m_postEffect.Render(rc, m_mainRenderTarget);

	// 8. メインレンダリングターゲットの内容をフレームバッファにコピー
	CopyMainRenderTargetToFrameBuffer(rc);

	// 9. 登録されている3Dモデルをクリア（次フレームの準備）
	m_renderToGBufferModels.clear();
	m_forwardRenderModels.clear();
	m_zprepassModels.clear();
}
```

### Execute 関数の各ステップを詳しく解説

---

### ステップ1: RenderToShadowMap（シャドウマップへの描画）

```cpp
void RenderingEngine::RenderToShadowMap(RenderContext& rc)
{
	int ligNo = 0;
	for (auto& shadowMapRender : m_shadowMapRenders)
	{
		shadowMapRender.Render(
			rc,
			m_deferredLightingCB.m_light.directionalLight[ligNo].direction
		);
		ligNo++;
	}
}
```

#### 何をしているのか？

各光源（ディレクショナルライト）の視点から見たシーンを描画し、**深度情報（どこに何があるか）** をシャドウマップに記録します。

**例え**: カメラを太陽の位置に置いて、そこから見える景色を撮影する感じです。

---

### ステップ2: ZPrepass（深度の事前取得）

```cpp
void RenderingEngine::ZPrepass(RenderContext& rc)
{
	// レンダリングターゲットとして設定できるまで待つ
	rc.WaitUntilToPossibleSetRenderTarget(m_zprepassRenderTarget);

	// レンダリングターゲットを設定
	rc.SetRenderTargetAndViewport(m_zprepassRenderTarget);

	// レンダリングターゲットをクリア（初期化）
	rc.ClearRenderTargetView(m_zprepassRenderTarget);

	// 全てのモデルを描画（色は描かず、深度だけを記録）
	for (auto& model : m_zprepassModels)
	{
		model->Draw(rc);
	}

	// 描画完了を待つ
	rc.WaitUntilFinishDrawingToRenderTarget(m_zprepassRenderTarget);
}
```

#### 何をしているのか？

各ピクセルについて「カメラからの距離（深度）」だけを記録します。色は描きません。

#### なぜ重要？

この情報を使うことで、後続の処理で「見えないピクセル」をスキップして、**処理を高速化** できます。

**例え**: 絵を描く前に、下書きで「どこに何を描くか」を決める作業です。

---

### ステップ3: RenderToGBuffer（G-Bufferへのレンダリング）

```cpp
void RenderingEngine::RenderToGBuffer(RenderContext& rc)
{
	// レンダリングターゲットをG-Bufferに変更
	RenderTarget* rts[enGBufferNum] = {
		&m_gBuffer[enGBufferAlbedo],        // 0番目: 物体の色
		&m_gBuffer[enGBufferNormal],        // 1番目: 表面の向き
		&m_gBuffer[enGBufferWorldPos],      // 2番目: 3D位置
		&m_gBuffer[enGBufferMetalSmooth],   // 3番目: 金属度・滑らかさ
		&m_gBuffer[enGBUfferShadowParam],   // 4番目: 影パラメータ
	};

	// レンダリングターゲットとして設定できるまで待つ
	rc.WaitUntilToPossibleSetRenderTargets(ARRAYSIZE(rts), rts);

	// 複数のレンダリングターゲットを同時に設定
	rc.SetRenderTargets(ARRAYSIZE(rts), rts);

	// 全てのレンダリングターゲットをクリア
	rc.ClearRenderTargetViews(ARRAYSIZE(rts), rts);

	// 全てのモデルを描画
	for (auto& model : m_renderToGBufferModels)
	{
		model->Draw(rc);
	}

	// 描画完了を待つ
	rc.WaitUntilFinishDrawingToRenderTargets(ARRAYSIZE(rts), rts);
}
```

#### 何をしているのか？

各オブジェクトについて、以下の情報を **5枚のテクスチャ（G-Buffer）** に同時に記録します：
- **色（Albedo）**
- **表面の向き（Normal）**
- **3D空間上の位置（WorldPos）**
- **材質情報（MetalSmooth）**
- **影のパラメータ（ShadowParam）**

**例え**: 人物の写真を撮る際に、「顔写真」「身長・体重データ」「服装情報」を同時に記録する感じです。

---

### ステップ4: DeferredLighting（ディファードライティング）

```cpp
void RenderingEngine::DeferredLighting(RenderContext& rc)
{
	// ライト情報を更新
	m_deferredLightingCB.m_light.eyePos = g_camera3D->GetPosition();

	// シャドウマップの変換行列を設定
	for (int i = 0; i < NUM_DEFERRED_LIGHTING_DIRECTIONAL_LIGHT; i++)
	{
		for (int areaNo = 0; areaNo < NUM_SHADOW_MAP; areaNo++)
		{
			m_deferredLightingCB.mlvp[i][areaNo] = m_shadowMapRenders[i].GetLVPMatrix(areaNo);
		}
	}

	// メインレンダリングターゲットを設定
	rc.WaitUntilToPossibleSetRenderTarget(m_mainRenderTarget);
	rc.SetRenderTargetAndViewport(m_mainRenderTarget);

	// G-Bufferの内容を元にしてライティング計算を実行
	m_diferredLightingSprite.Draw(rc);

	// 描画完了を待つ
	rc.WaitUntilFinishDrawingToRenderTarget(m_mainRenderTarget);
}
```

#### 何をしているのか？

G-Buffer に保存された情報（色、法線、位置など）を使って、**全てのライトの影響をまとめて計算** し、最終的な色を決定します。

**例え**: 集めた情報（顔写真、身長など）を使って、最終的な「人物評価レポート」を作成する作業です。

---

### ステップ5: SnapshotMainRenderTarget（スナップショット撮影）

```cpp
void RenderingEngine::SnapshotMainRenderTarget(RenderContext& rc, EnMainRTSnapshot enSnapshot)
{
	// メインレンダリングターゲットの内容をスナップショット
	rc.WaitUntilToPossibleSetRenderTarget(m_mainRTSnapshots[(int)enSnapshot]);
	rc.SetRenderTargetAndViewport(m_mainRTSnapshots[(int)enSnapshot]);
	m_copyMainRtToFrameBufferSprite.Draw(rc);
	rc.WaitUntilFinishDrawingToRenderTarget(m_mainRTSnapshots[(int)enSnapshot]);
}
```

#### 何をしているのか？

この時点でのメインレンダーターゲットの内容を、別のテクスチャにコピーして保存します。

#### なぜ必要？

後続の処理（例: 半透明オブジェクトの描画）で、「この時点での背景」を参照する必要があるためです。

**例え**: 作業途中の絵を写真に撮って保存しておく感じです。

---

### ステップ6: ForwardRendering（フォワードレンダリング）

```cpp
void RenderingEngine::ForwardRendering(RenderContext& rc)
{
	rc.WaitUntilToPossibleSetRenderTarget(m_mainRenderTarget);
	rc.SetRenderTarget(
		m_mainRenderTarget.GetRTVCpuDescriptorHandle(),
		m_gBuffer[enGBufferAlbedo].GetDSVCpuDescriptorHandle()  // 深度バッファを共有
	);

	// フォワードレンダリング用のモデルを描画
	for (auto& model : m_forwardRenderModels)
	{
		model->Draw(rc);
	}

	// 描画完了を待つ
	rc.WaitUntilFinishDrawingToRenderTarget(m_mainRenderTarget);
}
```

#### 何をしているのか？

**フォワードレンダリング** は、従来型の描画方法で、**半透明なオブジェクト** や **特殊なエフェクト** を描画します。

#### なぜディファードレンダリングではないのか？

半透明オブジェクトは、「背後の色」と「自分の色」を混ぜる必要があるため、G-Buffer には保存できません。そのため、通常の描画方法を使います。

**例え**: 基本的な絵が完成した後に、透明なガラスや炎などを追加で描く感じです。

---

### ステップ7: m_postEffect.Render（ポストエフェクト実行）

```cpp
m_postEffect.Render(rc, m_mainRenderTarget);
```

#### 何をしているのか？

完成した画像に対して、追加の画像処理を施します。

ポストエフェクトの例：
- **ブルーム**: 明るい部分を光らせる
- **被写界深度**: ピントが合っていない部分をぼかす
- **トーンマッピング**: HDR画像を通常のディスプレイで表示できるように調整

**例え**: 完成した写真に、フィルターやエフェクトを追加する Instagram のような処理です。

---

### ステップ8: CopyMainRenderTargetToFrameBuffer（フレームバッファへコピー）

```cpp
void RenderingEngine::CopyMainRenderTargetToFrameBuffer(RenderContext& rc)
{
	// フレームバッファをレンダリングターゲットに設定
	rc.SetRenderTarget(
		g_graphicsEngine->GetCurrentFrameBuffuerRTV(),
		g_graphicsEngine->GetCurrentFrameBuffuerDSV()
	);

	// ビューポート（描画領域）を指定
	D3D12_VIEWPORT viewport;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = 1280;   // 画面の幅
	viewport.Height = 720;   // 画面の高さ
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	rc.SetViewportAndScissor(viewport);

	// メインレンダーターゲットの内容をフレームバッファにコピー
	m_copyMainRtToFrameBufferSprite.Draw(rc);
}
```

#### 何をしているのか？

メインレンダーターゲット（内部的な作業用キャンバス）の内容を、**フレームバッファ（モニターに表示される最終画像）** にコピーします。

**例え**: 作業用のキャンバスで描いた絵を、額縁に入れて展示する感じです。

---

### ステップ9: モデルリストのクリア

```cpp
// 登録されている3Dモデルをクリア（次フレームの準備）
m_renderToGBufferModels.clear();
m_forwardRenderModels.clear();
m_zprepassModels.clear();
```

#### 何をしているのか？

今回のフレームで描画したモデルのリストをクリアして、**次のフレームの準備** をします。

ゲームでは、毎フレーム描画するオブジェクトが変わる可能性があるため、毎回リストを作り直します。

**例え**: 撮影が終わったら、スタジオを片付けて次の撮影の準備をする感じです。

---

## 4. まとめ

### Execute 関数の処理フロー（全体像）

```
準備段階（Init関数）
  ↓
[1] シャドウマップ描画 ← 影の情報を記録
  ↓
[2] ZPrepass ← 深度情報だけを記録（高速化のため）
  ↓
[3] G-Buffer描画 ← 色・法線・位置などを記録
  ↓
[4] ディファードライティング ← G-Bufferを使って光を計算
  ↓
[5] スナップショット撮影 ← 現在の画像を保存
  ↓
[6] フォワードレンダリング ← 半透明オブジェクトなどを追加
  ↓
[7] ポストエフェクト ← 画像処理（ブルーム、ぼかしなど）
  ↓
[8] フレームバッファへコピー ← モニターに表示
  ↓
[9] クリーンアップ ← 次フレームの準備
```

### DirectX とレンダリングエンジンの役割

- **DirectX**: GPU を操作するための低レベル API（道具箱）
- **レンダリングエンジン**: DirectX を使って、複雑な描画処理を効率的に管理するシステム（制御塔）

### 重要な概念の振り返り

| 概念 | 説明 |
|------|------|
| **レンダーターゲット** | GPU が描画結果を書き込むメモリ領域 |
| **G-Buffer** | ディファードレンダリングで使う情報の保管庫 |
| **シャドウマップ** | 影を描画するための深度情報 |
| **ZPrepass** | 深度情報だけを事前に取得して高速化 |
| **ディファードレンダリング** | 情報を先に集めて、後でまとめて光の計算を行う手法 |
| **フォワードレンダリング** | 従来型の描画方法（半透明オブジェクトなどに使用） |
| **ポストエフェクト** | 完成した画像に追加で行う画像処理 |
| **フレームバッファ** | モニターに表示される最終画像を保持するメモリ |

---

## おわりに

この `RenderingEngine` は、現代的なゲームエンジンで使われる高度な技術（ディファードレンダリング、シャドウマップ、ポストエフェクト）を実装した、非常に洗練されたシステムです。

一見複雑に見えますが、各ステップを分解して理解すると、**「情報を集める → 計算する → 加工する → 表示する」** という明確な流れがあることが分かります。

この解説が、DirectX やレンダリングエンジンの理解の助けになれば幸いです！
