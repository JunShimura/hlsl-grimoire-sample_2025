# sample.fx 解説メモ

このファイルは、`sample.fx` の内容を **HLSL / DirectX 初心者向け** に説明したものです。

難しい言葉はできるだけ避けて、**中学生でも読めるくらいのやさしさ** を目標にしています。

---

## 1. このシェーダーは何をしているのか

`sample.fx` は、DirectX Raytracing を使って物体を描くためのシェーダーです。

このファイルでは主に次のことをしています。

- カメラから画面の各ピクセルへレイを飛ばす
- 物体に当たった場所の色や法線を調べる
- 光が当たるかどうかを調べる
- 反射レイを飛ばす
- 屈折レイを飛ばす
- 反射・屈折・元の色を混ぜて最終色を決める

つまり、

**「光がどう跳ね返るか、どう通り抜けるか」を計算して絵を作る**

のがこのファイルの役目です。

---

## 2. まずは超ざっくり全体の流れ

このシェーダー全体の流れは次の通りです。

1. `rayGen()` で画面の1ピクセルごとにレイを飛ばす
2. レイが物体に当たらなければ `miss()` が呼ばれる
3. レイが物体に当たれば `chs()` が呼ばれる
4. `chs()` の中で
   - UVを求める
   - 法線を求める
   - 影を調べる
   - 反射を調べる
   - 屈折を調べる
   - それらを混ぜて最終色を作る
5. できた色を `gOutput` に書く

---

## 3. ファイルの前半にあるデータの意味

## 3.1 `SVertex`

```hlsl
struct SVertex
{
	float3 pos;
	float3 normal;
	float3 tangent;
	float3 binormal;
	float2 uv;
	int4 indices;
	float4 skinweigths;
};
```

1頂点ぶんの情報です。

- `pos` : 頂点の位置
- `normal` : 面の向き
- `tangent` : 表面の横方向
- `binormal` : 表面の縦方向
- `uv` : テクスチャ座標

このシェーダーで特に大事なのは `normal`、`tangent`、`uv` です。

---

## 3.2 `Camera`

```hlsl
struct Camera
{
	float4x4 mCameraRot;
	float3 pos;
	float aspect;
	float far;
	float near;
};
```

カメラ情報です。

- `mCameraRot` : カメラの向き
- `pos` : カメラの位置
- `aspect` : 横幅と縦幅の比率
- `far`, `near` : 見える距離の範囲

---

## 3.3 GPUリソース

```hlsl
RaytracingAccelerationStructure g_raytracingWorld : register(t0);
Texture2D<float4> gAlbedoTexture : register(t1);
Texture2D<float4> g_normalMap : register(t2);
Texture2D<float4> g_specularMap : register(t3);
Texture2D<float4> g_reflectionMap : register(t4);
Texture2D<float4> g_refractionMap : register(t5);
StructuredBuffer<SVertex> g_vertexBuffers : register(t6);
StructuredBuffer<int> g_indexBuffers : register(t7);
RWTexture2D<float4> gOutput : register(u0);
SamplerState s : register(s0);
```

ざっくり言うとこうです。

- `g_raytracingWorld` : レイを当てる世界データ
- `gAlbedoTexture` : 物体の基本色
- `g_normalMap` : 凹凸っぽく見せる法線マップ
- `g_reflectionMap` : どれくらい反射するか
- `g_refractionMap` : どれくらい屈折するか、または屈折率
- `g_vertexBuffers` : 頂点データ
- `g_indexBuffers` : 三角形を作るための番号
- `gOutput` : 最後に描き込む画像
- `s` : テクスチャの読み方

### 初心者向け補足

- `t0`, `t1` の `t` はテクスチャやSRV系の番号
- `u0` の `u` は書き込み可能なテクスチャ
- `s0` の `s` はサンプラー
- `b0` の `b` は定数バッファ

「どの入れ物をどの番号で使うか」を決めています。

---

## 4. `RayPayload` とは

```hlsl
struct RayPayload
{
	float3 color;
	int hit;
	int depth;
};
```

レイといっしょに運ぶデータです。

- `color` : レイが持ち帰る色
- `hit` : 何かに当たったか
- `depth` : 何回目の跳ね返りか

`depth` はとても大事です。
これがないと、反射や屈折がずっと続いて止まらなくなることがあります。

---

## 5. UVを求める `GetUV()`

```hlsl
float2 GetUV(BuiltInTriangleIntersectionAttributes attribs)
```

この関数は、レイが三角形のどこに当たったかから、
その場所のUV座標を計算します。

