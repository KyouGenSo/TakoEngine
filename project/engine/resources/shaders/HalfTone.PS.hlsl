// HalfTone: 陰をドットで描く。届く光の割合が小さい所ほどドットが大きくなり、陰の色で埋まる
#include "Object3d.hlsli"
#include "View.hlsli"
#include "Common.hlsli"
#include "Light.hlsli"
#include "Shadow.hlsli"

struct Material
{
    float4 color;      // テクスチャに掛ける色
    float3 shadeColor; // 陰（ドット）の色。面の色に掛ける
    float dotSize;     // ドットの間隔（高さ 1080 の画面でのピクセル数）
    float angle;       // 格子の回転（度）
    float toneMin;     // 届く光の割合がこれ以下なら、陰の色で埋まる
    float toneMax;     // 届く光の割合がこれ以上なら、ドットが出ない
    int dotShape;      // 0: 円、1: 四角、2: ひし形
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<View> gView : register(b2);

Texture2D<float4> gTexture : register(t0);

static const float kReferenceHeight = 1080.0;
static const float kTwoPi = 6.28318530718;
static const float3 kLuminance = float3(0.2126, 0.7152, 0.0722);

// マスの中で塗る順番（0: 最初に塗る、1: 最後に塗る）。cell は 1 がドットの間隔
float Spot(float2 cell, int shape)
{
    float2 fromCenter = abs(frac(cell) - 0.5) * 2.0;
    if (shape == 1)
    {
        return max(fromCenter.x, fromCenter.y);
    }
    if (shape == 2)
    {
        return (fromCenter.x + fromCenter.y) * 0.5;
    }
    // 印刷の網点の形。明るい側は丸い点、半分で市松、暗い側は丸い抜きになる
    return 0.5 - 0.25 * (cos(cell.x * kTwoPi) + cos(cell.y * kTwoPi));
}

// 塗る面積の割合を、Spot と比べるしきい値に直す
float SpotThreshold(float amount, int shape)
{
    if (shape == 1)
    {
        return sqrt(amount);
    }
    if (shape == 2)
    {
        return amount < 0.5 ? sqrt(amount * 0.5) : 1.0 - sqrt((1.0 - amount) * 0.5);
    }
    // ponytail: 円は直さない（塗る面積が両端で約 0.64 倍になる）。合わせるなら逆関数の近似を入れる
    return amount;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float3 N = normalize(input.normal);

    // full: 向きと影を無視して受ける光、lit: 実際に届く光
    float3 full = gDirectionalLight.color.rgb * gDirectionalLight.intensity;
    float NdotL = saturate(dot(N, -gDirectionalLight.direction));
    float diffuse = gDirectionalLight.lightType == 1 ? pow(NdotL * 0.5 + 0.5, 2.0) : NdotL;
    float3 lit = full * diffuse * DirectionalShadow(input.worldPos, N, gDirectionalLight.direction);

    for (int i = 0; i < gNumPointLights; i++)
    {
        PointLight light = gPointLights[i];
        float attenuation = light.enable != 0 ? PointLightAttenuation(light, input.worldPos) : 0.0;
        if (attenuation > 0.0)
        {
            float3 lightColor = light.color.rgb * light.intensity * attenuation;
            float3 L = normalize(light.position - input.worldPos);
            full += lightColor;
            lit += lightColor * saturate(dot(N, L)) * PointShadow(light.shadowSlice, light.position, input.worldPos, N);
        }
    }

    for (int j = 0; j < gNumSpotLights; j++)
    {
        SpotLight light = gSpotLight[j];
        float attenuation = light.enable != 0 ? SpotLightAttenuation(light, input.worldPos) : 0.0;
        if (attenuation > 0.0)
        {
            float3 lightColor = light.color.rgb * light.intensity * attenuation;
            float3 L = normalize(light.position - input.worldPos);
            full += lightColor;
            lit += lightColor * saturate(dot(N, L)) * SpotShadow(light.shadowSlice, light.position, input.worldPos, N);
        }
    }

    // 届く光の割合（ライトの強さに依らない）を、塗る面積の割合にする
    float tone = dot(lit, kLuminance) / max(dot(full, kLuminance), 0.0001);
    tone = saturate((tone - gMaterial.toneMin) / max(gMaterial.toneMax - gMaterial.toneMin, 0.0001));
    float amount = 1.0 - tone;

    // 画面に固定した格子。解像度が変わっても、画面に対するドットの大きさは同じ
    float cellPixels = max(gMaterial.dotSize, 1.0) * gView.screenSize.y / kReferenceHeight;
    float sinAngle, cosAngle;
    sincos(radians(gMaterial.angle), sinAngle, cosAngle);
    float2 cell = float2(input.pos.x * cosAngle - input.pos.y * sinAngle, input.pos.x * sinAngle + input.pos.y * cosAngle) / cellPixels;

    // 縁を 1 ピクセルの幅でぼかす。円は中心で傾きが 0 になるので、半ピクセル離れた所の値を下限にする
    float spot = Spot(cell, gMaterial.dotShape);
    float halfPixel = 0.5 * kTwoPi / cellPixels;
    float width = max(fwidth(spot) * 0.5, halfPixel * halfPixel / 8.0);
    // 幅の分だけ広げて比べ、amount が 0 と 1 のときにちょうど無地にする
    float ink = smoothstep(spot - width, spot + width, SpotThreshold(amount, gMaterial.dotShape) * (1.0 + 2.0 * width) - width);
    // 間隔が 2 ピクセル以下ではモアレになるので、平均の濃さで塗る
    ink = lerp(amount, ink, saturate(cellPixels - 2.0));

    float4 texColor = gTexture.Sample(gLinearWrap, input.texcoord);
    float3 surfaceColor = texColor.rgb * gMaterial.color.rgb * full;
    return float4(surfaceColor * lerp(float3(1.0, 1.0, 1.0), gMaterial.shadeColor, ink), texColor.a * gMaterial.color.a);
}
