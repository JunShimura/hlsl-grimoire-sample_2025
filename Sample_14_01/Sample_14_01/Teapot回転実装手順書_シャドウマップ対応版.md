# Teapot回転実装手順書：シャドウマップ対応版

## 目次
1. [概要](#1-概要)
2. [元のコードの状態](#2-元のコードの状態)
3. [実装の目標](#3-実装の目標)
4. [実装手順](#4-実装手順)
5. [変更の詳細解説](#5-変更の詳細解説)
6. [トラブルシューティング](#6-トラブルシューティング)
7. [動作確認方法](#7-動作確認方法)
8. [応用編](#8-応用編)

---

## 1. 概要

このドキュメントは、Sample_14_01 プロジェクトで、**静止しているティーポットを回転させる実装**を、**シャドウマップへの正しい反映**を含めて行う手順を解説します。

### 実装のポイント
- ティーポットを毎秒0.25回転（4秒で1回転）させる
- シャドウマップにも正しく回転を反映させる
- フレームレート非依存の実装（60FPS想定）

### 変更するファイル
1. `Sample_14_01/main.cpp` - 回転ロジックの実装
2. `Sample_14_01/ModelRender.h` - シャドウマップ用モデルの更新処理追加

---

## 2. 元のコードの状態

### 2.1 元の main.cpp（関連部分）

```cpp
// step-1 ティーポットの描画処理を初期化する
// （元のコードでは、ここに初期化コードがあるはずですが、step-1コメントのみ）

// 初期化時にワールド行列を設定（静止状態）
teapotModelRender.UpdateWorldMatrix({ 0.0f, 50.0f, 0.0f }, g_quatIdentity, g_vec3One);

//////////////////////////////////////
// 初期化を行うコードを書くのはここまで！！！
//////////////////////////////////////
auto& renderContext = g_graphicsEngine->GetRenderContext();

// ここからゲームループ
while (DispatchWindowMessage())
{
	// ... カメラ操作 ...

	bgModelRender.Draw();

	// step-2 ティーポットを描画する
	// （元のコードでは、ここにティーポット描画コードがあるはずですが、step-2コメントのみ）

	//レンダリングエンジンを実行
	renderingEngine.Execute(renderContext);

	g_engine->EndFrame();
}
```

**問題点**:
- ティーポットの初期化コードが不完全（step-1 コメントのみ）
- ゲームループ内で回転処理がない
- ワールド行列の更新が初期化時のみ

---

### 2.2 元の ModelRender.h の UpdateWorldMatrix()

```cpp
void UpdateWorldMatrix(Vector3 pos, Quaternion rot, Vector3 scale)
{
	m_zprepassModel.UpdateWorldMatrix(pos, rot, scale);
	if (m_renderToGBufferModel.IsInited())
	{
		m_renderToGBufferModel.UpdateWorldMatrix(pos, rot, scale);
	}
	if (m_forwardRenderModel.IsInited())
	{
		m_forwardRenderModel.UpdateWorldMatrix(pos, rot, scale);
	}
	// ⚠️ シャドウマップ用のモデル（m_shadowModels）の更新が抜けている！
}
```

**問題点**:
- `m_shadowModels` の更新処理がない
- 回転してもシャドウマップに反映されない

---

## 3. 実装の目標

### 3.1 機能要件
| 項目 | 内容 |
|------|------|
| **回転速度** | 毎秒0.25回転（4秒で1回転） |
| **回転軸** | Y軸（上下軸） |
| **回転方向** | 時計回り（正の角度） |
| **初期位置** | (0.0, 50.0, 0.0) |
| **フレームレート** | 60FPS想定 |

### 3.2 技術要件
| 項目 | 内容 |
|------|------|
| **描画パス** | フォワードレンダリング |
| **シャドウ対応** | シャドウキャスターとして動作 |
| **更新対象モデル** | ZPrepass、Forward、Shadow（12個）の全モデル |

---

## 4. 実装手順

### 【手順1】main.cpp にティーポット初期化コードを追加

**ファイル**: `Sample_14_01/Sample_14_01/main.cpp`

**場所**: step-1 コメントの下（行33付近）

**追加するコード**:
```cpp
// step-1 ティーポットの描画処理を初期化する
myRenderer::ModelInitDataFR modelInitData;
modelInitData.m_tkmFilePath = "Assets/modelData/teapot.tkm";
modelInitData.m_fxFilePath = "Assets/shader/sample.fx";

// 【注目】拡張SRVにZPrepassで作成された深度テクスチャを指定する
modelInitData.m_expandShaderResoruceView[0] = 
	&renderingEngine.GetZPrepassDepthTexture();

// 初期化情報を使って描画処理を初期化する
myRenderer::ModelRender teapotModelRender;

// InitForwardRendering()を利用すると
// フォワードレンダリングの描画パスで描画される
teapotModelRender.InitForwardRendering(renderingEngine, modelInitData);

// シャドウキャスターフラグをONにする
teapotModelRender.SetShadowCasterFlag(true);

// ティーポットの回転角度を初期化（ラジアン）
float teapotRotationAngle = 0.0f;
```

**解説**:
- `ModelInitDataFR` でフォワードレンダリング用の初期化データを作成
- `m_expandShaderResoruceView[0]` にZPrepassの深度テクスチャを設定（重要！）
- `SetShadowCasterFlag(true)` でシャドウキャスターとして登録
- `teapotRotationAngle` で回転角度を管理

---

### 【手順2】main.cpp のゲームループに回転処理を追加

**ファイル**: `Sample_14_01/Sample_14_01/main.cpp`

**場所**: step-2 コメントの下（行75付近）

**追加するコード**:
```cpp
// step-2 ティーポットを描画する
// 毎秒0.25回転（4秒で1回転）させる
// 60FPS想定: 1フレームあたり π/120 ラジアン回転
// 計算式: (2π × 0.25回転/秒) / 60フレーム/秒 = π/120 ラジアン/フレーム
teapotRotationAngle += Math::PI / 120.0f;

// 回転クォータニオンを作成
Quaternion rotY;
rotY.SetRotationY(teapotRotationAngle);

// ティーポットのワールド行列を更新
teapotModelRender.UpdateWorldMatrix({ 0.0f, 50.0f, 0.0f }, rotY, g_vec3One);

teapotModelRender.Draw();
```

**解説**:
- 毎フレーム `Math::PI / 120.0f` ラジアン（1.5度）回転
- 60FPS想定で、60フレーム × 4秒 = 240フレームで1回転（2π）
- `Quaternion` を使って回転を表現
- `UpdateWorldMatrix()` で全モデル（ZPrepass、Forward、Shadow）を更新
- `Draw()` で描画コマンドを登録

---

### 【手順3】ModelRender.h の UpdateWorldMatrix() を修正

**ファイル**: `Sample_14_01/Sample_14_01/ModelRender.h`

**場所**: `UpdateWorldMatrix()` 関数の中（行57-76付近）

**変更前のコード**:
```cpp
void UpdateWorldMatrix(Vector3 pos, Quaternion rot, Vector3 scale)
{
	m_zprepassModel.UpdateWorldMatrix(pos, rot, scale);
	if (m_renderToGBufferModel.IsInited())
	{
		m_renderToGBufferModel.UpdateWorldMatrix(pos, rot, scale);
	}
	if (m_forwardRenderModel.IsInited())
	{
		m_forwardRenderModel.UpdateWorldMatrix(pos, rot, scale);
	}
}
```

**変更後のコード**:
```cpp
void UpdateWorldMatrix(Vector3 pos, Quaternion rot, Vector3 scale)
{
	m_zprepassModel.UpdateWorldMatrix(pos, rot, scale);
	if (m_renderToGBufferModel.IsInited())
	{
		m_renderToGBufferModel.UpdateWorldMatrix(pos, rot, scale);
	}
	if (m_forwardRenderModel.IsInited())
	{
		m_forwardRenderModel.UpdateWorldMatrix(pos, rot, scale);
	}
	// シャドウマップ用のモデルも更新
	for (int ligNo = 0; ligNo < NUM_DEFERRED_LIGHTING_DIRECTIONAL_LIGHT; ligNo++)
	{
		for (int areaNo = 0; areaNo < NUM_SHADOW_MAP; areaNo++)
		{
			m_shadowModels[ligNo][areaNo].UpdateWorldMatrix(pos, rot, scale);
		}
	}
}
```

**解説**:
- `NUM_DEFERRED_LIGHTING_DIRECTIONAL_LIGHT` = 4（ライトの数）
- `NUM_SHADOW_MAP` = 3（カスケードシャドウマップの分割数）
- 合計 4 × 3 = 12 個のシャドウマップ用モデルを更新
- **これがないと、シャドウが静止したまま**になる！

---

### 【手順4】ビルドと動作確認

**ビルド手順**:
1. Visual Studio でソリューションを開く
2. ビルド → ソリューションのビルド
3. エラーがないことを確認

**動作確認**:
1. デバッグ → デバッグなしで開始（Ctrl + F5）
2. ティーポットがY軸回りに回転していることを確認
3. 地面に投影されるシャドウも回転していることを確認

---

## 5. 変更の詳細解説

### 5.1 回転角度の計算

#### 目標: 毎秒0.25回転（4秒で1回転）

```
【前提】
- 60FPS（1秒間に60フレーム）
- 1回転 = 2π ラジアン

【計算】
毎秒の回転角度 = 2π × 0.25 = π/2 ラジアン/秒

1フレームあたりの回転角度 = (π/2) / 60 = π/120 ラジアン/フレーム

【確認】
π/120 × 60フレーム/秒 × 4秒 = π/120 × 240 = 2π ✅
```

#### 度数法での表現

```
π/120 ラジアン = 180° / 120 = 1.5° / フレーム
1.5° × 60フレーム = 90° / 秒
90° × 4秒 = 360° ✅
```

---

### 5.2 Quaternion（クォータニオン）による回転

#### なぜクォータニオンを使うのか？

| 方式 | メリット | デメリット |
|------|---------|----------|
| **オイラー角**（X, Y, Z回転） | 直感的 | ジンバルロック発生 |
| **回転行列** | 高速 | 累積誤差が大きい |
| **クォータニオン** | ✅ ジンバルロックなし<br>✅ 補間が滑らか<br>✅ 累積誤差が少ない | やや難解 |

#### クォータニオンのコード

```cpp
Quaternion rotY;
rotY.SetRotationY(teapotRotationAngle);
```

**内部処理**（推測）:
```cpp
// Y軸回りの回転クォータニオン
float halfAngle = teapotRotationAngle / 2.0f;
rotY.x = 0.0f;
rotY.y = sin(halfAngle);  // Y軸成分
rotY.z = 0.0f;
rotY.w = cos(halfAngle);  // 実部
```

---

### 5.3 UpdateWorldMatrix() の内部動作

#### 呼び出し順序

```
main.cpp: teapotModelRender.UpdateWorldMatrix()
	↓
ModelRender.h: UpdateWorldMatrix()
	├─ m_zprepassModel.UpdateWorldMatrix()         ← ZPrepass用
	├─ m_renderToGBufferModel.UpdateWorldMatrix()  ← G-Buffer用（未使用）
	├─ m_forwardRenderModel.UpdateWorldMatrix()    ← フォワードレンダリング用
	└─ m_shadowModels[4][3].UpdateWorldMatrix()    ← シャドウマップ用（12個）
		  ↓
Model.cpp: UpdateWorldMatrix()
	├─ m_world = Matrix::Identity
	├─ m_world.MakeScaling(scale)
	├─ m_world *= Matrix::MakeRotationFromQuaternion(rot)
	└─ m_world.SetTranslation(pos)
```

#### ワールド行列の構築

```cpp
// Model.cpp の UpdateWorldMatrix() 内部（推測）
void Model::UpdateWorldMatrix(Vector3 pos, Quaternion rot, Vector3 scale)
{
	// スケール行列
	Matrix scaleMatrix = Matrix::MakeScaling(scale);

	// 回転行列
	Matrix rotationMatrix = Matrix::MakeRotationFromQuaternion(rot);

	// 平行移動行列
	Matrix translationMatrix = Matrix::MakeTranslation(pos);

	// ワールド行列 = スケール × 回転 × 平行移動
	m_world = scaleMatrix * rotationMatrix * translationMatrix;
}
```

---

### 5.4 シャドウマップへの反映の仕組み

#### シャドウマップ用モデルの配列

```cpp
// ModelRender.h より
Model m_shadowModels[NUM_DEFERRED_LIGHTING_DIRECTIONAL_LIGHT][NUM_SHADOW_MAP];
```

**インデックスの意味**:
- `ligNo` (0～3): ディレクショナルライトの番号
- `areaNo` (0～2): カスケードシャドウマップの分割番号

#### 具体例: 4つのライトと3つのカスケード

```
m_shadowModels[0][0]  ← ライト0、近距離カスケード
m_shadowModels[0][1]  ← ライト0、中距離カスケード
m_shadowModels[0][2]  ← ライト0、遠距離カスケード

m_shadowModels[1][0]  ← ライト1、近距離カスケード
m_shadowModels[1][1]  ← ライト1、中距離カスケード
m_shadowModels[1][2]  ← ライト1、遠距離カスケード

... 以下、ライト2、ライト3も同様 ...

合計: 4ライト × 3カスケード = 12個のシャドウマップ用モデル
```

#### シャドウマップ描画の流れ

```cpp
// RenderingEngine.cpp の RenderToShadowMap() 内部（推測）
void RenderingEngine::RenderToShadowMap(RenderContext& rc)
{
	for (int ligNo = 0; ligNo < NUM_DEFERRED_LIGHTING_DIRECTIONAL_LIGHT; ligNo++)
	{
		for (int areaNo = 0; areaNo < NUM_SHADOW_MAP; areaNo++)
		{
			// シャドウマップ用のレンダーターゲットを設定
			rc.SetRenderTarget(m_shadowMapRender.GetShadowMap(ligNo, areaNo));

			// ビュープロジェクション行列をライトの視点に設定
			rc.SetViewProjectionMatrix(m_lightViewProjMatrix[ligNo][areaNo]);

			// 登録されたシャドウキャスターを描画
			for (auto& model : m_shadowCasters)
			{
				model->GetShadowModel(ligNo, areaNo).Draw(rc);
				// ↑ ここで m_shadowModels[ligNo][areaNo] が描画される
			}
		}
	}
}
```

**ポイント**:
- 各シャドウマップに対して、ライトの視点から描画
- `UpdateWorldMatrix()` で更新されたワールド行列が使われる
- **更新されていないと、古い位置で描画される**

---

## 6. トラブルシューティング

### 問題1: 固定したFPSでの制御が成立している確証がない


**原因**:
- このエンジンには 時間計測が存在しない

**解決策**:
- フレームレート非依存の実装を諦め、60FPS固定を前提とする
- 毎フレーム固定の角度（`Math::PI / 120.0f`）で回転させる

**代替案**（フレームレート非依存）:
```cpp
// タイマークラスを自作する場合
class GameTimer {
public:
	void Update() {
		auto currentTime = std::chrono::high_resolution_clock::now();
		m_deltaTime = std::chrono::duration<float>(currentTime - m_previousTime).count();
		m_previousTime = currentTime;
	}
	float GetDeltaTime() const { return m_deltaTime; }
private:
	std::chrono::time_point<std::chrono::high_resolution_clock> m_previousTime;
	float m_deltaTime = 0.0f;
};

// 使用例
GameTimer timer;

// ゲームループ内
timer.Update();
float deltaTime = timer.GetDeltaTime();
teapotRotationAngle += (Math::PI * 0.5f) * deltaTime;  // π/2 rad/sec
```

---

### 問題2: シャドウが回転しない

**症状**:
- ティーポット本体は回転するが、影は静止したまま

**原因**:
- `ModelRender::UpdateWorldMatrix()` で `m_shadowModels` の更新が抜けている

**解決策**:
- 【手順3】を実施して、シャドウマップ用モデルの更新処理を追加

**確認方法**:
```cpp
// ModelRender.h の UpdateWorldMatrix() に以下が含まれているか確認
for (int ligNo = 0; ligNo < NUM_DEFERRED_LIGHTING_DIRECTIONAL_LIGHT; ligNo++)
{
	for (int areaNo = 0; areaNo < NUM_SHADOW_MAP; areaNo++)
	{
		m_shadowModels[ligNo][areaNo].UpdateWorldMatrix(pos, rot, scale);
	}
}
```

---

### 問題3: 回転速度が速すぎる / 遅すぎる

**原因**:
- フレームレートが60FPSでない
- モニターのリフレッシュレート（144Hzなど）に同期している

**診断方法**:
```cpp
// main.cpp のゲームループ内
static int frameCount = 0;
static auto startTime = std::chrono::high_resolution_clock::now();

frameCount++;
if (frameCount % 60 == 0)
{
	auto currentTime = std::chrono::high_resolution_clock::now();
	float elapsed = std::chrono::duration<float>(currentTime - startTime).count();
	float fps = 60.0f / elapsed;

	OutputDebugStringA(("FPS: " + std::to_string(fps) + "\n").c_str());

	startTime = currentTime;
}
```

**解決策**:
```cpp
// 方法1: VSync（垂直同期）を有効にする
// GraphicsEngine の初期化時に設定

// 方法2: フレームレート非依存の実装
teapotRotationAngle += (Math::PI * 0.5f) * deltaTime;
```

---

### 問題4: ティーポットが表示されない

**確認項目**:

| 項目 | 確認方法 |
|------|---------|
| **ファイルパス** | `"Assets/modelData/teapot.tkm"` が存在するか |
| **初期化順序** | `renderingEngine.Init()` が先に実行されているか |
| **Draw() 呼び出し** | `teapotModelRender.Draw()` が呼ばれているか |
| **カメラ位置** | ティーポット (0, 50, 0) が視野内にあるか |

**デバッグ方法**:
```cpp
// ModelRender.cpp の Draw() 内でログ出力
void ModelRender::Draw()
{
	OutputDebugStringA("teapotModelRender.Draw() called\n");

	if (!m_forwardRenderModel.IsInited())
	{
		OutputDebugStringA("ERROR: m_forwardRenderModel is not initialized!\n");
		return;
	}

	// ... 描画処理 ...
}
```

---

## 7. 動作確認方法

### 7.1 基本動作の確認

**チェックリスト**:
- [ ] ティーポットが表示される
- [ ] ティーポットがY軸回りに回転する
- [ ] 約4秒で1回転する（目視確認）
- [ ] 地面にシャドウが投影される
- [ ] シャドウも回転する

---

### 7.2 詳細な動作確認

#### 回転速度の測定

1. アプリケーションを起動
2. ティーポットの特定の部分（取っ手など）に注目
3. ストップウォッチで4秒測定
4. 1回転していることを確認

#### シャドウの確認

**確認ポイント**:
- シャドウがティーポットと同期して回転
- シャドウの形状が滑らかに変化
- カスケードの境界で不自然なシャドウが出ないか

**カメラ操作**:
- 左スティック（Y軸）: 前後移動
- 左スティック（X軸）: 左右移動
- 右スティック（Y軸）: 上下移動

---

### 7.3 パフォーマンスの確認

#### FPSの確認（Visual Studio デバッガ）

1. デバッグ → パフォーマンス プロファイラー
2. GPU 使用状況をチェック
3. 60FPS を維持しているか確認

#### メモリ使用量

**予想値**:
- CPU側: 約32.5KB（TkmFile + Modelインスタンス × 15）
- GPU側: 約2.27MB（頂点バッファ + シャドウマップ等）

---

## 8. 応用編

### 8.1 複数のオブジェクトを回転させる

```cpp
// 初期化時
myRenderer::ModelRender teapots[3];
float teapotAngles[3] = { 0.0f, 0.0f, 0.0f };

for (int i = 0; i < 3; i++)
{
	myRenderer::ModelInitDataFR modelInitData;
	modelInitData.m_tkmFilePath = "Assets/modelData/teapot.tkm";
	modelInitData.m_fxFilePath = "Assets/shader/sample.fx";
	modelInitData.m_expandShaderResoruceView[0] = 
		&renderingEngine.GetZPrepassDepthTexture();

	teapots[i].InitForwardRendering(renderingEngine, modelInitData);
	teapots[i].SetShadowCasterFlag(true);
}

// ゲームループ内
for (int i = 0; i < 3; i++)
{
	teapotAngles[i] += Math::PI / 120.0f;

	Quaternion rotY;
	rotY.SetRotationY(teapotAngles[i]);

	// 異なる位置に配置
	Vector3 pos = { i * 100.0f - 100.0f, 50.0f, 0.0f };

	teapots[i].UpdateWorldMatrix(pos, rotY, g_vec3One);
	teapots[i].Draw();
}
```

---

### 8.2 回転速度を変更する

#### パターン1: 倍速回転（毎秒0.5回転）

```cpp
// 2倍速: π/120 × 2 = π/60
teapotRotationAngle += Math::PI / 60.0f;
```

#### パターン2: 逆回転

```cpp
// 逆回転: -π/120
teapotRotationAngle -= Math::PI / 120.0f;
```

#### パターン3: 加速・減速

```cpp
// 変数を追加
float rotationSpeed = 0.0f;       // 現在の速度
float rotationAcceleration = 0.001f;  // 加速度

// ゲームループ内
rotationSpeed += rotationAcceleration;
teapotRotationAngle += rotationSpeed;
```

---

### 8.3 複数軸での回転

```cpp
// X, Y, Z 軸すべてで回転
float angleX = 0.0f, angleY = 0.0f, angleZ = 0.0f;

// ゲームループ内
angleX += Math::PI / 180.0f;  // X軸
angleY += Math::PI / 120.0f;  // Y軸（メイン）
angleZ += Math::PI / 240.0f;  // Z軸

Quaternion rotX, rotY, rotZ, finalRot;
rotX.SetRotationX(angleX);
rotY.SetRotationY(angleY);
rotZ.SetRotationZ(angleZ);

// クォータニオンの合成
finalRot = rotZ * rotY * rotX;

teapotModelRender.UpdateWorldMatrix({ 0.0f, 50.0f, 0.0f }, finalRot, g_vec3One);
```

---

### 8.4 ゲームパッドで回転速度を制御

```cpp
// ゲームループ内
float stickInput = g_pad[0]->GetRStickXF();  // 右スティックのX軸（-1.0 ～ 1.0）

// スティック入力に応じて回転速度を変化
float rotationSpeed = (Math::PI / 120.0f) * (1.0f + stickInput);

teapotRotationAngle += rotationSpeed;

// 回転クォータニオンを作成
Quaternion rotY;
rotY.SetRotationY(teapotRotationAngle);

teapotModelRender.UpdateWorldMatrix({ 0.0f, 50.0f, 0.0f }, rotY, g_vec3One);
teapotModelRender.Draw();
```

---

### 8.5 往復回転（振り子運動）

```cpp
// 変数を追加
float swingAngle = 0.0f;       // 現在の角度
float swingSpeed = 0.0f;       // 現在の角速度
const float maxAngle = Math::PI / 4.0f;  // 最大角度（45度）
const float gravity = 0.001f;  // 重力加速度（調整値）

// ゲームループ内
// 単純な振り子運動（エネルギー保存は無視）
swingSpeed += -sin(swingAngle) * gravity;
swingAngle += swingSpeed;

// 回転クォータニオンを作成
Quaternion rotY;
rotY.SetRotationY(swingAngle);

teapotModelRender.UpdateWorldMatrix({ 0.0f, 50.0f, 0.0f }, rotY, g_vec3One);
teapotModelRender.Draw();
```

---

## まとめ

### 実装のポイント

1. **main.cpp の3箇所を変更**
   - 初期化コードの追加
   - 回転角度の変数宣言
   - ゲームループ内での回転処理

2. **ModelRender.h の1箇所を変更**
   - `UpdateWorldMatrix()` にシャドウマップ用モデルの更新を追加

3. **回転速度の計算**
   - 60FPS想定: `Math::PI / 120.0f` ラジアン/フレーム
   - 毎秒0.25回転（4秒で1回転）

4. **シャドウマップへの反映**
   - 12個のシャドウマップ用モデル（4ライト × 3カスケード）をすべて更新

### 重要な注意点

- フレームレート非依存の実装が理想だが、このエンジンでは60FPS固定を前提
- シャドウマップ用モデルの更新を忘れると、影が静止したままになる
- クォータニオンを使うことで、滑らかで安定した回転を実現

### 次のステップ

- フレームレート非依存の実装を試す
- 複数のオブジェクトを回転させる
- ユーザー入力で回転速度を制御する
- 物理演算を導入して、よりリアルな動きを実現する

---

## 参考資料

| 項目 | ファイル |
|------|---------|
| **レンダリングエンジンの詳細** | `RenderingEngine_Execute関数詳細解説.md` |
| **ModelRender の詳細** | `ModelRender詳細解説_Main.cppとの関連.md` |
| **シャドウマップの仕組み** | `シャドウマップで同じ物体が複数のモデルを持つ理由.md` |
| **Git の操作** | `Git_mdファイルがアップロードされない理由.md` |

---

**作成日**: 2025年
**対象プロジェクト**: Sample_14_01
**エンジンバージョン**: hlsl-grimoire-sample_2025
