# ポリゴン境界で miss（赤）が出る原因と対策

## 1. いま起きていること（初心者向け）
- レイトレーシングは「レイ（光線）」を飛ばして、三角形に当たれば `closesthit`、当たらなければ `miss` が呼ばれます。
- このプロジェクトでは `miss` が赤を返すため、**レイが当たらなかった場所が赤で見える**状態です。
- 問題の見え方が「ポリゴン境界に沿って赤い筋」なので、境界付近でヒット判定が不安定になっている可能性が高いです。

---

## 2. 主な原因

### 原因A: 衝突点から次のレイを“ちょうど同じ位置”で飛ばしている
対象コード（`Assets/shader/sample.fx`）:

`chs()` 内:
```hlsl
ray.Origin = hitPos;
ray.TMin = 0.01f;
```

`TraceShadowRay()` 内:
```hlsl
ray.Origin = hitPos;
ray.TMin = 0.01f;
```

衝突点は浮動小数点誤差を含むため、**ぴったり表面上から再発射**すると境界で判定が揺れやすくなります。  
結果として、本来は当たるはずの近いポリゴンを取りこぼして `miss`（赤）になることがあります。

### 原因B: `TMin` が固定値で、シーンスケールに対して強すぎる/弱すぎる
- `0.01f` はモデルサイズによっては大きすぎて、境界近くの有効なヒットを飛ばすことがあります。
- 逆に小さすぎると自己ヒット（アクネ）が増えます。

### 原因C: レイ方向の正規化不足や発射方向と法線の関係
- 反射ベクトルは計算上ほぼ単位長ですが、明示正規化しておく方が安全です。
- バイアス方向（法線の+/-）を誤ると、面の内側へレイを押し込んでしまい miss の増加要因になります。

---

## 3. 対策（優先順）

## 対策1: レイ原点にバイアス（Ray Bias）を入れる（最重要）
衝突点 `hitPos` からそのまま打たず、法線方向へ少しずらしてから発射します。

- 例: `kRayBias = 1e-3`（シーンスケールに応じて調整）
- `origin = hitPos + normal * biasSign * kRayBias`
- `biasSign` は `dot(ray.Direction, normal)` の符号で決定

これで境界の数値誤差による取りこぼしを減らせます。

## 対策2: `TMin` を固定値1本にしない
- 初期値は `1e-4` ～ `1e-3` から開始。
- モデルサイズに対して不安定なら、距離やスケールに応じた可変値にします。

## 対策3: 反射/影レイ方向の正規化を徹底
- `ray.Direction = normalize(ray.Direction);`
- 小さいことですが、誤差蓄積を抑えられます。

## 対策4: デバッグ色を段階化して切り分け
- miss を一色赤だけでなく、主レイmissと反射レイmissで色を分けると原因特定が速くなります。

---

## 4. 改変案（`sample.fx`）

## 4.1 `chs()`（反射レイ）
1. 法線を取得後に正規化
2. 反射方向を正規化
3. 原点バイアス付きで `ray.Origin` を設定
4. `TMin` を小さめに再調整

`sample.fx` での具体的な置換:

置換前:
```hlsl
float3 normal = GetNormal(attribs);
float3 refDir = reflect(rayDirW, normal);

RayDesc ray;
ray.Origin = hitPos;
ray.Direction = refDir;
ray.TMin = 0.01f;
ray.TMax = 10000;
```

置換後:
```hlsl
float3 normal = normalize(GetNormal(attribs));
float3 refDir = normalize(reflect(rayDirW, normal));

RayDesc ray;
float biasSign = (dot(refDir, normal) >= 0.0f) ? 1.0f : -1.0f;
ray.Origin = hitPos + normal * biasSign * kRayBias;
ray.Direction = refDir;
ray.TMin = kRayTMin;
ray.TMax = 10000;
```

## 4.2 `TraceShadowRay()`（影レイ）
1. 衝突点算出後、法線取得
2. ライト方向へ飛ばす向きに合わせてバイアス方向を決定
3. `ray.Origin` と `TMin` を再設定

