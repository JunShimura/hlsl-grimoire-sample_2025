/*!
 * @brief ブルーム
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

/*!
 * @brief 頂点シェーダー
 */
PSInput VSMain(VSInput In)
{
    PSInput psIn;
    psIn.pos = mul(mvp, In.pos);
    psIn.uv = In.uv;
    return psIn;
}

Texture2D<float4> mainRenderTargetTexture : register(t0); // メインレンダリングターゲットのテクスチャ
sampler Sampler : register(s0);

/////////////////////////////////////////////////////////
// ブラー
/////////////////////////////////////////////////////////
struct PS_BlurInput
{
    float4 pos : SV_POSITION;
    float4 tex0 : TEXCOORD0;
    float4 tex1 : TEXCOORD1;
    float4 tex2 : TEXCOORD2;
    float4 tex3 : TEXCOORD3;
    float4 tex4 : TEXCOORD4;
    float4 tex5 : TEXCOORD5;
    float4 tex6 : TEXCOORD6;
    float4 tex7 : TEXCOORD7;
};

Texture2D<float4> sceneTexture : register(t0); // シーンテクスチャ

/*!
 * @brief ブラー用の定数バッファー
 */
cbuffer CBBlur : register(b1)
{
    float4 weight[2]; // 重み
}

/*!
 * @brief 横ブラー頂点シェーダー
 */
PS_BlurInput VSXBlur(VSInput In)
{
    // step-13 横ブラー用の頂点シェーダーを実装
	PS_BlurInput Out;
    
    // テクスチャ座標のオフセット値
	Out.pos = mul(mvp, In.pos);
    
    // テクスチャサイズを取得
	float2 texSize;
	float level;
	sceneTexture.GetDimensions(0, texSize.x, texSize.y, level);
    
    // 基準テクセルのUV座業を記録
	float2 tex = In.uv;
    
    // 基準テクセルからU座標を+1 テクセルずらすためのオフセットを計算する
	Out.tex0.xy = float2(1.0f / texSize.x, 0.0f); 
    
    // 基準テクセルからU座標を+3 テクセルずらすためのオフセットを計算する
	Out.tex1.xy = float2(3.0f / texSize.x, 0.0f);
    
    // 基準テクセルからU座標を+5 テクセルずらすためのオフセットを計算する
	Out.tex2.xy = float2(5.0f / texSize.x, 0.0f);
    
    // 基準テクセルからU座標を+7 テクセルずらすためのオフセットを計算する
	Out.tex3.xy = float2(7.0f / texSize.x, 0.0f);
    
    // 基準テクセルからU座標を+9 テクセルずらすためのオフセットを計算する
	Out.tex4.xy = float2(9.0f / texSize.x, 0.0f);
    
    // 基準テクセルからU座標を+11 テクセルずらすためのオフセットを計算する
	Out.tex5.xy = float2(11.0f / texSize.x, 0.0f);
    
    // 基準テクセルからU座標を+13 テクセルずらすためのオフセットを計算する
	Out.tex6.xy = float2(13.0f / texSize.x, 0.0f);
    
    // 基準テクセルからU座標を+15 テクセルずらすためのオフセットを計算する
	Out.tex7.xy = float2(15.0f / texSize.x, 0.0f);
    
    // オフセットに-1を掛けてマイナス方向のオフセットも計算する
	Out.tex0.zw = -Out.tex0.xy;
	Out.tex1.zw = -Out.tex1.xy;
	Out.tex2.zw = -Out.tex2.xy;
	Out.tex3.zw = -Out.tex3.xy;
	Out.tex4.zw = -Out.tex4.xy;
	Out.tex5.zw = -Out.tex5.xy;
	Out.tex6.zw = -Out.tex6.xy;
	Out.tex7.zw = -Out.tex7.xy;
    
    // オフセットに基準テクセルのUV座標を加算して最終的なテクスチャ座標を計算する
	Out.tex0 += float4(tex, tex);
	Out.tex1 += float4(tex, tex);
	Out.tex2 += float4(tex, tex);
	Out.tex3 += float4(tex, tex);
	Out.tex4 += float4(tex, tex);
	Out.tex5 += float4(tex, tex);
	Out.tex6 += float4(tex, tex);
	Out.tex7 += float4(tex, tex);
    
    return Out;
}

/*!
 * @brief 縦ブラー頂点シェーダー
 */
