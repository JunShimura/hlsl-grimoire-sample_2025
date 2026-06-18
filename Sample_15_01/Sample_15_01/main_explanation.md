# Sample_15_01 — main.cpp の処理フロー＋インスタンス統合ガイド

このドキュメントは、C++ は理解しているが DirectX 12 の流れに不慣れな初心者向けに、**処理の流れ（順序）と登場するインスタンスを統合** して、**データの繋がりを可視化** し、**各インスタンスの役割を初心者向けに解説** したものです。

---

## 全体像：データと処理のつながり

以下は、main.cpp における主要な処理ステップと、各ステップで登場するインスタンスがどのようにデータを受け渡しするかを示したものです。矢印 `→` はデータ流、`|` は保持・参照関係を表します。

```
【初期化フェーズ】

  CPU メモリ                GPU メモリ
  ─────────────────────────────────────────
   inputData[] (配列)         [空]
	  |
	  v
   StructuredBuffer inputSB   (SRV: 読み取り用)
   - データを GPU にアップロード
	  |
	  |───────> [GPU 側の入力バッファ]

   OutputData (構造体定義)     [空]
	  |
	  v
   RWStructuredBuffer outputSb (UAV: 書き込み用)
   - GPU に確保、CPU からも読み取り可能領域
	  |
	  |───────> [GPU 側の出力バッファ]

   Shader cs (コンパイル済みコード)
	  |
	  +──> コンパイル済みバイナリ (Blob)
			|
			v
   RootSignature rs (リソースレイアウト定義)
			|
			+──> "このシェーダーは SRV を slot 0 に、UAV を slot 0 に使う"
				 という"契約書"を作成
			|
			v
   PipelineState pipelineState (実行命令書)
			|
			+──> "シェーダー + ルートシグネチャ + 実行設定" を束ねた一体物
			|
			v
   DescriptorHeap ds (シェーダー用リソースカタログ)
			|
			+──> inputSB → slot 0 (SRV として登録)
			+──> outputSb → slot 0 (UAV として登録)
			+──> Commit() で GPU 用ディスクリプタヒープを確定

【ループフェーズ】

   BeginFrame():
	   コマンドリストをリセット → 新しいコマンド記録を準備

   inputData 更新 → inputSB.Update() → GPU バッファを更新

   RenderContext:
	   SetComputeRootSignature(rs)    → "ルートシグネチャをセット"
	   SetPipelineState(pipelineState) → "PSO をセット"
	   SetComputeDescriptorHeap(ds)   → "リソースカタログをセット"
	   Dispatch(1,1,1)                → "GPU で Compute を実行するよう指示"

   EndFrame():
	   コマンドを GPU に送出 → GPU が実行開始
	   フェンス待ち → GPU が完了するまで待機

   outputSb.GetResourceOnCPU() → CPU 側から結果を読み取り
```

---

## 処理ステップ＋インスタンス統合説明

### ステップ1: シェーダーの準備

```cpp
Shader cs;
cs.LoadCS("Assets/shader/sample.fx", "CSMain");
```

**インスタンス: Shader cs**

- **役割**: HLSL ファイルをコンパイルして GPU 実行可能な機械語（バイトコード）に変換し、保持する。
- **初心者向け比喩**: レシピ本を「調理可能な手順書」にコンパイルするようなもの。手順書がないと調理できません。
- **内部**:
  - `m_blob`: コンパイル済みのバイトコード（ID3DBlob）
  - `m_isInited`: 初期化完了フラグ
- **定義**: MiniEngine/Shader.h
- **このステップ後**: コンパイル済みコードが `cs` 内に保持される。

---

### ステップ2: GPU へのリソース指定ルール（ルートシグネチャ）の定義

```cpp
RootSignature rs;
InitRootSignature(rs, cs);
```

**インスタンス: RootSignature rs**

- **役割**: シェーダーが「どのスロット番号で」「どのリソース（バッファ等）を」受け取るかを定義する"契約書"。
- **初心者向け比喩**: 
  - 郵便配達員が「この家は玄関(slot 0)に荷物を、裏口(slot 1)に新聞を」という指示を受けるようなもの。
  - あるいは、工場の組立ラインで「ロボットアーム A は材料 X を slot 0 から、道具 Y を slot 1 から取る」という設定。
- **内部**:
  - `m_rootSignature`: ID3D12RootSignature オブジェクト
  - リソースがどのレジスタ（SRV slot 0, UAV slot 0 など）にあるかを定義
- **定義**: MiniEngine/RootSignature.h
- **このステップ後**: GPU がリソースの"受け取り方"のルールを知る。

---

### ステップ3: 実行設定の確定（パイプラインステート）

