/*!
 * @brief セピア調加工
 */

cbuffer cb : register(b0)
{
    float4x4 mvp;           // MVP行列
    float4 mulColor;        // 乗算カラー
};

cbuffer SepiaCB : register( b1 )
{
    float sepiaRate;        // セピア率
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

PSInput VSMain(VSInput In)
{
    PSInput psIn;
    psIn.pos = mul(mvp, In.pos);
    psIn.uv = In.uv;
    return psIn;
}

float4 PSMain(PSInput In) : SV_Target0
{
    float4 color = colorTexture.Sample(Sampler, In.uv);

    // step-1 画像を徐々にセピア調に変化させていく
    // ピクセルの明るさを計算する
	float Y = dot(color.rgb, float3(0.299, 0.587, 0.114));
    // セピア調ではモノクロ化とは違い、R,G,Bに明るさをそのまま代入しない
    // 今回の実装では赤味の成分に0.9．緑に0.7、青に0.4を乗算してセピア調に変化させる
	float3 sepiaColor = float3(Y * 0.9f, Y * 0.7f, Y * 0.4f);
    
    // セピア率に応じて元の色とセピア調の色を線形補間する
	color.rgb = lerp(color.rgb, sepiaColor, sepiaRate);
    return color;
}
