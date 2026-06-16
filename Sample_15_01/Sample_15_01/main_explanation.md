# Sample_15_01 — main.cpp の処理フローと型説明

このドキュメントは、C++ は理解しているが DirectX の流れに不慣れな方向けに、Sample_15_01 の main.cpp の処理の流れを簡潔にまとめ、登場する主要型の役割と定義ファイルを示したものです。

---

## 概要フロー（順序）

1. InitGame(...) を呼んでウィンドウとエンジンを初期化（system/system.cpp）。
2. コンピュートシェーダーをロード（Shader cs）。エントリポイントは "CSMain"（Assets/shader/sample.fx）。
3. ルートシグネチャを初期化（InitRootSignature(rs, cs)）。
4. コンピュート用のパイプラインステートを初期化（InitPipelineState(rs, pipelineState, cs)）。
5. 入力データ配列 inputData を作成し、StructuredBuffer inputSB を Init（SRV 用）。
6. 出力用構造体 OutputData に合わせて RWStructuredBuffer outputSb を Init（UAV 用）。
7. DescriptorHeap ds に SRV/UAV を登録し Commit()（ディスクリプタヒープを確定）。
8. g_graphicsEngine->GetRenderContext() で RenderContext を取得。
9. ゲームループ（DispatchWindowMessage が true の間）
   - g_engine->BeginFrame()
   - inputData をランダム更新して inputSB.Update()
   - RenderContext でルートシグネチャ／パイプライン／ディスクリプタヒープをセット
   - RenderContext.Dispatch(1,1,1) を呼んでコンピュートをディスパッチ（GPU 実行）
   - g_engine->EndFrame()（コマンド送出・フレーム終了）
   - outputSb.GetResourceOnCPU() で結果を読み取り表示（MessageBoxA）

---

## 登場する主要インスタンスと役割（参照ファイル）

- Shader cs
  - 型: Shader
  - 役割: HLSL をコンパイルしてコンパイル済バイナリ（Blob）を保持。Compute PSO 作成時に使う。
  - 定義: MiniEngine/Shader.h

- RootSignature rs
  - 型: RootSignature
  - 役割: シェーダーに渡すリソースレイアウト（ルートパラメータ、テーブル、静的サンプラ等）を表す。
  - 定義: MiniEngine/RootSignature.h

- PipelineState pipelineState
  - 型: PipelineState
  - 役割: コンピュート（またはグラフィクス）用の PSO。シェーダバイトコードとルートシグネチャを束ねる。
  - 定義: MiniEngine/PipelineState.h

- StructuredBuffer inputSB
  - 型: StructuredBuffer
  - 役割: SRV（読み取り専用）としてシェーダーに配列データを渡すラッパ。Init/Update を持つ。
  - 定義: MiniEngine/StructuredBuffer.h

- RWStructuredBuffer outputSb
  - 型: RWStructuredBuffer
  - 役割: UAV（書き込み可能）としてコンピュートシェーダーが結果を書き込む。CPU から読み取るための GetResourceOnCPU 等を提供。
  - 定義: MiniEngine/RWStructuredBuffer.h

- DescriptorHeap ds
  - 型: DescriptorHeap
  - 役割: SRV/UAV/CBV/サンプラのディスクリプタをまとめて管理し、Commit() で GPU 用ディスクリプタヒープを作る。RenderContext にセットしてシェーダーが参照。
  - 定義: MiniEngine/DescriptorHeap.h

- RenderContext renderContext
  - 型: RenderContext
  - 役割: ID3D12GraphicsCommandList をラップし、SetComputeRootSignature / SetPipelineState / SetComputeDescriptorHeap / Dispatch などのコマンド記録 API を提供。
  - 定義: MiniEngine/RenderContext.h

- g_graphicsEngine / g_engine
  - 型: GraphicsEngine（グローバルポインタ） と エンジン実装（TkEngine 等）
  - 役割: DirectX デバイス／コマンドキュー／スワップチェインの管理。BeginFrame/EndFrame でフレームの開始と送出を管理する。
  - 定義: MiniEngine/GraphicsEngine.h, system/system.cpp

---

## OutputData（main.cpp 内定義）

main.cpp 内で定義される構造体:

```cpp
struct OutputData {
  float averageScore; // 平均点
  float maxScore;     // 最高得点
  float minScore;     // 最小得点
};
```

- 役割: コンピュートシェーダーが計算して UAV に書き込む結果のフォーマット。CPU は RWStructuredBuffer の GetResourceOnCPU() でこれを参照して表示する。

---

## 要点（短く）

- GPU での計算は「バッファ作成→ディスクリプタ登録→Dispatch（コマンド記録）→GPU 実行→結果の読み戻し」の流れ。
- ルートシグネチャは "どのリソースがどのスロットにあるか" を GPU に知らせる設計図。PipelineState はその設計図とシェーダを束ねる箱。
- StructuredBuffer は読み取り用、RWStructuredBuffer は書き込み（UAV）用。

