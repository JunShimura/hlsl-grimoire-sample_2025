# ModelRender 詳細解説：Main.cpp と RenderingEngine.cpp との関連

## はじめに

この解説では、`ModelRender.cpp` のメソッドの実行順序と処理内容を、`Main.cpp` と `RenderingEngine.cpp` との関連を示しながら詳しく説明します。

DirectX やレンダリングエンジンの初心者でも理解できるように、プログラムの全体の流れを追いながら解説します。

---

## 目次

1. [全体の流れ：3つのファイルの役割](#1-全体の流れ3つのファイルの役割)
2. [Main.cpp：プログラムの開始点](#2-maincppプログラムの開始点)
3. [ModelRender.cpp：モデル描画の管理者](#3-modelrendercppモデル描画の管理者)
4. [実行順序の詳細フロー](#4-実行順序の詳細フロー)
5. [まとめ：3つのファイルの連携](#5-まとめ3つのファイルの連携)

---

## 1. 全体の流れ：3つのファイルの役割

### ファイルの役割分担

ゲームエンジンは、役割ごとにファイルを分けて管理します。この3つのファイルは以下のような関係になっています。

```
┌─────────────────────────────────────────────────────────┐
│                      Main.cpp                           │
│            （監督：全体の進行を管理）                      │
│  ・プログラムの開始と終了                                  │
│  ・初期化の指示                                           │
│  ・毎フレームのゲームループ                                │
└────────────────┬────────────────────────────────────────┘
				 │
				 │ 使用する
				 ↓
┌─────────────────────────────────────────────────────────┐
│                  ModelRender.cpp                        │
│              （俳優：3Dモデルを演じる）                     │
│  ・3Dモデルの初期化                                        │
│  ・描画の登録（どこに・どう描くか）                          │
│  ・位置・回転・拡大の管理                                   │
└────────────────┬────────────────────────────────────────┘
				 │
				 │ 登録する
				 ↓
┌─────────────────────────────────────────────────────────┐
│                RenderingEngine.cpp                      │
│            （撮影スタッフ：実際に描画を行う）                │
│  ・シャドウマップの描画                                     │
│  ・G-Bufferへの描画                                        │
│  ・ライティング計算                                         │
│  ・ポストエフェクト                                         │
└─────────────────────────────────────────────────────────┘
```

### 役割の比喩

| ファイル | 役割 | 映画制作での例え |
|---------|------|-----------------|
| **Main.cpp** | プログラム全体の制御 | 映画監督（撮影の進行を管理） |
| **ModelRender.cpp** | 個々の3Dモデルの管理 | 俳優（演技をする人） |
| **RenderingEngine.cpp** | 描画システム全体の実行 | 撮影スタッフ（カメラ・照明担当） |

---

## 2. Main.cpp：プログラムの開始点

### 2.1 プログラムの構造

`Main.cpp` は、Windows アプリケーションの **エントリーポイント（開始地点）** です。

```cpp
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	// ゲームの初期化
	InitGame(hInstance, hPrevInstance, lpCmdLine, nCmdShow, TEXT("Game"));

	// 初期化フェーズ
	// ...

	// ゲームループ
	while (DispatchWindowMessage())
	{
		// 毎フレーム実行される処理
	}

	return 0;
}
```

プログラムは大きく3つのフェーズに分かれています：

1. **初期化フェーズ**：ゲーム開始前の準備
2. **ゲームループ**：毎フレーム（1/60秒ごと）繰り返される処理
3. **終了処理**：ゲーム終了時のクリーンアップ

---

### 2.2 初期化フェーズ（ゲーム開始前の準備）

```cpp
//////////////////////////////////////
// ここから初期化を行うコードを記述する
//////////////////////////////////////

// ルートシグネチャを作成
RootSignature rootSignature;
InitRootSignature(rootSignature);

//レンダリングエンジンを初期化
myRenderer::RenderingEngine renderingEngine;
renderingEngine.Init();

// 背景モデルのレンダラーを初期化
myRenderer::ModelRender bgModelRender;
bgModelRender.InitDeferredRendering(renderingEngine, "Assets/modelData/bg/bg.tkm", true);

// ティーポットの描画処理を初期化する（step-1のコメント部分）
// teapotModelRender.UpdateWorldMatrix({ 0.0f, 50.0f, 0.0f }, g_quatIdentity, g_vec3One);

//////////////////////////////////////
// 初期化を行うコードを書くのはここまで！！！
//////////////////////////////////////
```

#### 実行される処理を順番に解説

---

#### ステップ1: ルートシグネチャの作成

```cpp
RootSignature rootSignature;
InitRootSignature(rootSignature);
```

**ルートシグネチャ（Root Signature）** とは？

DirectX 12 における「シェーダーとデータの受け渡し契約書」のようなものです。

**詳しく説明すると：**
- GPU（グラフィックスカード）で動くプログラム（シェーダー）に、どんなデータを渡すかを定義
- 例：テクスチャ、ライトの情報、カメラの位置など

```cpp
void InitRootSignature(RootSignature& rs)
{
	rs.Init(
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,      // テクスチャフィルタリング（拡大・縮小時の補間方法）
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,      // テクスチャの繰り返し方法（U方向）
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,      // テクスチャの繰り返し方法（V方向）
		D3D12_TEXTURE_ADDRESS_MODE_WRAP       // テクスチャの繰り返し方法（W方向）
	);
}
```

**例え**: レストランで注文する前に、「どんなメニューを提供できるか」を決めるメニュー表を作る作業です。

---

#### ステップ2: レンダリングエンジンの初期化

```cpp
myRenderer::RenderingEngine renderingEngine;
renderingEngine.Init();
```

**レンダリングエンジン** を初期化します。この処理は、前回の解説（`RenderingEngine_Execute関数詳細解説.md`）で説明した以下の準備を行います：

- シャドウマップ用のレンダーターゲット作成
- G-Buffer 用のレンダーターゲット作成
- メインレンダーターゲット作成
- ディファードライティングの設定
- ポストエフェクトの準備

**例え**: 映画の撮影スタッフが、カメラ・照明・マイクなどの機材をセットアップする作業です。

---

#### ステップ3: ModelRender の初期化

```cpp
myRenderer::ModelRender bgModelRender;
bgModelRender.InitDeferredRendering(renderingEngine, "Assets/modelData/bg/bg.tkm", true);
```

ここで **`ModelRender`** クラスが登場します。

**`ModelRender` とは？**

個々の3Dモデル（キャラクター、背景、アイテムなど）を管理するクラスです。

**この行で行われること：**
1. `bgModelRender` という名前の背景モデル用のレンダラーを作成
2. `InitDeferredRendering` を呼び出して初期化
   - 第1引数：レンダリングエンジンへの参照
   - 第2引数：モデルファイルのパス（`bg.tkm`）
   - 第3引数：影を受けるかどうか（`true` = 受ける）

**例え**: 俳優（3Dモデル）を撮影スタジオ（レンダリングエンジン）に招待し、出演準備をさせる作業です。

---

#### ステップ4: ワールド行列の更新

```cpp
teapotModelRender.UpdateWorldMatrix({ 0.0f, 50.0f, 0.0f }, g_quatIdentity, g_vec3One);
```

**ワールド行列（World Matrix）** とは、3D空間でのモデルの **位置・回転・拡大率** を表す数学的なデータです。

**この行の意味：**
- 位置：`{ 0.0f, 50.0f, 0.0f }` → X=0, Y=50, Z=0 の位置に配置
- 回転：`g_quatIdentity` → 回転なし（クォータニオンの単位元）
- 拡大率：`g_vec3One` → 等倍（拡大・縮小なし）

**例え**: 舞台の上で、俳優を「どこに立たせるか」「どの方向を向かせるか」を指示する作業です。

---

### 2.3 ゲームループ（毎フレームの処理）

```cpp
// ここからゲームループ
while (DispatchWindowMessage())
{
	// レンダリング開始
	g_engine->BeginFrame();

	// カメラ操作（ゲームパッドの入力）
	g_camera3D->MoveForward(g_pad[0]->GetLStickYF());
	g_camera3D->MoveRight(g_pad[0]->GetLStickXF());
	g_camera3D->MoveUp(g_pad[0]->GetRStickYF());

	//////////////////////////////////////
	// ここから絵を描くコードを記述する
	//////////////////////////////////////

	bgModelRender.Draw();

	// step-2 ティーポットを描画する（コメント部分）

	//レンダリングエンジンを実行
	renderingEngine.Execute(renderContext);

	/////////////////////////////////////////
	// 絵を描くコードを書くのはここまで！！！
	//////////////////////////////////////

	// レンダリング終了
	g_engine->EndFrame();
}
```

#### ゲームループの流れ

```
1. BeginFrame()     ← フレームの開始を宣言
   ↓
2. カメラ操作       ← プレイヤーの入力を処理
   ↓
3. bgModelRender.Draw() ← モデルを描画登録（★重要★）
   ↓
4. renderingEngine.Execute() ← 実際に描画を実行（★重要★）
   ↓
5. EndFrame()       ← フレームの終了を宣言（画面に表示）
```

#### 重要なポイント

**`Draw()` と `Execute()` の違い**

| メソッド | 役割 | 実行する内容 |
|---------|------|-------------|
| `bgModelRender.Draw()` | **描画の予約** | 「このモデルを描いてください」とエンジンに登録 |
| `renderingEngine.Execute()` | **描画の実行** | 登録されたモデルを実際に描画 |

**例え**: 
- `Draw()` = レストランで注文する（「これを作ってください」と伝える）
- `Execute()` = キッチンで実際に料理を作る

---

## 3. ModelRender.cpp：モデル描画の管理者

### 3.1 ModelRender の役割

`ModelRender` クラスは、1つの3Dモデルを様々な描画パスで描画するための **マルチパス対応モデルマネージャー** です。

#### なぜ複数のモデルが必要なのか？

現代のゲームエンジンでは、1つのオブジェクトを描画するために、**異なるシェーダー（GPU プログラム）** を使った複数の描画が必要です。

```
同じ「ティーポット」でも...

1. シャドウマップ用      ← 影を作るための深度情報を描画
2. ZPrepass用          ← 深度情報だけを事前に描画
3. G-Buffer用          ← 色・法線・材質情報を描画
4. フォワードレンダリング用 ← 特殊なシェーディング用
```

`ModelRender` は、これらの **4種類のモデルを内部で管理** します。

---

### 3.2 ModelRender のメンバ変数

```cpp
private:
	RenderingEngine* m_renderingEngine = nullptr;   // レンダリングエンジンへの参照
	Model m_zprepassModel;                          // ZPrepass で描画されるモデル
	Model m_forwardRenderModel;                     // フォワードレンダリング用のモデル
	Model m_renderToGBufferModel;                   // G-Buffer への描画用のモデル
	Model m_shadowModels[NUM_DEFERRED_LIGHTING_DIRECTIONAL_LIGHT][NUM_SHADOW_MAP];  // シャドウマップ用のモデル（複数）
	bool m_isShadowCaster = false;                  // 影を落とすかどうかのフラグ
```

#### 各モデルの役割

| メンバ変数 | 用途 | シェーダー | 描画内容 |
|-----------|------|-----------|---------|
| `m_shadowModels` | 影の生成 | `DrawShadowMap.fx` | 光源からの深度情報 |
| `m_zprepassModel` | 深度の事前取得 | `ZPrepass.fx` | カメラからの深度情報のみ |
| `m_renderToGBufferModel` | G-Bufferへの描画 | `RenderToGBufferFor3DModel.fx` | 色・法線・位置・材質 |
| `m_forwardRenderModel` | フォワードレンダリング | ユーザー指定 | 特殊なシェーディング |

---

### 3.3 InitDeferredRendering メソッド（初期化）

```cpp
void ModelRender::InitDeferredRendering(RenderingEngine& renderingEngine, const char* tkmFilePath, bool isShadowReciever)
{
	ModelInitData modelInitData;

	// G-Buffer描画用のシェーダーを指定
	modelInitData.m_fxFilePath = "Assets/shader/preset/RenderToGBufferFor3DModel.fx";

	// 影を受ける場合は、専用のピクセルシェーダーを指定
	if (isShadowReciever)
	{
		modelInitData.m_psEntryPointFunc = "PSMainShadowReciever";
	}

	// モデルファイルのパスを設定
	modelInitData.m_tkmFilePath = tkmFilePath;

	// レンダーターゲットのフォーマットを設定（G-Bufferの各バッファに対応）
	modelInitData.m_colorBufferFormat[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;  // Albedo（色）
	modelInitData.m_colorBufferFormat[1] = DXGI_FORMAT_R8G8B8A8_UNORM;      // Normal（法線）
	modelInitData.m_colorBufferFormat[2] = DXGI_FORMAT_R32G32B32A32_FLOAT;  // WorldPos（位置）
	modelInitData.m_colorBufferFormat[3] = DXGI_FORMAT_R8G8B8A8_UNORM;      // MetalSmooth（材質）
	modelInitData.m_colorBufferFormat[4] = DXGI_FORMAT_R8G8B8A8_UNORM;      // ShadowParam（影パラメータ）

	// G-Buffer描画用のモデルを初期化
	m_renderToGBufferModel.Init(modelInitData);

	// 共通の初期化処理を呼び出す
	InitCommon(renderingEngine, tkmFilePath);
}
```

#### このメソッドの役割

**ディファードレンダリング用のモデルを初期化** します。

1. **G-Buffer描画用のモデル** を作成
2. 5つのレンダーターゲット（色・法線・位置・材質・影）に対応
3. 影を受けるかどうかで、異なるピクセルシェーダーを使用

**例え**: 俳優に「PBRライティング（物理ベースレンダリング）を使った演技をしてください」と指示し、専用の衣装（シェーダー）を着せる作業です。

---

### 3.4 InitCommon メソッド（共通初期化）

```cpp
void ModelRender::InitCommon(RenderingEngine& renderingEngine, const char* tkmFilePath)
{
	// レンダリングエンジンへの参照を保存
	m_renderingEngine = &renderingEngine;

	// ZPrepass描画用のモデルを初期化
	{
		ModelInitData modelInitData;
		modelInitData.m_tkmFilePath = tkmFilePath;
		modelInitData.m_fxFilePath = "Assets/shader/preset/ZPrepass.fx";
		modelInitData.m_colorBufferFormat[0] = DXGI_FORMAT_R32G32_FLOAT;

		m_zprepassModel.Init(modelInitData);
	}

	// シャドウマップ描画用のモデルを初期化
	{
		ModelInitData modelInitData;
		modelInitData.m_tkmFilePath = tkmFilePath;
		modelInitData.m_fxFilePath = "Assets/shader/preset/DrawShadowMap.fx";
		modelInitData.m_colorBufferFormat[0] = DXGI_FORMAT_R32_FLOAT;

		// 複数の光源 × 複数のシャドウマップ領域に対応
		for (int ligNo = 0; ligNo < NUM_DEFERRED_LIGHTING_DIRECTIONAL_LIGHT; ligNo++)
		{
			m_shadowModels[ligNo][0].Init(modelInitData);  // 近景用シャドウマップ
			m_shadowModels[ligNo][1].Init(modelInitData);  // 中景用シャドウマップ
			m_shadowModels[ligNo][2].Init(modelInitData);  // 遠景用シャドウマップ
		}
	}
}
```

#### このメソッドの役割

**ZPrepass用とシャドウマップ用のモデル** を初期化します。

**シャドウマップが複数ある理由**

影の品質を向上させるために、**カスケードシャドウマップ** という技術を使用しています。

```
近景（カメラに近い）  → 高解像度のシャドウマップ
中景（中くらいの距離） → 中解像度のシャドウマップ
遠景（カメラから遠い） → 低解像度のシャドウマップ
```

これにより、近くの影は鮮明に、遠くの影は控えめに描画され、メモリと処理時間を節約できます。

**例え**: 俳優に「近くで撮影する用の衣装」「遠くで撮影する用の簡易衣装」など、複数の衣装を用意させる作業です。

---

### 3.5 Draw メソッド（描画登録）

```cpp
void ModelRender::Draw()
{
	// シャドウキャスター（影を落とすオブジェクト）の場合
	if (m_isShadowCaster)
	{
		for (int ligNo = 0; ligNo < NUM_DEFERRED_LIGHTING_DIRECTIONAL_LIGHT; ligNo++)
		{
			// シャドウマップ描画パスにモデルを追加
			m_renderingEngine->Add3DModelToRenderToShadowMap(
				ligNo,
				m_shadowModels[ligNo][0],  // 近景用
				m_shadowModels[ligNo][1],  // 中景用
				m_shadowModels[ligNo][2]   // 遠景用
			);
		}
	}

	// ZPrepass描画パスにモデルを追加
	m_renderingEngine->Add3DModelToZPrepass(m_zprepassModel);

	// G-Buffer描画パスにモデルを追加（初期化済みの場合）
	if (m_renderToGBufferModel.IsInited())
	{
		m_renderingEngine->Add3DModelToRenderGBufferPass(m_renderToGBufferModel);
	}

	// フォワードレンダリング描画パスにモデルを追加（初期化済みの場合）
	if (m_forwardRenderModel.IsInited())
	{
		m_renderingEngine->Add3DModelToForwardRenderPass(m_forwardRenderModel);
	}
}
```

#### このメソッドの役割

**各描画パスにモデルを登録** します。実際の描画は行わず、「このモデルを描画してください」という **予約** だけを行います。

#### 登録される順序

```
1. シャドウマップ描画パス   （影を作る）
   ↓
2. ZPrepass描画パス        （深度を記録）
   ↓
3. G-Buffer描画パス        （色・法線・材質を記録）
   ↓
4. フォワードレンダリングパス （特殊なシェーディング）
```

**重要**: この時点では **まだ描画されません**。後で `renderingEngine.Execute()` が呼ばれたときに、初めて実際の描画が行われます。

**例え**: レストランで注文を伝える作業です。キッチンに「これを作ってください」と伝えるだけで、まだ料理は作られていません。

---

### 3.6 UpdateWorldMatrix メソッド（位置更新）

```cpp
void UpdateWorldMatrix(Vector3 pos, Quaternion rot, Vector3 scale)
{
	// ZPrepassモデルの位置を更新
	m_zprepassModel.UpdateWorldMatrix(pos, rot, scale);

	// G-Bufferモデルの位置を更新（初期化済みの場合）
	if (m_renderToGBufferModel.IsInited())
	{
		m_renderToGBufferModel.UpdateWorldMatrix(pos, rot, scale);
	}

	// フォワードレンダリングモデルの位置を更新（初期化済みの場合）
	if (m_forwardRenderModel.IsInited())
	{
		m_forwardRenderModel.UpdateWorldMatrix(pos, rot, scale);
	}
}
```

#### このメソッドの役割

**全ての内部モデルの位置・回転・拡大率を一括更新** します。

#### なぜ全てのモデルを更新する必要があるのか？

`ModelRender` は内部で複数のモデル（ZPrepass用、G-Buffer用、フォワードレンダリング用）を持っています。これらは **同じ3Dオブジェクトの異なる描画方法** なので、位置は全て同じでなければなりません。

**例え**: 同じ俳優が、異なるカメラ（シャドウマップ用、本番用など）で撮影される際に、全てのカメラに対して同じ位置に立つ必要がある、という状況です。

---

## 4. 実行順序の詳細フロー

### 4.1 プログラム起動から描画までの完全フロー

```
【起動時（1回だけ実行）】

1. Main.cpp: wWinMain() が呼ばれる
   ↓
2. Main.cpp: InitGame() でウィンドウを作成
   ↓
3. Main.cpp: InitRootSignature() でルートシグネチャを初期化
   ↓
4. Main.cpp: renderingEngine.Init() を呼ぶ
   ↓
5. RenderingEngine.cpp: Init() が実行される
   - シャドウマップ用レンダーターゲット作成
   - G-Buffer 用レンダーターゲット作成
   - メインレンダーターゲット作成
   - ディファードライティング設定
   - ポストエフェクト準備
   ↓
6. Main.cpp: bgModelRender.InitDeferredRendering() を呼ぶ
   ↓
7. ModelRender.cpp: InitDeferredRendering() が実行される
   ↓
8. ModelRender.cpp: InitCommon() が実行される
   - ZPrepass用モデルを初期化
   - シャドウマップ用モデルを初期化（複数）
   ↓
9. Main.cpp: bgModelRender.UpdateWorldMatrix() を呼ぶ
   ↓
10. ModelRender.cpp: UpdateWorldMatrix() が実行される
	- 全ての内部モデルの位置を更新

【ここまでが初期化フェーズ】
```

---

### 4.2 ゲームループ（毎フレーム実行）

```
【毎フレーム（1/60秒ごとに実行）】

1. Main.cpp: while (DispatchWindowMessage()) のループ開始
   ↓
2. Main.cpp: g_engine->BeginFrame()
   - フレームの開始を宣言
   - GPU コマンドリストの準備
   ↓
3. Main.cpp: カメラ操作の処理
   - ゲームパッドの入力を取得
   - カメラの位置を更新
   ↓
4. Main.cpp: bgModelRender.Draw() を呼ぶ ★重要★
   ↓
5. ModelRender.cpp: Draw() が実行される
   - シャドウマップ描画パスに登録
   - ZPrepass描画パスに登録
   - G-Buffer描画パスに登録
   - （この時点ではまだ描画されない）
   ↓
6. Main.cpp: renderingEngine.Execute(renderContext) を呼ぶ ★重要★
   ↓
7. RenderingEngine.cpp: Execute() が実行される

   7-1. RenderToShadowMap()
		- 登録されたモデルをシャドウマップに描画
   ↓
   7-2. ZPrepass()
		- 登録されたモデルの深度情報を描画
   ↓
   7-3. RenderToGBuffer()
		- 登録されたモデルの色・法線・位置・材質を描画
   ↓
   7-4. DeferredLighting()
		- G-Bufferを使ってライティング計算
   ↓
   7-5. SnapshotMainRenderTarget()
		- 現在の画像をスナップショット
   ↓
   7-6. ForwardRendering()
		- フォワードレンダリング用のモデルを描画
   ↓
   7-7. m_postEffect.Render()
		- ポストエフェクトを適用
   ↓
   7-8. CopyMainRenderTargetToFrameBuffer()
		- 最終画像をフレームバッファにコピー
   ↓
   7-9. モデルリストをクリア
		- 次フレームの準備
   ↓
8. Main.cpp: g_engine->EndFrame()
   - GPU コマンドを実行
   - 画面に表示（Present）
   ↓
9. ループの先頭に戻る（次のフレームへ）
```

---

### 4.3 タイムライン図

```
時間軸 →

初期化フェーズ（プログラム起動時に1回だけ）
├─ Main.cpp: InitGame()
├─ Main.cpp: InitRootSignature()
├─ RenderingEngine.cpp: Init()
│   ├─ シャドウマップ準備
│   ├─ G-Buffer準備
│   ├─ メインレンダーターゲット準備
│   └─ ポストエフェクト準備
├─ ModelRender.cpp: InitDeferredRendering()
│   ├─ G-Buffer用モデル初期化
│   └─ InitCommon()
│       ├─ ZPrepass用モデル初期化
│       └─ シャドウマップ用モデル初期化
└─ ModelRender.cpp: UpdateWorldMatrix()

ゲームループ（1/60秒ごとに繰り返し）
│
├─ フレーム1 ───────────────────────────
│   ├─ BeginFrame()
│   ├─ カメラ操作
│   ├─ ModelRender.Draw()      ← モデルを登録
│   ├─ RenderingEngine.Execute() ← 実際に描画
│   │   ├─ シャドウマップ描画
│   │   ├─ ZPrepass
│   │   ├─ G-Buffer描画
│   │   ├─ ディファードライティング
│   │   ├─ フォワードレンダリング
│   │   ├─ ポストエフェクト
│   │   └─ フレームバッファにコピー
│   └─ EndFrame()            ← 画面に表示
│
├─ フレーム2 ───────────────────────────
│   ├─ BeginFrame()
│   ├─ カメラ操作
│   ├─ ModelRender.Draw()
│   ├─ RenderingEngine.Execute()
│   └─ EndFrame()
│
├─ フレーム3 ───────────────────────────
│   ...
```

---

## 5. まとめ：3つのファイルの連携

### 5.1 役割の整理

| ファイル | 役割 | 主な処理 |
|---------|------|---------|
| **Main.cpp** | プログラム全体の制御 | ・初期化の指示<br>・ゲームループの管理<br>・`Draw()` と `Execute()` の呼び出し |
| **ModelRender.cpp** | 個々の3Dモデルの管理 | ・複数の描画パス用モデルの初期化<br>・描画の登録（予約）<br>・位置・回転の管理 |
| **RenderingEngine.cpp** | 描画システム全体の実行 | ・登録されたモデルの実際の描画<br>・シャドウマップ・G-Buffer・ライティング<br>・ポストエフェクト |

---

### 5.2 データの流れ

```
Main.cpp
   │
   │ [1] 初期化指示
   ↓
RenderingEngine.Init()
   ↓
   [準備完了: レンダーターゲットなどが用意される]


Main.cpp
   │
   │ [2] モデル初期化指示
   ↓
ModelRender.InitDeferredRendering(renderingEngine, ...)
   │
   │ [3] レンダリングエンジンへの参照を保存
   ↓
   [準備完了: 複数の描画パス用モデルが初期化される]


【毎フレーム】

Main.cpp: bgModelRender.Draw()
   │
   │ [4] モデルを各描画パスに登録
   ↓
RenderingEngine
   ├─ m_shadowMapRenders[0] に登録
   ├─ m_zprepassModels に登録
   ├─ m_renderToGBufferModels に登録
   └─ m_forwardRenderModels に登録

Main.cpp: renderingEngine.Execute(renderContext)
   │
   │ [5] 登録されたモデルを実際に描画
   ↓
RenderingEngine.Execute()
   ├─ [6] RenderToShadowMap() → シャドウマップに描画
   ├─ [7] ZPrepass() → 深度情報を描画
   ├─ [8] RenderToGBuffer() → G-Bufferに描画
   ├─ [9] DeferredLighting() → ライティング計算
   ├─ [10] ForwardRendering() → フォワードレンダリング
   ├─ [11] PostEffect() → ポストエフェクト
   └─ [12] CopyToFrameBuffer() → 画面に表示
```

---

### 5.3 重要な設計パターン

このコードは、**コマンドパターン（Command Pattern）** という設計パターンを使用しています。

#### コマンドパターンとは？

「実行してほしい処理」を **登録（記録）** して、後でまとめて **実行** する設計です。

```
通常の方法（即座に実行）:
ModelRender.Draw() → すぐに描画される

コマンドパターン（登録してから実行）:
ModelRender.Draw() → 「描画してください」と登録だけ
RenderingEngine.Execute() → 登録された内容をまとめて実行
```

#### なぜこの設計が良いのか？

**利点1: 描画順序の最適化**
- 全てのモデルを登録してから描画することで、GPU に効率的な順序で命令を送れる

**利点2: マルチスレッド対応**
- 複数のスレッドから同時に `Draw()` を呼べる（登録だけなので安全）
- 実際の描画は1つのスレッドで行う

**利点3: レンダリングパスの集中管理**
- どのモデルをどのタイミングで描くかを、`RenderingEngine` が一元管理できる

---

### 5.4 実際のゲームでの使用例

```cpp
// キャラクター100体を描画する例

// 初期化（ゲーム開始時に1回だけ）
RenderingEngine renderingEngine;
renderingEngine.Init();

std::vector<ModelRender> characters(100);
for (int i = 0; i < 100; i++)
{
	characters[i].InitDeferredRendering(renderingEngine, "character.tkm", true);
	characters[i].UpdateWorldMatrix({i * 2.0f, 0.0f, 0.0f}, ...);
}

// ゲームループ（毎フレーム）
while (gameIsRunning)
{
	BeginFrame();

	// 全てのキャラクターを登録
	for (auto& character : characters)
	{
		character.Draw();  // ← 100回呼ばれる（登録だけ）
	}

	// まとめて描画
	renderingEngine.Execute(renderContext);  // ← 1回だけ呼ばれる（実際の描画）

	EndFrame();
}
```

---

## おわりに

### 学んだこと

1. **Main.cpp** は、プログラム全体の「監督」として、初期化とゲームループを管理
2. **ModelRender.cpp** は、個々の3Dモデルの「俳優」として、複数の描画パス用のモデルを管理
3. **RenderingEngine.cpp** は、「撮影スタッフ」として、登録されたモデルを実際に描画
4. **コマンドパターン** により、描画の登録と実行を分離し、効率的なレンダリングを実現

### 全体の流れの比喩

```
映画撮影の流れに例えると...

初期化フェーズ:
- Main.cpp: 監督が撮影スタジオを借りる
- RenderingEngine.Init(): スタッフがカメラ・照明をセットアップ
- ModelRender.Init(): 俳優が衣装を準備

ゲームループ:
- ModelRender.Draw(): 俳優が「私を撮影してください」と手を挙げる
- RenderingEngine.Execute(): スタッフが全ての俳優をまとめて撮影
  - シャドウマップ: 影を作るための撮影
  - ZPrepass: 距離測定のための撮影
  - G-Buffer: 詳細情報を記録するための撮影
  - ディファードライティング: 光の計算
  - ポストエフェクト: 映像編集
  - フレームバッファへコピー: 映画館で上映
```

この設計により、複雑な現代のゲームグラフィックスを、効率的かつ管理しやすい形で実装できています！