中で使っているのは **バリセントリック座標** です。

### バリセントリック座標をやさしく言うと

三角形の中の点を、

- 頂点Aを何割
- 頂点Bを何割
- 頂点Cを何割

で表す考え方です。

3つの割合を足すと 1 になります。

```hlsl
float3 barycentrics = float3(
	1.0 - attribs.barycentrics.x - attribs.barycentrics.y,
	attribs.barycentrics.x,
	attribs.barycentrics.y);
```

これは、A・B・Cの混ぜる割合を作っています。

そのあとで

```hlsl
float2 uv = barycentrics.x * uv0 + barycentrics.y * uv1 + barycentrics.z * uv2;
```

として、3頂点のUVを割合で混ぜています。

### 数学ポイント

これは **重み付き平均** です。

たとえばテストの点数の平均を、
「国語2割、数学3割、英語5割」で出すのと似ています。

---

## 6. 法線を求める `GetNormal()`

```hlsl
float3 GetNormal(BuiltInTriangleIntersectionAttributes attribs, float2 uv)
```

この関数は、当たった位置の法線を求めます。
しかも、ただの頂点法線ではなく、**法線マップ込みの法線** にしています。

流れはこうです。

1. 頂点法線を3頂点から補間する
2. 接線 `tangent` を3頂点から補間する
3. `cross()` で `binormal` を作る
4. 法線マップを読む
5. 法線マップの値をワールド寄りの向きへ変換する

### 法線とは

法線は、面からまっすぐ立てた矢印です。
光の当たり方を計算するときに重要です。

### `cross(tangent, normal)` とは

`cross()` は **外積** です。

難しそうですが、ここでは

**「2本の矢印に対して、両方に直角な新しい矢印を作る」**

くらいで大丈夫です。

それで `binormal` を作っています。

### 法線マップの値変換

```hlsl
float3 binSpaceNormal = g_normalMap.SampleLevel (s, uv, 0.0f).xyz;
binSpaceNormal = (binSpaceNormal * 2.0f) - 1.0f;
```

テクスチャの色は普通 0.0 ～ 1.0 の範囲です。
でも法線ベクトルは -1.0 ～ 1.0 を使いたいので、

- 0.0 → -1.0
- 0.5 → 0.0
- 1.0 → 1.0

に変換しています。

### 最後の合成

```hlsl
normal = tangent * binSpaceNormal.x +
		 binormal * binSpaceNormal.y +
		 normal * binSpaceNormal.z;
```

これは

- 横方向成分
- 縦方向成分
- 面の正面方向成分

を足して、最終的な法線を作っています。

---

## 7. 影を調べる `TraceLightRay()`

```hlsl
void TraceLightRay(inout RayPayload raypayload, float3 normal)
```

この関数は、当たった場所から光の方向へ向かってレイを飛ばします。

目的は1つです。

**その場所から光が見えているかどうかを調べること**

もし途中で別の物体に当たれば、そこは影です。

### ここでやっていること

```hlsl
float3 posW = rayOriginW + hitT * rayDirW;
```

これはレイの式です。

- 出発点 + 進む向き × 距離

で、当たった位置を求めています。

中学向けに言うと、

**「今いる場所から、矢印の向きへ、hitTだけ進んだ場所」**

です。

---

## 8. 反射を調べる `TraceReflectionRay()`

```hlsl
float3 refDir = reflect(rayDirW, normal);
```

`reflect()` は反射方向を求める関数です。

鏡を思い出すとわかりやすいです。

- 入ってきた角度
- 跳ね返る角度

が同じになる方向を計算します。

### 深さ制限

```hlsl
if(raypayload.depth < 3)
```

反射が反射を呼んで無限に続くのを防ぐため、
3回までに制限しています。

---

## 9. 屈折を調べる `TraceRefractionRay()`

このサンプルの見どころの1つです。

```hlsl
void TraceRefractionRay(
	inout RayPayload raypayload,
	float3 normal,
	float2 uv,
	out float3 orientedNormal,
	out float fresnel)
```

この関数は、ガラスや水のように、光が物体の中へ入るときの向きを計算します。

### 9.1 屈折率を読む

```hlsl
float kRefractiveIndex = g_refractionMap.SampleLevel(s, uv, 0.0f).r;
```

ここでは屈折率をテクスチャから読んでいます。

屈折率は、

**「光が曲がりやすい度合い」**

です。

たとえば、

- 空気 : だいたい 1.0
- 水 : だいたい 1.33
- ガラス : だいたい 1.5

です。

値が大きいほど、光は曲がりやすくなります。

