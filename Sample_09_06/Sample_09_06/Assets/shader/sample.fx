/*!
 * @brief チェッカーボードワイプ
 */

cbuffer cb : register(b0)
{
    float4x4 mvp;       // MVP行列
    float4 mulColor;    // 乗算カラー
};

cbuffer WipeCB : register(b1)
{
    float wipeSize;     // ワイプサイズ
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

//	float2 v1 = normalize(float2(3.0, 4.0));
//	float2 v2 = normalize(float2(3.0, 2.0));
//	float2 v3 = float2(dot(v1, In.pos.xy), dot(v2, In.pos.xy));

	float2 v3 = float2(
        dot(float2(1.0f, 0.0f), float2(640, 320) - In.pos.xy)*100,
    	length(float2(640, 320) - In.pos.xy)
    );


    
    // step-1 チェッカーボードワイプを実装する
    // 1.ピクセルのY座標を128で割って、小数点以下を切り捨て、行番号を求める
	// float t = floor(In.pos.y / 128.0f);
	// float t = fmod(In.pos.y, 128.0f) / 128.0f;
	float t = floor(v3.y / 128.0f);
    
    
    // 2.行番号を2で割った余りを求める。偶数ならばtは0, 奇数ならばtは1になる
	t = fmod(t, 2.0f);
    
    // 3.奇数行ならX座標を64ずらして、縦じまのワイプ処理を行う
	// t = (int) fmod(In.pos.x + 64.0 * t, 128.0f);
	t = (int) fmod(v3.x + 64.0 * t, 128.0f);
	clip(t - wipeSize);

    return color;
}