PS_BlurInput VSYBlur(VSInput In)
{
    // step-14 縦ブラー用の頂点シェーダーを実装
	PS_BlurInput Out;
    
    // 座標変換
	Out.pos = mul(mvp, In.pos);
    
    // テクスチャサイズを取得
	float2 texSize;
	float level;
	sceneTexture.GetDimensions(0, texSize.x, texSize.y, level);
    
    // 基準テクセルのUV座業を記録
	float2 tex = In.uv;
    
    // 基準テクセルからV座標を+1 テクセルずらすためのオフセットを計算する
	Out.tex0.xy = float2(0.0f, 1.0f / texSize.y);
    
    // 基準テクセルからV座標を+3 テクセルずらすためのオフセットを計算する
	Out.tex1.xy = float2(0.0f, 3.0f / texSize.y);
    
    // 基準テクセルからV座標を+5 テクセルずらすためのオフセットを計算する
	Out.tex2.xy = float2(0.0f, 5.0f / texSize.y);
    
    // 基準テクセルからV座標を+7 テクセルずらすためのオフセットを計算する
	Out.tex3.xy = float2(0.0f, 7.0f / texSize.y);
    
    // 基準テクセルからV座標を+9 テクセルずらすためのオフセットを計算する
	Out.tex4.xy = float2(0.0f, 9.0f / texSize.y);
    
    // 基準テクセルからV座標を+11 テクセルずらすためのオフセットを計算する
	Out.tex5.xy = float2(0.0f, 11.0f / texSize.y);
    
    // 基準テクセルからV座標を+13 テクセルずらすためのオフセットを計算する
	Out.tex6.xy = float2(0.0f, 13.0f / texSize.y);
    
    // 基準テクセルからV座標を+15 テクセルずらすためのオフセットを計算する
	Out.tex7.xy = float2(0.0f, 15.0f / texSize.y);
    
    // オフセットに-1を掛けてマイナス方向のオフセットも計算する
	Out.tex0.zw = -Out.tex0.xy;
	Out.tex1.zw = -Out.tex1.xy;
	Out.tex2.zw = -Out.tex2.xy;
	Out.tex3.zw = -Out.tex3.xy;
	Out.tex4.zw = -Out.tex4.xy;
	Out.tex5.zw = -Out.tex5.xy;
	Out.tex6.zw = -Out.tex6.xy;
	Out.tex7.zw = -Out.tex7.xy;
    
    // オフセットに基準テクセルのUV座標を加算して最終的なテクスチャ座標を計算する
	Out.tex0 += float4(tex, tex);
	Out.tex1 += float4(tex, tex);
	Out.tex2 += float4(tex, tex);
	Out.tex3 += float4(tex, tex);
	Out.tex4 += float4(tex, tex);
	Out.tex5 += float4(tex, tex);
	Out.tex6 += float4(tex, tex);
	Out.tex7 += float4(tex, tex);
    
    return Out;
}

/*!
 * @brief ブラーピクセルシェーダー
 */
float4 PSBlur(PS_BlurInput In) : SV_Target0
{
    // step-15 横、縦ブラー用のピクセルシェーダーを実装
	float4 Color;
    
    // 基準テクセルからプラス方向に8テクセル、重み付きでサンプリング
	Color = weight[0].x * sceneTexture.Sample(Sampler, In.tex0.xy);
    
	Color += weight[0].y * sceneTexture.Sample(Sampler, In.tex1.xy);
	Color += weight[0].z * sceneTexture.Sample(Sampler, In.tex2.xy);
	Color += weight[0].w * sceneTexture.Sample(Sampler, In.tex3.xy);
	Color += weight[0].x * sceneTexture.Sample(Sampler, In.tex4.xy);
	Color += weight[0].y * sceneTexture.Sample(Sampler, In.tex5.xy);
	Color += weight[0].z * sceneTexture.Sample(Sampler, In.tex6.xy);
	Color += weight[0].w * sceneTexture.Sample(Sampler, In.tex7.xy);
    
    // 基準テクセルからマイナス方向に8テクセル、重み付きでサンプリング
	Color += weight[1].x * sceneTexture.Sample(Sampler, In.tex0.zw);
	Color += weight[1].y * sceneTexture.Sample(Sampler, In.tex1.zw);
	Color += weight[1].z * sceneTexture.Sample(Sampler, In.tex2.zw);
	Color += weight[1].w * sceneTexture.Sample(Sampler, In.tex3.zw);
	Color += weight[1].x * sceneTexture.Sample(Sampler, In.tex4.zw);
	Color += weight[1].y * sceneTexture.Sample(Sampler, In.tex5.zw);
	Color += weight[1].z * sceneTexture.Sample(Sampler, In.tex6.zw);
	Color += weight[1].w * sceneTexture.Sample(Sampler, In.tex7.zw);

	return float4(Color.rgb, 1.0f);
}