### 9.2 物体の内側か外側かを判定

```hlsl
if(dot(rayDirW, orientedNormal) > 0.0f)
```

`dot()` は **内積** です。

ここでは細かい式の意味より、

**「レイが法線と同じ向き側へ進んでいるか」**

を調べています。

その結果、

- 外から中へ入る
- 中から外へ出る

を見分けています。

中から外へ出るときは、法線の向きをひっくり返して、屈折率の入れ替えも行います。

---

### 9.3 Fresnel を計算

```hlsl
float cosTheta = saturate(dot(-rayDirW, orientedNormal));
float r0 = (etaI - etaT) / (etaI + etaT);
r0 *= r0;
fresnel = r0 + (1.0f - r0) * pow(1.0f - cosTheta, 5.0f);
```

Fresnel は、

**見る角度によって反射の強さが変わる現象**

です。

たとえば窓ガラスは、正面から見ると向こう側が見えやすく、
斜めから見ると反射が強く見えます。

この式はその感じを作っています。

### `pow(x, 5.0f)` の意味

`pow(a, b)` は `a` の `b` 乗です。

たとえば

- `pow(2, 3)` は 8

です。

ここでは5乗を使って、角度変化を強めています。

---

### 9.4 屈折方向を求める

```hlsl
float3 refrDir = refract(rayDirW, orientedNormal, eta);
```

`refract()` は屈折方向を返します。

ストローを水に入れると、折れて見えます。
あの「曲がる向き」を計算する関数だと思えば大丈夫です。

---

### 9.5 全反射

```hlsl
if(dot(refrDir, refrDir) > 0.0001f)
```

屈折できない場合があります。
それが **全反射** です。

たとえば水の中から水面を斜めに見たとき、外が見えずに反射だけになることがあります。

このコードでは、屈折方向ベクトルがほぼゼロかどうかを見て、
屈折できないなら

```hlsl
fresnel = 1.0f;
```

として、完全反射にしています。

---

## 10. `rayGen()` は何をしているか

```hlsl
[shader("raygeneration")]
void rayGen()
```

これはレイトレーシングのスタート地点です。
画面の各ピクセルに対して呼ばれます。

### 主な処理

```hlsl
uint3 launchIndex = DispatchRaysIndex();
uint3 launchDim = DispatchRaysDimensions();
```

- `DispatchRaysIndex()` : 今処理中のピクセル番号
- `DispatchRaysDimensions()` : 画面全体の大きさ

そのあと

```hlsl
float2 d = ((crd / dims) * 2.f - 1.f);
```

で画面座標を -1 ～ 1 の範囲へ変換しています。

### なぜ -1 ～ 1 にするのか

画面の左端を -1、右端を 1、上や下も同じようにすると、
中央が 0 になって扱いやすいからです。

### レイを作る部分

```hlsl
ray.Origin = g_camera.pos;
ray.Direction = normalize(float3(d.x * g_camera.aspect, -d.y, -1));
ray.Direction = mul(g_camera.mCameraRot, ray.Direction);
```

- 出発点はカメラ位置
- 方向はピクセルの位置から決める
- `mul()` でカメラの回転を反映する

### `normalize()` の意味

ベクトルの長さを1にそろえる関数です。

長さを1にすると、向きだけをきれいに扱えます。
地図で「東向き」を比べるときに、矢印の長さがバラバラだと見にくいのと同じです。

---

## 11. `miss()` は何をしているか

```hlsl
[shader("miss")]
void miss(inout RayPayload payload)
{
	payload.color = float3(0.1, 0.0, 0.0);
}
```

レイが何にも当たらなかったときの色を決めています。

このサンプルでは少し赤っぽい暗い色です。
背景色のようなものです。

---

## 12. いちばん大事な `chs()` の流れ

```hlsl
[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
```

ここがメイン処理です。

### 流れ

1. `payload.depth++` で再帰回数を増やす
2. `GetUV()` でUV取得
3. `GetNormal()` で法線取得
4. 法線を回転行列で向きを調整
5. `TraceLightRay()` で影を調べる
6. 明るさ `lig` を作る
7. 反射用レイを飛ばす
8. 屈折用レイを飛ばす
9. 反射率・屈折率・アルベド色を読む
10. それらを混ぜて最終色を作る
11. `payload.depth--` で戻す

---

## 13. 明るさ `lig` の計算

```hlsl
float t = max(0.0f, dot(ligDir, normal));
lig = t;
lig += 0.5f;
```

### 何をしているか

- 光の向きと法線の向きが近いほど明るい
- 反対向きなら暗い
- 最低限の明るさとして環境光 `0.5` を足す

