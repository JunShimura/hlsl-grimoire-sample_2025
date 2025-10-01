#include "stdafx.h"
#include "LoadObjPolygon.h"

void LoadObjPolygon::Init(RootSignature& rs)
{
	//シェーダーをロード。
	LoadShaders();
	//パイプラインステートを作成。
	InitPipelineState(rs);
	//頂点バッファを作成。
	InitVertexBuffer();
	//インデックスバッファを作成。
	InitIndexBuffer();
}
void LoadObjPolygon::Draw(RenderContext& rc)
{
	//パイプラインステートを設定。
	rc.SetPipelineState(m_pipelineState);
	//プリミティブのトポロジーを設定。
	rc.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//頂点バッファを設定。
	rc.SetVertexBuffer(m_vertexBuffer);
	//インデックスバッファを設定。
	rc.SetIndexBuffer(m_indexBuffer);
	//ドローコール
	rc.DrawIndexed(36);
}
void LoadObjPolygon::LoadShaders()
{
	m_vertexShader.LoadVS("Assets/shader/sample_objPolygon.fx", "VSMain");
	m_pixelShader.LoadPS("Assets/shader/sample_objPolygon.fx", "PSMain");
}
void LoadObjPolygon::InitPipelineState(RootSignature& rs)
{
	// 頂点レイアウトを定義する。
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	//パイプラインステートを作成。
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = { 0 };
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.pRootSignature = rs.Get();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vertexShader.GetCompiledBlob());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_pixelShader.GetCompiledBlob());
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	m_pipelineState.Init(psoDesc);
}
void LoadObjPolygon::InitVertexBuffer()
{
    // DefaultCube.objの内容に基づく24頂点
    SimpleVertex vertices[24] = {
        // --- 上面 (Y+)
        {{-1,  1, -1}, {1,1,1}, {0.875f, 0.5f}},   // 0: v5/vt1
        {{ 1,  1,  1}, {1,1,1}, {0.625f, 0.75f}},  // 1: v3/vt2
        {{ 1,  1, -1}, {1,1,1}, {0.625f, 0.5f}},   // 2: v1/vt3
        {{-1,  1,  1}, {1,1,1}, {0.375f, 1.0f}},   // 3: v7/vt4

        // --- 底面 (Y-)
        {{ 1, -1,  1}, {1,1,1}, {0.375f, 0.25f}},  // 4: v4/vt7
        {{-1, -1,  1}, {1,1,1}, {0.375f, 0.0f}},   // 5: v8/vt8
        {{ 1, -1, -1}, {1,1,1}, {0.625f, 0.0f}},   // 6: v2/vt6
        {{-1, -1, -1}, {1,1,1}, {0.375f, 0.75f}},  // 7: v6/vt5

        // --- 前面 (Z+)
        {{ 1,  1,  1}, {1,1,1}, {0.625f, 0.75f}},  // 8: v3/vt2
        {{-1, -1,  1}, {1,1,1}, {0.375f, 0.0f}},   // 9: v8/vt8
        {{ 1, -1,  1}, {1,1,1}, {0.375f, 0.25f}},  //10: v4/vt7
        {{-1,  1,  1}, {1,1,1}, {0.375f, 1.0f}},   //11: v7/vt4

        // --- 背面 (Z-)
        {{ 1,  1, -1}, {1,1,1}, {0.625f, 0.5f}},   //12: v1/vt3
        {{-1, -1, -1}, {1,1,1}, {0.125f, 0.5f}},   //13: v6/vt11
        {{ 1, -1, -1}, {1,1,1}, {0.625f, 0.0f}},   //14: v2/vt6
        {{-1,  1, -1}, {1,1,1}, {0.375f, 0.5f}},   //15: v5/vt9

        // --- 右面 (X+)
        {{ 1,  1, -1}, {1,1,1}, {0.625f, 0.5f}},   //16: v1/vt3
        {{ 1,  1,  1}, {1,1,1}, {0.625f, 0.75f}},  //17: v3/vt2
        {{ 1, -1,  1}, {1,1,1}, {0.625f, 0.25f}},  //18: v4/vt12
        {{ 1, -1, -1}, {1,1,1}, {0.625f, 0.0f}},   //19: v2/vt6

        // --- 左面 (X-)
        {{-1,  1,  1}, {1,1,1}, {0.375f, 1.0f}},   //20: v7/vt4
        {{-1,  1, -1}, {1,1,1}, {0.875f, 0.5f}},   //21: v5/vt1
        {{-1, -1, -1}, {1,1,1}, {0.375f, 0.75f}},  //22: v6/vt5
        {{-1, -1,  1}, {1,1,1}, {0.375f, 0.0f}},   //23: v8/vt8
    };

    // メンバにコピー
    for (int i = 0; i < 24; ++i) {
        m_vertices[i] = vertices[i];
    }
    m_vertexBuffer.Init(sizeof(m_vertices), sizeof(m_vertices[0]));
    m_vertexBuffer.Copy(m_vertices);
}

void LoadObjPolygon::SetUVCoord(int vertNo, float U, float V)
{
	m_vertices[vertNo].uv[0] = U;
	m_vertices[vertNo].uv[1] = V;
	m_vertexBuffer.Copy(m_vertices);
}
void LoadObjPolygon::InitIndexBuffer()
{
    // DefaultCube.objの内容に基づく36インデックス
    unsigned short indices[36] = {
        // 上面
        0, 1, 2, 0, 3, 1,
        // 底面
        4, 5, 6, 5, 7, 6,
        // 前面
        8, 9,10, 8,11, 9,
        // 背面
        12,13,14,12,15,13,
        // 右面
        16,17,18,16,18,19,
        // 左面
        20,21,22,20,22,23
    };

    m_indexBuffer.Init(sizeof(indices), 2);
    m_indexBuffer.Copy(static_cast<uint16_t*>(indices));
}