```cpp
PipelineState pipelineState;
InitPipelineState(rs, pipelineState, cs);
```

**インスタンス: PipelineState pipelineState**

- **役割**: シェーダーコード + ルートシグネチャ + 実行時設定（ブレンド、ラスタライザなど）を一体の"実行命令書"にまとめる。
- **初心者向け比喩**: 
  - 料理で「このレシピ + この調理器具 + この火力設定」をセットで確定する、みたいなもの。
  - 一度作ったら、何度も使える固定の "設定パッケージ"。
- **内部**:
  - `m_pipelineState`: ID3D12PipelineState オブジェクト
- **定義**: MiniEngine/PipelineState.h
- **このステップ後**: GPU が "何を、どう実行するか" の全て が決まる。

---

### ステップ4: 入力データ用バッファの作成

```cpp
int inputData[] = { 20, 30, 40 };

StructuredBuffer inputSB;
inputSB.Init(sizeof(int), 3, inputData);
```

**インスタンス: StructuredBuffer inputSB**

- **役割**: CPU 側の配列データを GPU メモリにアップロードし、シェーダーから **読み取り専用** で参照可能にするラッパー。
- **初心者向け比喩**: 
  - 雑誌を図書館に寄贈する。雑誌は図書館に保管されて、みんなが読める（書き込めない）状態。
  - SRV (Shader Resource View) = "読むだけ" という意味。
- **内部**:
  - `m_buffersOnGPU[2]`: GPU メモリ上のバッファ（ダブルバッファで管理）
  - `m_buffersOnCPU[2]`: CPU からアクセス可能なマップメモリ（Update() で使用）
  - `m_numElement`: 要素数（3）
  - `m_sizeOfElement`: 各要素のサイズ（sizeof(int) = 4 バイト）
- **定義**: MiniEngine/StructuredBuffer.h
- **このステップ後**: inputData の内容が GPU メモリ上に存在し、シェーダーから読める状態。

---

### ステップ5: 出力データ用バッファの作成

```cpp
struct OutputData {
  float averageScore;  // 平均点
  float maxScore;      // 最高得点
  float minScore;      // 最小得点
};

RWStructuredBuffer outputSb;
outputSb.Init(sizeof(OutputData), 1, nullptr);
```

**インスタンス: RWStructuredBuffer outputSb**

- **役割**: シェーダーが **読み書き可能** な出力バッファ。コンピュートシェーダーが結果を書き込み、CPU も読み取り可能。
- **初心者向け比喩**: 
  - ホワイトボードを複数の人が共有する。GPU が計算結果を書き込み、CPU がそれを確認する。
  - UAV (Unordered Access View) = "読み書きできる" という意味。
- **内部**:
  - `m_buffersOnGPU[2]`: GPU メモリ上のバッファ
  - `m_buffersOnCPU[2]`: CPU からアクセス可能なマップメモリ（GetResourceOnCPU() で使用）
  - 内部的には CPU と GPU の同期を取る仕掛けが隠れている
- **定義**: MiniEngine/RWStructuredBuffer.h
- **このステップ後**: GPU が書き込める出力領域が準備完了。

---

### ステップ6: リソースカタログの作成と確定（ディスクリプタヒープ）

```cpp
DescriptorHeap ds;
ds.RegistShaderResource(0, inputSB);           // SRV を slot 0 に登録
ds.RegistUnorderAccessResource(0, outputSb);  // UAV を slot 0 に登録
ds.Commit();                                   // 確定
```

**インスタンス: DescriptorHeap ds**

- **役割**: シェーダーが参照するリソース（SRV, UAV, CBV など）をまとめて管理し、GPU に渡す"リソースカタログ"。
- **初心者向け比喩**: 
  - 工場の組立ロボットが参照する"部品表"。"部品 A は棚 1、部品 B は棚 2" というリスト。
  - ルートシグネチャが「どのスロットにリソースがあるか」を言い、ディスクリプタヒープが「実際のリソースはここ」と指差す。
- **内部**:
  - `m_shaderResources[]`: SRV（読み取りリソース）の配列
  - `m_uavResources[]`: UAV（読み書きリソース）の配列
  - `m_descriptorHeap[2]`: GPU 用ディスクリプタヒープ（ダブルバッファ）
- **定義**: MiniEngine/DescriptorHeap.h
- **このステップ後**: "inputSB は SRV slot 0、outputSb は UAV slot 0" という対応が GPU 側に確定される。

---

### ステップ7: コマンド記録と実行（RenderContext + BeginFrame/EndFrame）

