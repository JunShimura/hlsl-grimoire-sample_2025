#include <DirectXMath.h>
#include <iostream>

int main()
{
	using namespace DirectX;

	// 平行移動行列を作成 (10, 20, 30)
	XMMATRIX M = XMMatrixTranslation(10.0f, 20.0f, 30.0f);

	// XMFLOAT4X4に格納
	XMFLOAT4X4 mat;
	XMStoreFloat4x4(&mat, M);

	std::cout << "XMMatrixTranslation(10, 20, 30) の結果:\n\n";

	std::cout << "構造体メンバー表記:\n";
	std::cout << "_11=" << mat._11 << " _12=" << mat._12 << " _13=" << mat._13 << " _14=" << mat._14 << "\n";
	std::cout << "_21=" << mat._21 << " _22=" << mat._22 << " _23=" << mat._23 << " _24=" << mat._24 << "\n";
	std::cout << "_31=" << mat._31 << " _32=" << mat._32 << " _33=" << mat._33 << " _34=" << mat._34 << "\n";
	std::cout << "_41=" << mat._41 << " _42=" << mat._42 << " _43=" << mat._43 << " _44=" << mat._44 << "\n\n";

	std::cout << "配列表記 m[行][列]:\n";
	std::cout << "m[0][0]=" << mat.m[0][0] << " m[0][1]=" << mat.m[0][1] << " m[0][2]=" << mat.m[0][2] << " m[0][3]=" << mat.m[0][3] << "\n";
	std::cout << "m[1][0]=" << mat.m[1][0] << " m[1][1]=" << mat.m[1][1] << " m[1][2]=" << mat.m[1][2] << " m[1][3]=" << mat.m[1][3] << "\n";
	std::cout << "m[2][0]=" << mat.m[2][0] << " m[2][1]=" << mat.m[2][1] << " m[2][2]=" << mat.m[2][2] << " m[2][3]=" << mat.m[2][3] << "\n";
	std::cout << "m[3][0]=" << mat.m[3][0] << " m[3][1]=" << mat.m[3][1] << " m[3][2]=" << mat.m[3][2] << " m[3][3]=" << mat.m[3][3] << "\n\n";

	std::cout << "平行移動成分の位置:\n";
	std::cout << "X=10は: _14=" << mat._14 << " または m[0][3]=" << mat.m[0][3] << "\n";
	std::cout << "Y=20は: _24=" << mat._24 << " または m[1][3]=" << mat.m[1][3] << "\n";
	std::cout << "Z=30は: _34=" << mat._34 << " または m[2][3]=" << mat.m[2][3] << "\n";

	return 0;
}
