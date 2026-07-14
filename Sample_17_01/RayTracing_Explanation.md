# RayTracing（レイトレーシング）によるレンダリングの仕組み

## 目次

1. [概要](#概要)
2. [1フレームのレンダリングフロー](#1フレームのレンダリングフロー)
3. [詳細解説](#詳細解説)
   - [Step-0: DirectX 12初期化](#step-0-directx-12初期化)
   - [Step-1: モデルをレイトレワールドに追加](#step-1-モデルをレイトレワールドに追加)
   - [Step-2: 登録されたモデルを使ってレイトレワールドを構築](#step-2-登録されたモデルを使ってレイトレワールドを構築)
   - [Step-3: レイをディスパッチ](#step-3-レイをディスパッチ)
   - [Step-4: レイと衝突した点の色を計算](#step-4-レイと衝突した点の色を計算)
   - [Step-5: レイが三角形ポリゴンと衝突しなかった時のカラーを計算](#step-5-レイが三角形ポリゴンと衝突しなかった時のカラーを計算)
4. [DirectX 12 API・データ構造リファレンス](#directx-12-apiデータ構造リファレンス)

---

## 概要

RayTracing（レイトレーシング）は、コンピューターグラフィックスにおいて、カメラから投射した光線（レイ）を使用して、3D空間内のジオメトリと交差判定を行い、リアルな光の挙動をシミュレートするレンダリング手法です。

DirectX 12では、GPUが直接レイトレーシング計算を実行できる専用機能が提供されており、以下のステップで実装されます：

1. **加速構造の構築**: ジオメトリ情報を効率的に検索できるデータ構造（BLASとTLAS）を構築
2. **シェーダーの設定**: Ray Generation Shader、Closest Hit Shader、Miss Shaderを定義
3. **レイのディスパッチ**: GPUに対してレイトレーシング計算の開始を指令
4. **結果の収集**: ピクセルごとの色計算結果を出力

このドキュメントでは、Sample_17_01のmain.after.cppをベースに、起動後から1フレームのレンダリング完了までの流れを、使用されるDirectX 12 APIとデータ構造の意味を合わせて解説します。

---

## 1フレームのレンダリングフロー

```
main()関数起動
	↓
[DirectX 12初期化]  ← GraphicsEngine::Init()内で実行
	↓
// 初期化フェーズ（ゲームループの前に一度だけ行う処理）
	├─ Step-1: RegistModelToRaytracingWorld(model)  ... モデル登録
	├─ Step-2: BuildRaytracingWorld(renderContext)  ... BLAS/TLAS構築
	↓
[ゲームループ開始]
	├─ BeginFrame() ... レンダリング開始準備
	├─ Step-3: DispatchRaytracing(renderContext)    ... レイディスパッチ
	│  ├─ Ray Generation Shader実行
	│  ├─ Closest Hit / Miss Shader実行
	│  └─ 結果を出力テクスチャに書き込み
	├─ EndFrame() ... レンダリング終了
	└─ 結果をフレームバッファに表示
	↓
[ゲームループ終了]
```

---

## 詳細解説

### Step-0: DirectX 12初期化

**処理概要**: 
`main.after.cpp`の最初で`InitGame()`関数が呼び出され、DirectX 12デバイスやコマンドキューなどの初期化が行われます。

```cpp
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	// ゲームの初期化
	InitGame(hInstance, hPrevInstance, lpCmdLine, nCmdShow, TEXT("Game"));
	// ...
}
```

**内部処理**:
`GraphicsEngine::Init()`が呼ばれ、以下の処理が実行されます：

#### 1-1. D3Dデバイスの作成
**データ構造**: `ID3D12Device5*` 
**説明**: DirectX 12のメインインターフェース。GPUリソースの作成やコマンド発行の管理を行います。
**API**: [ID3D12Device5](https://learn.microsoft.com/windows/win32/api/d3d12/nn-d3d12-id3d12device5)

```cpp
bool GraphicsEngine::CreateD3DDevice(IDXGIFactory4* dxgiFactory)
{
	// D3Dアダプタの列挙
	// ID3D12Device5の作成
}
```

#### 1-2. コマンドキューの作成
**データ構造**: `ID3D12CommandQueue*`
**説明**: GPUに実行コマンドを送信するキューです。コマンドリストに記録されたコマンドをGPUに送信します。
**API**: [ID3D12CommandQueue](https://learn.microsoft.com/windows/win32/api/d3d12/nn-d3d12-id3d12commandqueue)

```cpp
D3D12_COMMAND_QUEUE_DESC queueDesc = {};
queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
```

#### 1-3. スワップチェインの作成
**データ構造**: `IDXGISwapChain3*`
**説明**: フロントバッファとバックバッファを切り替えて、ちらつきのないアニメーション表示を実現します。
**API**: [IDXGISwapChain3](https://learn.microsoft.com/windows/win32/api/dxgi1_3/nn-dxgi1_3-idxgiswapchain3)

#### 1-4. ディスクリプタヒープの作成
**データ構造**: `ID3D12DescriptorHeap*`
**説明**: GPU上のリソースへのビュー情報（SRV、UAV、CBVなど）を一括管理するメモリ領域です。

```cpp
D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
heapDesc.NumDescriptors = 256;
heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap));
```

#### 1-5. コマンドリストの作成
**データ構造**: `ID3D12GraphicsCommandList4*`
**説明**: GPU実行コマンドを記録します。描画コマンド、リソースのバリア設定、計算シェーダーの実行など、すべてのGPU処理はコマンドリストに記録されます。
**API**: [ID3D12GraphicsCommandList4](https://learn.microsoft.com/windows/win32/api/d3d12/nn-d3d12-id3d12graphicscommandlist4)

#### 1-6. フェンスの作成（GPUとCPUの同期用）
**データ構造**: `ID3D12Fence*`
**説明**: GPUとCPUの処理タイミングを同期させるためのオブジェクトです。GPUが特定の処理を完了したことを確認するために使用されます。
**API**: [ID3D12Fence](https://learn.microsoft.com/windows/win32/api/d3d12/nn-d3d12-id3d12fence)

---

### Step-1: モデルをレイトレワールドに追加

**処理概要**:
`main.after.cpp`で以下のコードが実行されます：

```cpp
// モデルをロードする
ModelInitData modelInitData;
modelInitData.m_tkmFilePath = "Assets/modelData/sample.tkm";
Model model;
model.Init(modelInitData);

// モデルをレイトレワールドに追加
g_graphicsEngine->RegistModelToRaytracingWorld(model);
```

**内部処理**:
`GraphicsEngine::RegistModelToRaytracingWorld()`内で、`RaytracingEngine::RegistGeometry()`が呼ばれ、さらに`World::RegistGeometry()`が実行されます。

#### 1-1. モデルのメッシュとマテリアルの取得

```cpp
void World::RegistGeometry(Model& model)
{
	model.QueryMeshAndDescriptorHeap([&](const SMesh& mesh, const DescriptorHeap& ds) {
		// メッシュごとにループ
		for (int i = 0; i < mesh.m_materials.size(); i++) {
			// ...
		}
	});
}
```

#### 1-2. D3D12_RAYTRACING_GEOMETRY_DESCの設定

**データ構造**: `D3D12_RAYTRACING_GEOMETRY_DESC`
**説明**: レイトレーシング用のジオメトリ情報を定義します。頂点バッファとインデックスバッファの情報を指定し、レイ-ポリゴン交差判定用のデータを構築します。
**API**: [D3D12_RAYTRACING_GEOMETRY_DESC](https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_raytracing_geometry_desc)

```cpp
D3D12_RAYTRACING_GEOMETRY_DESC desc;
memset(&desc, 0, sizeof(desc));

// ジオメトリのタイプは「三角形」
desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

// 変換行列は使用しない（アイデンティティ行列を使用）
desc.Triangles.Transform3x4 = 0;

// 頂点バッファの情報を設定
const auto& vertexBufferView = mesh.m_vertexBuffer.GetView();
// GPU仮想アドレス（ID3D12Resource::GetGPUVirtualAddress()から取得される値）を直接使う
// これにより、加速構造の構築時にGPUが頂点データへアクセスできるようにする
desc.Triangles.VertexBuffer.StartAddress = vertexBufferView.BufferLocation;
// 1頂点あたりのバイト数（頂点構造体のサイズ）。インターリーブされた頂点レイアウトでも正しいストライドを指定する
desc.Triangles.VertexBuffer.StrideInBytes = vertexBufferView.StrideInBytes;
// 総頂点数はバッファサイズ / ストライドで計算できる
desc.Triangles.VertexCount = vertexBufferView.SizeInBytes / vertexBufferView.StrideInBytes;

// 頂点フォーマットを指定（位置のみを使う場合の例）
// 頂点バッファに法線やUVも含まれる場合でも、ここではレイ-三角形交差で必要な位置データのフォーマットを指定する
desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

// オプティマイゼーションフラグを指定（透過なし＝opaque）
// OPAQUEを指定すると、トラバースやヒット時の最適化が行われる。
// 透明や透過処理が必要なジオメトリはOPAQUEを外す（またはALLOW_ANY）ことを検討する
desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

// インデックスバッファの情報を設定
const auto& indexBufferView = mesh.m_indexBufferArray[i]->GetView();
// インデックスもGPU仮想アドレスで指定
desc.Triangles.IndexBuffer = indexBufferView.BufferLocation;
// インデックスの総数（トライアングル構成に応じて3の倍数であることが多い）
desc.Triangles.IndexCount = mesh.m_indexBufferArray[i]->GetCount();
// 16bitか32bitかを指定（例: DXGI_FORMAT_R16_UINT または DXGI_FORMAT_R32_UINT）
desc.Triangles.IndexFormat = indexBufferView.Format;
```

**詳細解説（初心者向け）**:
- GPU仮想アドレスとは: ID3D12Resourceが保持するGPU側でのアドレスで、CPU側のポインタとは異なる。加速構造の記述ではこのアドレスを使ってGPUが直接頂点/インデックスへアクセスする。
- ストライドとフォーマット: 頂点バッファが {position, normal, uv} のように複数要素を持つ場合でも、StrideInBytesは1頂点分のバイト数を指す。VertexFormatはレイトレーシングが必要とする頂点の要素（通常は位置）を表す。
- インデックス形式の差: 16ビットインデックスはメモリ節約になるが、頂点数が多い場合は32ビットが必要になる。IndexCountはインデックスの総数（頂点参照回数）を指定する。
- リソースの状態: 加速構造を構築する前に、頂点/インデックスバッファはGPUが読み出せる状態（たとえば D3D12_RESOURCE_STATE_GENERIC_READ や D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE 等）になっている必要がある。必要に応じて ID3D12GraphicsCommandList::ResourceBarrier で遷移を行う。

**注意点とベストプラクティス**:
- 頂点バッファのオフセット: 一部の実装では頂点配列の先頭に余分なヘッダや別データがある場合があるため、StartAddressは適切なオフセットを指していることを確認する。
- 逆順（Winding）とカリング: レイトレーシングでも三角形の向きは重要になる場合がある。モデルの頂点順序が期待通りか、カリング設定に合わせる必要がある。
- ジオメトリフラグ: D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE の他に、ALLOW_UPDATE や NO_DUPLICATE_ANY_HIT_INVOCATION 等のフラグがあり、用途に応じて設定できる。

#### 1-3. Instanceの生成

```cpp
// 加速構造構築で使用するためのインスタンス（メッシュ＋マテリアル）を作成
InstancePtr instance = std::make_unique<Instance>();
instance->geometoryDesc = desc; // 構築したジオメトリ記述を保持
instance->m_material = mesh.m_materials[i]; // ピクセルやシェーダーで使うマテリアル情報を参照

// RWSBはこのサンプル内のラッパー（読み出し用GPUバッファを管理）
// BLASを構築する際、頂点/インデックスのGPUアドレスが必要になるため、
// 元のバッファ資源を保持しておくラッパーを用意している
instance->m_vertexBufferRWSB.Init(mesh.m_vertexBuffer, false);
instance->m_indexBufferRWSB.Init(*mesh.m_indexBufferArray[i], false);

m_instances.emplace_back(std::move(instance));
```

- これらのInstanceは、BuildRaytracingWorld() の呼び出し時に BLAS/TLAS の入力としてまとめて参照される。順序は重要で、登録が完了してから加速構造を構築する必要がある。
- Instance は TLAS 内での InstanceID やシェーダーヒットグループへの対応付け（InstanceContributionToHitGroupIndex）などの情報と合わせて使用される。

---

### Step-2: 登録されたモデルを使ってレイトレワールドを構築

**処理概要**:
`main.after.cpp`で以下のコードが実行されます：

```cpp
// Step-2 登録されたモデルを使ってレイトレワールドを構築
g_graphicsEngine->BuildRaytracingWorld(renderContext);
```

**内部処理**:
`RaytracingEngine::CommitRegistGeometry(renderContext)`が呼ばれ、BLAS（Bottom Level Acceleration Structure）とTLAS（Top Level Acceleration Structure）が構築されます。

#### 2-1. BLAS（Bottom Level Acceleration Structure）の構築

**データ構造**: `AccelerationStructureBuffers`
**説明**: BLAS は、個々のジオメトリ（メッシュ）に対する加速構造です。複数の三角形ポリゴンをBVH（Bounding Volume Hierarchy）という階層的なバウンディングボックスの木構造として管理することで、レイとの交差判定を高速化します。

```cpp
void BLASBuffer::Init(RenderContext& rc, const std::vector<InstancePtr>& instances)
{
	// 各インスタンスのジオメトリ情報からBLASを構築
	for(const auto& instance : instances) {
		// D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESCを作成
		// BuildRaytracingAccelerationStructureを呼び出してBLASを構築
		// AccelerationStructureBuffersに結果を格納
	}
}
```

**処理フロー**:
1. `D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC`構造体を作成

3. 構築結果を`AccelerationStructureBuffers`に格納

**関連API**: 
- [D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC](https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_build_raytracing_acceleration_structure_desc)

#### 2-2. TLAS（Top Level Acceleration Structure）の構築

**データ構造**: `AccelerationStructureBuffers`（BLAS同様）
**説明**: TLAS は、シーン内のすべてのジオメトリを管理する最上位レベルの加速構造です。複数のBLASインスタンスの空間的位置と姿勢の情報を含み、グローバルなレイ-シーン交差判定を効率化します。

```cpp
void TLASBuffer::Init(RenderContext& rc, const std::vector<InstancePtr>& instances, const BLASBuffer& blasBuffer)
{
	// 各インスタンスの位置・回転・スケール情報を設定
	// D3D12_RAYTRACING_INSTANCE_DESCを構築
	// TLASを構築
}
```

**処理フロー**:
1. 各インスタンスの位置・回転・スケール情報（変換行列）を定義
2. `D3D12_RAYTRACING_INSTANCE_DESC`配列を構築
3. `ID3D12Device5::BuildRaytracingAccelerationStructure()`を呼び出しTLASを構築

**関連API**: 
- [D3D12_RAYTRACING_INSTANCE_DESC](https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_raytracing_instance_desc)

**D3D12_RAYTRACING_INSTANCE_DESCのメンバ説明**:

| メンバ | 説明 |
|--------|------|
| `Transform` | 3x4の変換行列（ワールド座標系への変換） |
| `InstanceID` | インスタンスの識別子（0〜16777215） |
| `InstanceMask` | レイトレーシング時のマスク値（0〜255） |
| `InstanceContributionToHitGroupIndex` | ヒットグループテーブル内のオフセット |
| `Flags` | インスタンスのフラグ（変換行列の形式など） |
| `AccelerationStructure` | このインスタンスが参照するBLASのGPUアドレス |

---

### Step-3: レイをディスパッチ

**処理概要**:
ゲームループ内で、以下のコードが毎フレーム実行されます：

```cpp
// Step-3 レイをディスパッチ
g_graphicsEngine->DispatchRaytracing(renderContext);
```

**内部処理**:
`RaytracingEngine::Dispatch(renderContext)`が呼ばれます。

#### 3-1. レイトレーシング用PSO（Pipeline State Object）の設定

**データ構造**: `ID3D12StateObject*`
**説明**: PSO（パイプラインステートオブジェクト）は、GPUのパイプライン設定を一括管理するオブジェクトです。レイトレーシング用のPSOには、Ray Generation ShaderやHit Shaderなど、複数のシェーダーを関連付ける情報が含まれます。
**API**: [ID3D12StateObject](https://learn.microsoft.com/windows/win32/api/d3d12/nn-d3d12-id3d12stateobject)

```cpp
void PSO::Init(const DescriptorHeaps& descriptorHeaps)
{
	// Ray Generation Shaderのルートシグネチャを定義
	// Closest Hit ShaderやMiss Shaderのルートシグネチャを定義
	// State Object Configurationを作成
	// Device->CreateStateObject()でPSOを生成
}
```

#### 3-2. シェーダーテーブルの作成

**データ構造**: `ShaderTable`
**説明**: シェーダーテーブルは、各シェーダーのメモリアドレスと関連データをまとめたテーブルです。Ray Generation、Hit、Missなど異なる種類のシェーダーの情報をGPUが効率的に参照できるように構成されます。

**シェーダーテーブルの構成**:
```
[Ray Generation Record]
	- Shader Identifier
	- Root Arguments (定数バッファへのアドレスなど)

[Miss Shader Record]
	- Shader Identifier
	- Root Arguments

[Closest Hit Shader Record]
	- Shader Identifier
	- Root Arguments
	...
```

#### 3-3. ディスクリプタヒープの設定

**概要**: レイトレーシング実行時に使用するリソース（TLAS、出力テクスチャ、定数バッファなど）をディスクリプタヒープに登録します。

#### 3-4. DispatchRaysコマンドの実行

**API**: [ID3D12GraphicsCommandList4::DispatchRays](https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist4-dispatchrays)

```cpp
D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
dispatchDesc.RayGenerationShaderRecord.StartAddress = rayGenRecord.StartAddress;
dispatchDesc.RayGenerationShaderRecord.SizeInBytes = rayGenRecord.SizeInBytes;
dispatchDesc.MissShaderTable.StartAddress = missTableStart;
dispatchDesc.MissShaderTable.SizeInBytes = missTableSize;
dispatchDesc.MissShaderTable.StrideInBytes = missShaderRecordSize;
dispatchDesc.HitGroupTable.StartAddress = hitGroupTableStart;
dispatchDesc.HitGroupTable.SizeInBytes = hitGroupTableSize;
dispatchDesc.HitGroupTable.StrideInBytes = hitGroupRecordSize;
dispatchDesc.Width = screenWidth;
dispatchDesc.Height = screenHeight;
dispatchDesc.Depth = 1;

commandList->DispatchRays(&dispatchDesc);
```

**DispatchRaysの処理フロー**:
1. GPU上で、スクリーン空間のピクセルごとにRay Generation Shaderを並列実行
2. 各Ray Generation Shaderが、カメラからスクリーンピクセルに向かうレイを生成
3. そのレイをシーン内のTLASに対して投射（TraceRay）
4. レイが何かに衝突した場合、Closest Hit Shaderを実行
5. 何にも衝突しなかった場合、Miss Shaderを実行
6. シェーダーが計算した色を出力テクスチャ（UAV）に書き込み

---

### Step-4: レイと衝突した点の色を計算

**処理概要**:
`sample.after.fx`のClosest Hit Shader（`chs`）が実行されます。

```hlsl
[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	// Step-4 レイと衝突した点の色を計算する
	float3 barycentrics;
	barycentrics.x = 1.0f - attribs.barycentrics.x - attribs.barycentrics.y;
	barycentrics.y = attribs.barycentrics.x;
	barycentrics.z = attribs.barycentrics.y;
	payload.color = barycentrics;
}
```

**パラメータ説明**:

| パラメータ | 説明 |
|-----------|------|
| `payload` | Ray Generation Shaderから渡された構造体。シェーダー間でデータを受け渡す |
| `attribs` | 衝突した三角形の交差属性。重心座標（Barycentrics）を含む |

**重心座標（Barycentrics）について**:
レイが衝突した三角形上の位置は、3つの頂点の重心座標で表現されます。

```
交差点 = v0 * b0 + v1 * b1 + v2 * b2
  ここで b0 + b1 + b2 = 1.0
```

`BuiltInTriangleIntersectionAttributes::barycentrics`には、b1とb2が格納されており、b0は`1.0 - b1 - b2`で計算できます。

このシェーダーでは、重心座標そのものを色として出力しているため、以下のような色分けが行われます：
- 三角形の頂点0に近い部分：紫（1-u-v, 0, 0） 
- 三角形の頂点1に近い部分：赤（0, u, 0）
- 三角形の頂点2に近い部分：緑（0, 0, v）
- 三角形の中心部分：カラフル（混合色）

---

### Step-5: レイが三角形ポリゴンと衝突しなかった時のカラーを計算

**処理概要**:
`sample.after.fx`のMiss Shader（`miss`）が実行されます。

```hlsl
[shader("miss")]
void miss(inout RayPayload payload)
{
	// Step-5 レイがポリゴンが衝突しなかった時のカラーを計算する
	payload.color = float3(1.0f, 0.0f, 0.0f);
}
```

**説明**:
レイがシーン内のすべてのジオメトリに衝突しなかった場合、このMiss Shaderが実行されます。ここでは、背景色を赤（1.0, 0.0, 0.0）に設定しています。

---

### Ray Generation Shaderの詳細

**処理概要**:
`sample.after.fx`のRay Generation Shader（`rayGen`）は、DispatchRaysの開始時に、各ピクセルごとに実行されます。

```hlsl
[shader("raygeneration")]
void rayGen()
{
	// DispatchRaysで指定された領域内のインデックスを取得
	uint3 launchIndex = DispatchRaysIndex();
	uint3 launchDim = DispatchRaysDimensions();

	// ピクセル座標を正規化座標（-1.0〜1.0）に変換
	float2 crd = float2(launchIndex.xy);
	float2 dims = float2(launchDim.xy);
	float2 d = ((crd / dims) * 2.f - 1.f);

	// レイの設定
	RayDesc ray;
	ray.Origin = g_camera.pos;                                    // レイの開始点（カメラ位置）
	ray.Direction = normalize(float3(d.x * g_camera.aspect, -d.y, -1));
	ray.Direction = mul(g_camera.mCameraRot, ray.Direction);     // カメラの回転を適用
	ray.TMin = 0;                                                 // レイの最小距離
	ray.TMax = 10000;                                             // レイの最大距離

	// ペイロード（レイを射出したときに返却される結果データ）の初期化
	RayPayload payload;

	// レイをシーンに投射（TraceRay API）
	TraceRay(g_raytracingWorld,      // TLAS（Top Level Acceleration Structure）
			 0 /*rayFlags*/,          // レイフラグ
			 0xFF,                    // レイマスク（すべてのジオメトリに対して投射）
			 0 /* ray index*/,        // レイインデックス
			 0,                       // マルチプリケーティビティ（複数のヒットグループを使用しない）
			 0,                       // ミスシェーダーのインデックス
			 ray,                     // レイの定義
			 payload);                // 結果を格納するペイロード

	// 計算結果を出力テクスチャに書き込み
	float3 col = payload.color;
	gOutput[launchIndex.xy] = float4(col, 1);
}
```

**TraceRay APIについて**:
**API**: [TraceRay](https://learn.microsoft.com/windows/win32/direct3dhlsl/traceray-function)

TraceRayは、HLSL内でレイを投射し、加速構造（TLAS）との交差判定を実行する関数です。以下の処理を行います：

1. レイが加速構造内のジオメトリと交差するか判定
2. 最も近い交差点を特定
3. Closest Hit Shaderを実行（交差があった場合）
4. Miss Shaderを実行（交差がなかった場合）
5. ペイロード構造体を通じて結果をRay Generation Shaderに返す

**TraceRayのパラメータ説明**:

| パラメータ | 説明 |
|-----------|------|
| `AccelerationStructure` | TLASのリソース |
| `RayFlags` | レイの振る舞いを制御するフラグ（例：D3D12_RAY_FLAG_CULL_BACK_FACING_TRIANGLES） |
| `InstanceInclusionMask` | インスタンスのマスク値との AND 演算で投射対象を決定 |
| `RayContributionToHitGroupIndex` | ヒットグループテーブルのインデックスのオフセット |
| `MultiplierForGeometryContributionToShaderIndex` | ジオメトリごとのシェーダーインデックスの乗数 |
| `MissShaderIndex` | 衝突しなかった場合に実行するMiss Shaderのインデックス |
| `Ray` | RayDesc構造体（原点、方向、最小・最大距離） |
| `Payload` | 結果を格納する構造体 |

---

### RayPayload構造体

```hlsl
struct RayPayload
{
	float3 color;  // レイが衝突した物体の色、または背景色
};
```

**説明**: Ray Generation ShaderからHit ShaderやMiss Shaderに、そしてHit ShaderやMiss Shaderからほとんどの場合、最終的にはRay Generation Shaderに結果を返すデータ構造です。複雑なシーンでは、複数段階の再帰的な追跡（例：反射・屈折の計算）を行うことも可能です。

---

### Camera構造体（定数バッファ）

```hlsl
struct Camera
{
	float4x4 mCameraRot;    // カメラの回転行列
	float3 pos;             // カメラ座標
	float aspect;           // アスペクト比
	float far;              // 遠平面
	float near;             // 近平面
};

cbuffer rayGenCB :register(b0)
{
	Camera g_camera;
};
```

**説明**: Ray Generation Shaderで使用するカメラ情報です。C++側から毎フレーム更新されます。レジスタb0（定数バッファスロット0）に割り当てられています。

---

## DirectX 12 API・データ構造リファレンス

### 主要なレイトレーシング関連API

#### 1. ID3D12Device5
**説明**: DirectX 12のメインインターフェース。特にレイトレーシング機能を提供します。

**主要なメソッド**:
- `BuildRaytracingAccelerationStructure()`: 加速構造（BLAS/TLAS）を構築
- `CopyRaytracingAccelerationStructure()`: 加速構造をコピー
- `CreateStateObject()`: レイトレーシング用のState Objectを作成
- `GetRaytracingAccelerationStructurePrebuildInfo()`: 加速構造の構築情報を取得

**リンク**: [ID3D12Device5 - Microsoft Learn](https://learn.microsoft.com/windows/win32/api/d3d12/nn-d3d12-id3d12device5)

#### 2. D3D12_RAYTRACING_GEOMETRY_DESC
**説明**: レイトレーシング用のジオメトリ情報を定義します。

**主要なメンバ**:
```cpp
struct D3D12_RAYTRACING_GEOMETRY_DESC
{
	D3D12_RAYTRACING_GEOMETRY_TYPE Type;
	D3D12_RAYTRACING_GEOMETRY_FLAGS Flags;
	union {
		D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC Triangles;
		D3D12_RAYTRACING_GEOMETRY_AABBS_DESC AABBs;
	};
};
```

**リンク**: [D3D12_RAYTRACING_GEOMETRY_DESC - Microsoft Learn](https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_raytracing_geometry_desc)

#### 3. D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC
**説明**: 三角形メッシュのジオメトリ情報を指定します。

**主要なメンバ**:
```cpp
struct D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC
{
	D3D12_GPU_VIRTUAL_ADDRESS Transform3x4;
	DXGI_FORMAT VertexFormat;
	UINT VertexCount;
	UINT VertexStrideInBytes;
	D3D12_GPU_VIRTUAL_ADDRESS VertexBuffer;
	D3D12_GPU_VIRTUAL_ADDRESS IndexBuffer;
	UINT IndexCount;
	DXGI_FORMAT IndexFormat;
};
```

**リンク**: [D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC - Microsoft Learn](https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_raytracing_geometry_triangles_desc)

#### 4. D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC
**説明**: 加速構造（BLAS/TLAS）の構築パラメータを定義します。

**主要なメンバ**:
```cpp
struct D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC
{
	D3D12_GPU_VIRTUAL_ADDRESS DestAccelerationStructureData;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS Inputs;
	D3D12_GPU_VIRTUAL_ADDRESS SourceAccelerationStructureData;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS Flags;
};
```

**リンク**: [D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC - Microsoft Learn](https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_build_raytracing_acceleration_structure_desc)

#### 5. D3D12_RAYTRACING_INSTANCE_DESC
**説明**: TLAS内のインスタンス（BLASの参照）情報を定義します。

**主要なメンバ**:
```cpp
struct D3D12_RAYTRACING_INSTANCE_DESC
{
	FLOAT Transform[3][4];
	UINT InstanceID : 24;
	UINT InstanceMask : 8;
	UINT InstanceContributionToHitGroupIndex : 24;
	UINT Flags : 8;
	D3D12_GPU_VIRTUAL_ADDRESS AccelerationStructure;
};
```

**主要なメンバの説明**:
- `Transform`: ワールド座標系への3x4変換行列
- `InstanceID`: インスタンスの識別子（0〜16777215）
- `InstanceMask`: レイマスク（0〜255）
- `InstanceContributionToHitGroupIndex`: ヒットグループテーブルのオフセット
- `Flags`: インスタンスのフラグ
- `AccelerationStructure`: このインスタンスが参照するBLASのGPUアドレス

**リンク**: [D3D12_RAYTRACING_INSTANCE_DESC - Microsoft Learn](https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_raytracing_instance_desc)

#### 6. ID3D12StateObject
**説明**: レイトレーシング用のパイプラインステートオブジェクト。複数のシェーダーとルートシグネチャを関連付けます。

**State Objectの構成要素**:
- DXIL Library（コンパイルされたシェーダー）
- Root Signature（ルートシグネチャ）
- Subobject（サブオブジェクト：シェーダーとルートシグネチャの関連付け）
- Pipeline Config（パイプラインの設定）

**リンク**: [ID3D12StateObject - Microsoft Learn](https://learn.microsoft.com/windows/win32/api/d3d12/nn-d3d12-id3d12stateobject)

#### 7. D3D12_DISPATCH_RAYS_DESC
**説明**: DispatchRaysコマンドのパラメータを定義します。

**主要なメンバ**:
```cpp
struct D3D12_DISPATCH_RAYS_DESC
{
	D3D12_GPU_VIRTUAL_ADDRESS_RANGE RayGenerationShaderRecord;
	D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE MissShaderTable;
	D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE HitGroupTable;
	D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE CallableShaderTable;
	UINT Width;
	UINT Height;
	UINT Depth;
};
```

**主要なメンバの説明**:
- `RayGenerationShaderRecord`: Ray Generation Shaderのメモリ範囲
- `MissShaderTable`: Miss Shaderテーブル（ストライド情報含む）
- `HitGroupTable`: Hit Groupテーブル（ストライド情報含む）
- `Width`, `Height`, `Depth`: DispatchRaysの実行次元

**リンク**: [D3D12_DISPATCH_RAYS_DESC - Microsoft Learn](https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_dispatch_rays_desc)

#### 8. RayDesc（HLSL）
**説明**: HLSLシェーダー内でレイを定義する構造体。

```hlsl
struct RayDesc
{
	float3 Origin;
	float TMin;
	float3 Direction;
	float TMax;
};
```

**メンバ説明**:
- `Origin`: レイの開始点（ワールド座標）
- `TMin`: レイの最小距離（通常0）
- `Direction`: レイの方向（正規化されていることが推奨）
- `TMax`: レイの最大距離

**リンク**: [RayDesc - Microsoft Learn](https://learn.microsoft.com/windows/win32/direct3dhlsl/raydesc-structure)

#### 9. BuiltInTriangleIntersectionAttributes（HLSL）
**説明**: Hit Shaderに渡される、三角形との交差情報。

```hlsl
struct BuiltInTriangleIntersectionAttributes
{
	float2 barycentrics;  // 重心座標（u, v）。w = 1 - u - v
};
```

**リンク**: [BuiltInTriangleIntersectionAttributes - Microsoft Learn](https://learn.microsoft.com/windows/win32/direct3dhlsl/builtintriangleintersectionattributes)

#### 10. TraceRay（HLSL）
**説明**: HLSLシェーダー内でレイを投射し、加速構造との交差判定を実行。

```hlsl
void TraceRay(
	RaytracingAccelerationStructure AccelerationStructure,
	uint RayFlags,
	uint InstanceInclusionMask,
	uint RayContributionToHitGroupIndex,
	uint MultiplierForGeometryContributionToShaderIndex,
	uint MissShaderIndex,
	RayDesc Ray,
	inout RayPayload Payload
);
```

**パラメータ説明**:
- `AccelerationStructure`: 投射対象のTLAS
- `RayFlags`: レイの振る舞いを制御するフラグ
- `InstanceInclusionMask`: インスタンスのマスク値（ビット AND で投射対象を決定）
- `RayContributionToHitGroupIndex`: ヒットグループテーブルのオフセット
- `MultiplierForGeometryContributionToShaderIndex`: ジオメトリごとのシェーダーインデックスの乗数
- `MissShaderIndex`: Miss Shaderのインデックス
- `Ray`: 投射するレイの定義
- `Payload`: 結果を格納するユーザー定義構造体

**リンク**: [TraceRay - Microsoft Learn](https://learn.microsoft.com/windows/win32/direct3dhlsl/traceray-function)

---

## まとめ

RayTracing によるレンダリングは、以下のステップで実現されます：

1. **初期化フェーズ**: DirectX 12デバイス、コマンドキュー、コマンドリストなどの基盤を構築
2. **モデル登録フェーズ**: 3Dモデルのジオメトリ情報をレイトレワールドに登録
3. **加速構造構築フェーズ**: BLASとTLASを構築し、レイ-ジオメトリ交差判定を高速化
4. **レイディスパッチフェーズ**: DispatchRaysコマンドでGPU上にレイトレーシング計算を指示
5. **シェーダー実行フェーズ**: Ray Generation ShaderがピクセルごとにTraceRayを実行
6. **交差判定と結果計算**: Hit ShaderまたはMiss Shaderが交差情報に基づいて色を計算
7. **結果出力**: 計算結果が出力テクスチャに書き込まれ、フレームバッファに表示

このプロセスを通じて、現代的でリアルなレンダリング結果が実現されます。

---

**参考リンク**:
- [Microsoft Learn - Direct3D 12 API Reference](https://learn.microsoft.com/ja-jp/windows/win32/direct3d12/direct3d-12-reference)
- [Microsoft Learn - Direct3D 12 Ray Tracing](https://learn.microsoft.com/ja-jp/windows/win32/direct3d12/direct3d-12-ray-tracing)
- [NVIDIA DXR Tutorials](https://developer.nvidia.com/rtx/raytracing/dxr/DX12-Raytracing-tutorial-Part-1)