### `dot()` のイメージ

2本の矢印が

- 同じ向きに近い → 大きい
- 直角に近い → 0 に近い
- 反対向きに近い → マイナス

になります。

この性質を使って「光がどれだけ正面から当たっているか」を調べます。

### `max(0.0f, ...)` の意味

暗くなりすぎたり、マイナスの明るさにならないようにしています。

---

## 14. 法線の回転行列

```hlsl
float cs = cos(-1.57f);
float sn = sin(-1.57f);
```

ここでは法線を回転させています。
`-1.57` はだいたい `-90度` です。

### 行列とは

行列は、

- 回転する
- 拡大する
- 移動する

といった変形をまとめて扱うための表のようなものです。

初心者向けには、

**「座標や向きを変換するためのルール表」**

と思えば大丈夫です。

### `transpose()` の意味

行列の縦横を入れ替える関数です。
行列の向きの取り扱いを合わせるために使っています。

---

## 15. 最終色の合成

```hlsl
float reflectRate = g_reflectionMap.SampleLevel(s, uv, 0.0f).r;
float refractRate = g_refractionMap.SampleLevel(s, uv, 0.0f).r;

float3 color = gAlbedoTexture.SampleLevel(s, uv, 0.0f).rgb;
color *= lig * kAlbedoStrength;

float transmissionRate = (1.0f - fresnel) * refractRate;
float reflectionRate = 1.0f - transmissionRate;
float3 reflRefrCombined =
	refPayload.color * reflectionRate +
	refrPayload.color * transmissionRate;
payload.color = lerp(color, reflRefrCombined, reflectRate);
```

ここでは

- 元の色
- 反射色
- 屈折色

を混ぜています。

### `lerp(a, b, t)` の意味

`lerp()` は線形補間です。

- `t = 0` なら `a`
- `t = 1` なら `b`
- `t = 0.5` なら半分ずつ

です。

ジュースを混ぜるイメージに近いです。

- オレンジ 70%
- りんご 30%

のような混ぜ方です。

---

## 16. 影専用の `shadowChs()` と `shadowMiss()`

```hlsl
[shader("closesthit")]
void shadowChs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.hit = 1;
}

[shader("miss")]
void shadowMiss(inout RayPayload payload)
{
   payload.hit = 0;
}
```

これは影判定用です。

- 何かに当たった → `hit = 1`
- 当たらなかった → `hit = 0`

とてもシンプルです。

影用レイは「何に当たったか」より、
**光までの間に邪魔があるか** だけわかればいいからです。

---

## 17. このファイルで使われている数学をやさしく整理

## 17.1 ベクトル

`float3(1, 2, 3)` のような3つ組です。

これは

- 座標
- 向き
- 速度

などを表せます。

このシェーダーでは主に **向きの矢印** として使っています。

---

## 17.2 正規化 `normalize()`

矢印の長さを1にそろえます。

向きだけ比べたいときに便利です。

---

## 17.3 内積 `dot(a, b)`

2本の矢印がどれだけ同じ向きを向いているかを調べます。

- 同じ向きに近い → 大きい
- 直角 → 0
- 反対向き → マイナス

照明計算や、内側・外側判定に使います。

---

## 17.4 外積 `cross(a, b)`

2本の矢印の両方に直角な矢印を作ります。

ここでは接線から `binormal` を作るのに使っています。

---

## 17.5 反射 `reflect(i, n)`

入ってきた方向 `i` と法線 `n` から、鏡のように跳ね返る向きを出します。

---

## 17.6 屈折 `refract(i, n, eta)`

入ってきた方向 `i` と法線 `n` と屈折率比 `eta` から、
曲がって進む向きを出します。

---

## 17.7 補間

`GetUV()` や `GetNormal()` でやっているのは補間です。

2つや3つの値の間を、割合でなめらかにつなぐことです。

ゲームの見た目を自然にするのにとても大事です。

---

## 18. HLSL特有の関数・書き方の解説

## 18.1 `[shader("raygeneration")]`

この関数がレイ生成シェーダーだと示します。

---

## 18.2 `[shader("miss")]`

レイが何にも当たらなかったときに呼ばれます。

---

## 18.3 `[shader("closesthit")]`

レイが当たった中で、いちばん近い場所に対して呼ばれます。

---

## 18.4 `TraceRay()`

レイを実際に飛ばす関数です。

このシェーダーで最重要クラスの関数の1つです。

- カメラからレイを飛ばす
- 影用レイを飛ばす
- 反射レイを飛ばす
- 屈折レイを飛ばす