```cpp
auto& renderContext = g_graphicsEngine->GetRenderContext();

while (DispatchWindowMessage()) {
  g_engine->BeginFrame();

  // ⇒ ここから コマンド記録
  renderContext.SetComputeRootSignature(rs);
  renderContext.SetPipelineState(pipelineState);
  renderContext.SetComputeDescriptorHeap(ds);
  renderContext.Dispatch(1, 1, 1);
  // ⇐ ここまで コマンド記録

  g_engine->EndFrame();

  // 結果読み取り
  OutputData* outputData = (OutputData*)outputSb.GetResourceOnCPU();
}
```

**インスタンス: RenderContext renderContext**

- **役割**: GPU へのコマンド記録（SetRootSignature, SetPipelineState, SetDescriptorHeap, Dispatch）を行うコマンドリストラッパー。
- **初心者向け比喩**: 
  - GPS カーナビの指示を記録する紙のリスト。"左に曲がれ" → "信号待ち" → "右に曲がれ" をリストアップ。
  - SetComputeRootSignature ~ Dispatch までが "今のフレームで GPU にやらせるコマンド群" を記録する。
- **内部**:
  - `m_commandList`: ID3D12GraphicsCommandList4（実物の DirectX コマンドリスト）
  - SetXXX メソッドはコマンドを記録するだけ。GPU はまだ実行していない。
- **定義**: MiniEngine/RenderContext.h

**インスタンス: g_engine->BeginFrame() / EndFrame()**

- **BeginFrame() の役割**: 
  - 新しいコマンドリストの記録を開始する準備。前フレームの GPU 実行が完了するまで待つ（同期）。
  - コマンドアロケータをリセット。
- **EndFrame() の役割**:
  - コマンドリストを Close（記録終了）。
  - ExecuteCommandLists で GPU に送出（フラッシュ）。
  - 必要に応じてフェンスでの同期。

**このステップ後**: GPU はコマンドキュー上でコマンドを実行開始。

---

## コンピュート実行時の GPU/CPU の同期（フラッシュ／フェンス）

DirectX 12 では CPU と GPU は独立に動作します。CPU がコマンドリスト（Dispatch 含む）を記録してコマンドキューに送信しても、GPU がそれを完了するまで CPU は別作業を継続できます。結果を CPU 側で安全に読むには同期が必要です。

**フラッシュ (Flush)**
- コマンドリストを Close して ExecuteCommandLists で GPU のコマンドキューに送出すること。
- CPU は記録作業を終え、GPU に "これから実行してください" と引き渡す瞬間。
- EndFrame() 内でこれが行われる。

**フェンス (Fence)**
- GPU の完了を CPU が知るための同期オブジェクト（ID3D12Fence）。
- GPU が処理を終わらせると、フェンスに "完了信号" を置く。
- CPU はその信号を待つ（WaitForSingleObject など）。
- これにより CPU と GPU の処理が安全に交互実行される。

**本サンプルの流れ**:
1. EndFrame() で コマンドを GPU に送出（フラッシュ）。
2. GPU が Dispatch を実行、結果を outputSb に書き込む。
3. 同期機構（内部的には EndFrame の後、または GetResourceOnCPU の前）でフェンスを待つ。
4. GPU 完了を確認後、CPU が GetResourceOnCPU() で結果を安全に読み取る。

**注意点**:
- フェンス待ちは **パフォーマンス低下の原因** になります（CPU が無駄に待つ）。
- 実際のゲームやレンダラーでは、複数フレームを先読みして待ち時間を最小化する工夫（ダブルバッファリング等）が使われます。

---

## 各インスタンスの相互接続図

以下は、各インスタンスが どのようなプロセスで互いに接続されるかを可視化したものです：

```
【メモリ配置と接続】

CPU メモリ層                    GPU メモリ層
═════════════════════════════════════════════════

inputData[3]                    [inputSB GPU バッファ]
   ↓ Update()                        ↑
   │                                 │ SRV
   └─> StructuredBuffer inputSB ────┘ (読み取り)

OutputData (構造体)             [outputSb GPU バッファ]
   ↑ GetResourceOnCPU()              ↓
   │                                 │ UAV
   └─ RWStructuredBuffer outputSb ←─┘ (読み書き)

【GPU 実行フェーズ】

DescriptorHeap ds
   ├─ SRV[slot 0] → inputSB
   └─ UAV[slot 0] → outputSb
		↓ SetComputeDescriptorHeap()
   RenderContext
		├─ SetComputeRootSignature(rs) → どのスロットにリソースを受け取るか指示
		├─ SetPipelineState(pipelineState) → 何を実行するか指示
		├─ Dispatch(1,1,1) → 実行コマンド記録
		↓
   コマンドリスト（GPU へのコマンド列）
		↓ ExecuteCommandLists (EndFrame 内)
   ================== フラッシュ（GPU へ送出） ==================
		↓
   【GPU 側】
   コンピュートシェーダー (CSMain)
		├─ SRV[0] から inputData を読む
		└─ UAV[0] へ OutputData を書き込む
		↓
   フェンス信号 ← GPU 完了
		↓
   ================== 同期待ち（CPU が GPU 完了を確認） ==================
		↓
   CPU: GetResourceOnCPU() で結果を読み取り可能
```