`sample.fx` での具体的な置換:

1) 関数宣言・定義を変更（法線計算に `attribs` を渡すため）

置換前:
```hlsl
void TraceShadowRay(inout RayPayload payload);
...
void TraceShadowRay(inout RayPayload payload)
```

置換後:
```hlsl
void TraceShadowRay(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs);
...
void TraceShadowRay(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
```

2) 関数内部のレイ設定を変更

置換前:
```hlsl
ray.Origin = hitPos;
ray.Direction = float3(0.5f, 0.5f, 0.2f);
ray.Direction = normalize(ray.Direction);
ray.TMin = 0.01f;
ray.TMax = 100;
```

置換後:
```hlsl
float3 normal = normalize(GetNormal(attribs));
float3 lightDir = normalize(float3(0.5f, 0.5f, 0.2f));
float biasSign = (dot(lightDir, normal) >= 0.0f) ? 1.0f : -1.0f;

ray.Origin = hitPos + normal * biasSign * kRayBias;
ray.Direction = lightDir;
ray.TMin = kRayTMin;
ray.TMax = 100;
```

## 4.3 共通定数
ファイル先頭付近に以下のような定数を追加して管理:
```hlsl
static const float kRayBias = 1e-3f;
static const float kRayTMin = 1e-4f;
```

追加位置（例）:
```hlsl
SamplerState  g_samplerState : register(s0);                        // サンプラーステート

static const float kRayBias = 1e-3f;
static const float kRayTMin = 1e-4f;
```

---

## 5. 導入後の確認ポイント
1. ポリゴン境界の赤筋が減る/消えるか
2. 自己ヒット由来のノイズ（黒点・ちらつき）が増えていないか
3. シーンを拡大/縮小しても同じ設定で破綻しないか

---

## 6. まとめ
- 今回の赤表示は、境界での浮動小数点誤差とレイ再発射条件（原点・`TMin`）が主因の可能性が高いです。
- 最も効果があるのは **「法線方向バイアス + `TMin` 再調整」** です。
- まずは小さな値で導入し、シーンスケールに合わせて段階調整するのが安全です。

---

## 7. `TMin` の意味と特性（詳説）

### 7.1 `TMin` の意味
- `TMin` は `RayDesc` の「交差判定を開始する最小距離」です。
- レイは次で表されます。

```hlsl
P(t) = Origin + t * Direction
```

- このとき有効範囲は `t ∈ [TMin, TMax]` です。
- つまり **`TMin` 未満の範囲はヒット判定しない** という意味になります。

### 7.2 `TMin` を使う主目的
- 主目的は **自己ヒット回避** です。
- 反射レイやシャドウレイをヒット点から再発射すると、浮動小数点誤差で同じ面に再ヒットしやすくなります。
- `TMin > 0` にすると、原点直近の再ヒットを除外できます。

### 7.3 値を変えたときの振る舞い

小さすぎる場合（例: `0`、`1e-8`）
- 自己ヒットが増えやすい
- シャドウアクネやちらつきが出やすい

大きすぎる場合（シーンスケールに対して過大）
- 近距離の本来ヒットすべき面を飛ばす
- 境界の抜け、隙間、`miss`（赤）が増える

`TMin` は「自己ヒット抑制」と「ヒット取りこぼし抑制」のトレードオフです。

### 7.4 実運用での重要点
- `TMin` 単独では不十分なことが多く、次の併用が重要です。
  1. 原点バイアス（`Origin += normal * bias`）
  2. 適切な `TMin`（小さめ）

- 役割の違い:
  - 原点バイアス: 発射位置の安定化
  - `TMin`: 判定開始距離の下限設定

### 7.5 調整の目安
- 初期値目安: `1e-4` ～ `1e-3`
- シーンスケールが大きいほど相対調整が必要
- 反射レイとシャドウレイで最適値が異なる場合がある

### 7.6 よくある誤解
- 誤解: `TMin` は物理的な最小到達距離
  - 実際: 交差判定の計算上の下限
- 誤解: 大きくすれば安定する
  - 実際: 大きすぎるとヒット欠落が増える
