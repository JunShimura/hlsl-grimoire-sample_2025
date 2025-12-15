/*!
 * @brief ノイズシェーダー
 */

cbuffer cb : register(b0)
{
    float4x4 mvp;       // MVP行列
    float4 mulColor;    // 乗算カラー
};

struct VSInput
{
    float4 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

Texture2D<float4> colorTexture : register(t0); // カラーテクスチャ
sampler Sampler : register(s0);

// ハッシュ関数
float hash( float n )
{
    return frac(sin(n)*43758.5453);
}

// 3次元ベクトルからシンプレックスノイズを生成する関数
// 説明：
// - 入力 `x` はノイズを取得したい連続座標（float3）。
// - グリッドの整数座標 `p = floor(x)` と、各軸での局所座標（0..1）`f = frac(x)` を求める。
// - `f = f*f*(3.0 - 2.0*f)` により補間係数を滑らかに（Hermite 補間 / smoothstep 相当）して境界での連続性を高める。
// - `n = p.x + p.y * 57.0 + 113.0 * p.z` は各格子点を一意に識別するための基礎値（異なる素数係数で座標を混ぜることでハッシュの衝突を低減）。
// - `hash(n + offsets)` を使って、現在の格子を構成する8つの頂点それぞれの擬似乱数値（0..1）を取得する。
// - 最後に三方向にわたって三重線形補間（lerp の入れ子）を行い、格子内の位置に対応するノイズ値を算出する。
// - 出力範囲は 0.0 .. 1.0（呼び出し側で -1..1 に変換して使うことが多い）
// 実装上は簡易な値ノイズ（value noise）で、パーフェクトなカテゴリカルな「Simplex ノイズ」アルゴリズムとは異なりますが、見た目のノイズ生成には十分です。


float SimplexNoise(float3 x)
{
    // The noise function returns a value in the range -1.0f -> 1.0f
    float3 p = floor(x);
    float3 f = frac(x);

    f = f * f * (3.0 - 2.0 * f);
    float n = p.x + p.y * 57.0 + 113.0 * p.z;

    return lerp(lerp(lerp(hash(n + 0.0), hash(n + 1.0), f.x),
                     lerp(hash(n + 57.0), hash(n + 58.0), f.x), f.y),
                lerp(lerp(hash(n + 113.0), hash(n + 114.0), f.x),
                     lerp(hash(n + 170.0), hash(n + 171.0), f.x), f.y), f.z);
}

PSInput VSMain(VSInput In)
{
    PSInput psIn;
    psIn.pos = mul(mvp, In.pos);
    psIn.uv = In.uv;
    return psIn;
}

float4 PSMain(PSInput In) : SV_Target0
{
    // step-1 シンプレックスノイズを利用してノイズ加工を行う
    // シンプレックスノイズを使用して0.0～1.0の範囲のノイズ値を取得
	float t = SimplexNoise(In.pos.xyz);

    // ノイズの範囲を0～1から-1～1に変換
	t = (t - 0.5f) * 2.0f;
    
    // UV座標にノイズを加える。0.01はノイズの強さ
    // この数値を調整することでノイズの強さを変えられる
	float2 uv = In.uv + t * 0.03125f;
    
    // ずらしたUV座標を利用して、カラーをサンプリングする
	float4 color = colorTexture.Sample(Sampler, uv);
    
    return color;
}