---

## DirectX 12 初心者向けの理解ポイント

### 1. CPU と GPU は独立している

DirectX 12 では、CPU と GPU は別々の処理単位です。

- **CPU**: コマンドを "記録" して GPU に送出する。
- **GPU**: 受け取ったコマンドを独立に実行。CPU を待たない。

**比喩**: 
- 宅配便のドライバーに荷物と配達先リストを渡す（CPU がコマンド送出）。
- ドライバーはその後、独立に配達を続ける（GPU が実行中）。
- 一方、CPU は新しい指示を準備したり、配達結果を確認したりできる。

---

### 2. バッファ（メモリ）には 2 種類の用途がある

| 用途 | インスタンス | 読み取り | 書き込み | 例 |
|------|-----------|--------|---------|-----|
| **入力（読み取り）** | StructuredBuffer (SRV) | ✓ GPU | ✗ | 配列、テクスチャデータ |
| **出力（読み書き）** | RWStructuredBuffer (UAV) | ✓ GPU | ✓ GPU<br>✓ CPU | 計算結果、スクリーン出力 |

- **SRV (Shader Resource View)**: "看板" のようなもの。読むだけ。
- **UAV (Unordered Access View)**: "ホワイトボード" のようなもの。読み書き。

---

### 3. ルートシグネチャはシェーダーとリソースの "約束書"

**ルートシグネチャ**: "シェーダーは SRV slot 0 で入力データを受け取り、UAV slot 0 で出力を書き込む"

**ディスクリプタヒープ**: "実際に、SRV slot 0 には inputSB が、UAV slot 0 には outputSb が入っています"

両者が一致して初めて、シェーダーが正しいデータにアクセスできます。

**比喩**: 
- ルートシグネチャ = 工場の設計書「ロボット A は材料を棚 1 から、工具を棚 2 から取る」
- ディスクリプタヒープ = 実際の配置「棚 1 には鋼材が、棚 2 にはドライバーがある」

---

### 4. パイプラインステートは "1 回だけ作って何度も使う"

PipelineState を一度作ると、毎フレーム同じ設定で実行できます。

- **メリット**: 設定が固定で高速。
- **デメリット**: 設定を変えたい場合は、別の PipelineState を作る必要がある。

**比喩**: 
- ホットプレートの設定を「200℃」で固定しておく。
- 使うたびに設定を確認・変更しなくて済む（高速）。

---

### 5. コマンド記録 → フラッシュ → GPU 実行 → 同期

```
【CPU】                      【GPU】
  ↓
BeginFrame()
（前フレームの完了を待つ）
  ↓
SetRootSignature()  ┐
SetPipelineState()  ├─ コマンド記録
Dispatch()          ┘
  ↓
EndFrame()
（コマンドを閉じて GPU に送出 = フラッシュ）
  ↓
		   フラッシュ ──→ コマンド実行開始
		   （GPU）
  ↓
		（GPU がコマンド実行中...）
  ↓
（必要に応じてフェンス待ち）  ←─ 完了信号
  ↓
GetResourceOnCPU()
（結果を読み取り）
```

---

## 参照ファイル一覧（プロジェクト内）

- **Sample_15_01/Sample_15_01/main.cpp** - メイン処理
- **Sample_15_01/Sample_15_01/sub.h** - InitRootSignature/InitPipelineState の宣言
- **MiniEngine/Shader.h** - シェーダーコンパイル
- **MiniEngine/RootSignature.h** - リソース指定ルール
- **MiniEngine/PipelineState.h** - 実行命令書
- **MiniEngine/StructuredBuffer.h** - 読み取り用バッファ
- **MiniEngine/RWStructuredBuffer.h** - 読み書き用バッファ
- **MiniEngine/DescriptorHeap.h** - リソースカタログ
- **MiniEngine/RenderContext.h** - コマンド記録ラッパー
- **MiniEngine/GraphicsEngine.h** - DirectX 全体管理
- **Sample_15_01/Sample_15_01/system/system.cpp** - ウィンドウ＆エンジン初期化

---

## まとめ：処理の流れ（一句で）

> **GPU で計算させたいなら、「何を実行するか」「何を受け取るか」「どこにリソースがあるか」の 3 つを GPU に教えて、コマンドを送り、完了を待つ。**

---


