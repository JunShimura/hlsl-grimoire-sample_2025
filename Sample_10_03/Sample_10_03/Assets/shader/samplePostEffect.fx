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

Texture2D<float4> sceneTexture : register(t0); // シーンテクスチャ
sampler Sampler : register(s0);

PSInput VSMain(VSInput In)
{
    PSInput psIn;
    psIn.pos = mul(mvp, In.pos);
    psIn.uv = In.uv;
    return psIn;
}

float4 PSMain(PSInput In) : SV_Target0
{
	float4 color = { 0.5f, 0.5f, 0.5f, 1.0f }; //
	// sceneTexture.Sample(Sampler, In.uv);


    // step-1 基準テクセル+近傍8テクセルの平均を計算する
    // 2.5テクセルずらすためのUV値を求める
	float offsetU = 1.0f / 1280.0f; // 横解像度で割る
	float offsetV = 1.0f / 720.0f; // 縦解像度で割る
    
    // 9つのテクセルの色を合計する
    // ┌
	color -= sceneTexture.Sample(Sampler, In.uv + float2(-offsetU, -offsetV));
	// ↑
    color += sceneTexture.Sample(Sampler, In.uv + float2(0.0f, -offsetV));
    // ┐
	color += sceneTexture.Sample(Sampler, In.uv + float2(offsetU, -offsetV));
	// ←
    color += sceneTexture.Sample(Sampler, In.uv + float2(-offsetU, 0.0f));
	// →
    color += sceneTexture.Sample(Sampler, In.uv + float2(offsetU, 0.0f));
	// └
    color += sceneTexture.Sample(Sampler, In.uv + float2(-offsetU, offsetV));
	// ↓
    color += sceneTexture.Sample(Sampler, In.uv + float2(0.0f, offsetV));
	// ┘
    color += sceneTexture.Sample(Sampler, In.uv + float2(offsetU, offsetV));
    
	color /= 9.0f; // 合計を9で割って平均を求める

    return color;
}