の全部で使っています。

---

## 18.5 `DispatchRaysIndex()`

今処理しているピクセルの番号を返します。

---

## 18.6 `DispatchRaysDimensions()`

全体のピクセル数を返します。

---

## 18.7 `PrimitiveIndex()`

今当たっている三角形の番号を返します。

---

## 18.8 `WorldRayOrigin()`

今のレイの出発点を返します。

---

## 18.9 `WorldRayDirection()`

今のレイの進行方向を返します。

---

## 18.10 `RayTCurrent()`

今のレイが当たった距離を返します。

`位置 = 出発点 + 方向 × 距離`

で当たり位置を求めるときに使います。

---

## 18.11 `SampleLevel()`

テクスチャを読む関数です。

```hlsl
gAlbedoTexture.SampleLevel(s, uv, 0.0f)
```

は

- `s` というサンプラーで
- `uv` の場所を
- mip level 0 で読む

という意味です。

初心者向けには、

**「テクスチャ画像のこの場所の色を読む」**

で大丈夫です。

---

## 18.12 `mul()`

行列とベクトルを掛けて変換します。

ここではカメラ回転や法線回転に使っています。

---

## 18.13 `saturate()`

値を 0 ～ 1 の範囲におさめます。

- 0未満なら 0
- 1より大きければ 1

にします。

---

## 18.14 `lerp(a, b, t)`

2つの値を `t` の割合で混ぜます。

---

## 18.15 `register(...)`

GPU側のどのスロットに割り当てるかを指定します。

DirectXでリソースを結びつけるために必要です。

---

## 18.16 `cbuffer`

定数データを入れる入れ物です。

このファイルではカメラ情報を入れています。

---

## 19. 初心者が混乱しやすい点

## 19.1 `g_refractionMap` が2つの意味で使われている

このコードでは `g_refractionMap` が

- 屈折率として使われる場所
- 屈折の混ぜ具合として使われる場所

の両方に出てきます。

```hlsl
float kRefractiveIndex = g_refractionMap.SampleLevel(s, uv, 0.0f).r;
float refractRate = g_refractionMap.SampleLevel(s, uv, 0.0f).r;
```

これはサンプルとしては動きますが、意味としては少し混ざっています。

本格的に作るなら、たとえば

- 屈折率用マップ
- 透過率用マップ

を分けるほうがわかりやすいです。

---

## 19.2 `payload.depth` の増減

```hlsl
payload.depth++;
...
payload.depth--;
```

これは再帰の管理です。

関数を出る前に戻しておかないと、
次の計算に影響が残ることがあります。

---

## 19.3 法線の回転が少し力技

```hlsl
// ワールド空間に変換するのがめんどいので、超適当に・・・。さーせん
```

コメントにもある通り、ここは厳密な実装というよりサンプル寄りです。

学習用としては

- 法線の向きを合わせるための調整をしている

と理解すれば十分です。

---

## 20. このファイルをひとことで言うと

`sample.fx` は、

**「カメラからレイを飛ばし、当たった場所で色・影・反射・屈折を計算して、最終的な絵を作る HLSL のレイトレーシングシェーダー」**

です。

---

## 21. 最後に: 初心者向けの読み方

もし最初から全部理解しようとすると大変です。
次の順番で読むとわかりやすいです。

1. `rayGen()` を読む
2. `miss()` を読む
3. `chs()` を読む
4. `GetUV()` と `GetNormal()` を読む
5. `TraceLightRay()` を読む
6. `TraceReflectionRay()` を読む
7. `TraceRefractionRay()` を読む

まずは

- レイを飛ばす
- 当たる
- 色を決める

の流れが見えれば十分です。

そのあとで

- 内積
- 外積
- 反射
- 屈折
- Fresnel

を少しずつ覚えると理解しやすくなります。

---

## 22. 用語ミニ辞典

- **レイ** : 光線のようなもの
- **法線** : 面から垂直に出る向き
- **UV** : テクスチャの位置
- **補間** : 値と値の間をなめらかにつなぐこと
- **反射** : 鏡のように跳ね返ること
- **屈折** : 水やガラスで光が曲がること
- **Fresnel** : 角度によって反射の強さが変わる現象
- **Payload** : レイが持ち運ぶデータ
- **Miss Shader** : 何にも当たらなかったときの処理
- **Closest Hit Shader** : いちばん近くに当たった場所の処理

---

必要なら次は、`sample.fx` を

- 行番号つきで順番に解説した版
- 図を入れた版
- 反射と屈折だけに絞った版

として分けて作れます。
