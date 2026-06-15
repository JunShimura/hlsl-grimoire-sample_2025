# HLSL 行列格納順序のエビデンス

## 公式ドキュメントによるエビデンス

### 1. Microsoft Learn - Variable Syntax (HLSL)

**URL:** https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-variable-syntax

**該当箇所:**

> **column_major** or **row_major**
> 
> Optional variable storage class modifier. Using column_major or row_major is only valid on declarations of matrices or arrays of matrices.
>
> By default, **matrices are stored in column-major order**. Applying column_major or row_major to a variable overrides the storage class set by the /Zpr (default) and /Zpc switches.

### 2. Microsoft Learn - Storage Class (HLSL)

**URL:** https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-storage-class

**該当箇所:**

> **column_major**
> 
> Store matrix values in column-major order.
>
> **row_major**
>
> Store matrix values in row-major order.
>
> By default, **the shader compiler packs matrix values in column-major order**.

### 3. コンパイラオプション

**URL:** https://learn.microsoft.com/en-us/windows/win32/direct3dtools/dx-graphics-tools-fxc-syntax

fxc コンパイラオプション:
- `/Zpc` - Pack matrices in **column-major order** (default)
- `/Zpr` - Pack matrices in row-major order

## 重要なポイント

### デフォルトは列優先（column-major）

HLSLのデフォルトは **列優先（column-major）** です。これは：

1. **Microsoft公式ドキュメントで明記**されている
2. **コンパイラのデフォルトオプション** (`/Zpc`) が列優先
3. 明示的に `row_major` を指定しない限り、列優先で格納される

## 列優先と行優先の違い

### 列優先（column-major）- デフォルト

```
メモリ上の配置：
[m00, m10, m20, m30, m01, m11, m21, m31, m02, m12, m22, m32, m03, m13, m23, m33]
 ↑第1列       ↑第2列       ↑第3列       ↑第4列

論理的な行列表現：
[m00  m01  m02  m03]
[m10  m11  m12  m13]
[m20  m21  m22  m23]
[m30  m31  m32  m33]
```

### 行優先（row-major）

```
メモリ上の配置：
[m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33]
 ↑第1行       ↑第2行       ↑第3行       ↑第4行

論理的な行列表現：
[m00  m01  m02  m03]
[m10  m11  m12  m13]
[m20  m21  m22  m23]
[m30  m31  m32  m33]
```

## sample.fx における影響

### コード例

```hlsl
float4 objectPos = mWorld[3];
```

このコードは、**列優先（デフォルト）** の場合：

- `mWorld[3]` は論理的に「4行目」を取得
- 列優先では、この4行目が平行移動成分（Translation）を含む
- つまり `objectPos = (m30, m31, m32, m33)` が取得される

### mul() 関数の動作

```hlsl
psIn.pos = mul(mWorld, vsIn.pos);
```

列優先の場合の計算：
```
x' = m00*x + m01*y + m02*z + m03*w
y' = m10*x + m11*y + m12*z + m13*w
z' = m20*x + m21*y + m22*z + m23*w
w' = m30*x + m31*y + m32*z + m33*w
```

## 明示的な指定方法

### 変数ごとに指定

```hlsl
column_major float4x4 mWorld;  // 列優先（デフォルト）
row_major float4x4 mView;      // 行優先
```

### コンパイラオプションで指定

```
fxc /Zpc shader.fx  # 列優先（デフォルト）
fxc /Zpr shader.fx  # 行優先
```

## まとめ

- **HLSLのデフォルトは列優先（column-major）**
- Microsoft公式ドキュメントで明記されている
- sample.fxでは明示的な指定がないため、列優先がデフォルトで適用される
- `mWorld[3]` で平行移動成分（4行目）が取得できるのは、列優先のおかげ

## 参考資料

1. [Variable Syntax (HLSL) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-variable-syntax)
2. [Storage Class (HLSL) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-storage-class)
3. [Effect-Compiler Tool (fxc) - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3dtools/dx-graphics-tools-fxc-syntax)