---

## コンピュート実行時の GPU/CPU の同期（フラッシュ／フェンス）

DirectX 12 では CPU と GPU は独立に動作します。CPU がコマンドリスト（Dispatch 含む）を記録してコマンドキューに送信しても、GPU がそれを完了するまで CPU は別作業を継続できます。結果を CPU 側で安全に読むには同期が必要です。

- フレームの送出（EndFrame）:
  - g_engine->EndFrame() 相当の処理で、コマンドリストを Close してコマンドキューに ExecuteCommandLists で送ります。この送出が「フラッシュ」に当たります。
- フェンス(Fence) と待ち合せ:
  - GPU の完了を検知するために ID3D12Fence を用いるのが一般的です。エンジン側（GraphicsEngine）でフェンス値を増やし、GPU にシグナルを送り、CPU は SetEventOnCompletion または WaitForSingleObject で待ちます。
- 本サンプルでの結果読み取り:
  - outputSb.GetResourceOnCPU() が直接結果ポインタを返しています。内部実装では、EndFrame() が GPU にコマンド送出してから GPU の完了を待つ（フェンスで同期）か、または出力バッファが CPU からもアクセス可能なリソース（アップロードや readback バッファ）を使っているはずです。
- 注意点:
  - フェンスで同期していない状態で CPU が結果へアクセスすると、未確定のデータを読む危険があります（レースコンディション）。
  - 同期はコストが高い（待ち時間）ため、パイプラインを分解して待ちを最小化する手法（ダブル／トリプルバッファリング、非ブロッキングのフェンスポーリング）を使うことが多いです。

---

## main.cpp の処理を図解（ステップ別シーケンス図風）

以下は main.cpp における主要エンティティ間のやり取りをステップ順に追ったテキストのシーケンス図風説明です。

登場エンティティ: CPU (main), GraphicsEngine (g_engine / g_graphicsEngine), RenderContext, GPU, StructuredBuffer (inputSB), RWStructuredBuffer (outputSb), DescriptorHeap (ds)

1. CPU: InitGame(...) -> GraphicsEngine を初期化（ウィンドウ、D3D デバイス等）
2. CPU: Shader cs.LoadCS("sample.fx", "CSMain")
3. CPU: InitRootSignature(rs, cs)  // ルートレイアウト準備
4. CPU: InitPipelineState(rs, pipelineState, cs)  // Compute PSO 準備
5. CPU: inputSB.Init(..., inputData)  // 入力バッファ作成（GPU 上）
6. CPU: outputSb.Init(..., nullptr)  // 出力バッファ作成（GPU 上、CPU 読み取り可能領域あり）
7. CPU: ds.RegistShaderResource(0, inputSB); ds.RegistUnorderAccessResource(0, outputSb); ds.Commit()
8. CPU: renderContext = g_graphicsEngine->GetRenderContext()

ループ開始:

A. CPU: DispatchWindowMessage() == true
B. CPU: g_engine->BeginFrame()  // コマンドアロケータ/コマンドリストをリセットして記録開始
C. CPU: inputData を更新、inputSB.Update(inputData)  // CPU→GPU バッファ更新
D. CPU: renderContext.SetComputeRootSignature(rs)
E. CPU: renderContext.SetPipelineState(pipelineState)
F. CPU: renderContext.SetComputeDescriptorHeap(ds)
G. CPU: renderContext.Dispatch(1,1,1)  // コマンドリストに Dispatch を記録
H. CPU: g_engine->EndFrame()  // コマンドリストを Close して ExecuteCommandLists で GPU に送出（フラッシュ）
I. GPU: コマンドキュー上で Dispatch 実行 → CSMain が inputSB を読み、outputSb(UAV) に書き込む
J. GPU: Dispatch 完了後、フェンスにシグナルを置く
K. CPU: EndFrame 内か別の同期処理でフェンスを待ち、GPU 完了を確認
L. CPU: outputData = (OutputData*)outputSb.GetResourceOnCPU()  // 安全に結果参照
M. CPU: MessageBoxA を表示
N. ループへ戻る

---

## 参照ファイル一覧（プロジェクト内）
- Sample_15_01/Sample_15_01/main.cpp
- Sample_15_01/Sample_15_01/sub.h
- MiniEngine/Shader.h
- MiniEngine/RootSignature.h
- MiniEngine/PipelineState.h
- MiniEngine/StructuredBuffer.h
- MiniEngine/RWStructuredBuffer.h
- MiniEngine/DescriptorHeap.h
- MiniEngine/RenderContext.h
- MiniEngine/GraphicsEngine.h
- Sample_15_01/Sample_15_01/system/system.cpp

---

必要であれば、フェンスの具体的なコード例や outputSb の内部実装（GetResourceOnCPU の実装）を抜粋して詳説できます。どちらが必要ですか？