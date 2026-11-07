Texture2D scene_color : register(t0);

cbuffer Tonemap : register(b0)
{
    float exposure;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

/// <summary>
/// 頂点バッファを使わずに、画面全体を覆う三角形を作ります。
/// </summary>
VSOutput VSMain(uint vertex_id : SV_VertexID)
{
    const float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    VSOutput output;
    output.position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

/// <summary>
/// ACESのトーンカーブの近似（Narkowicz）です。
/// </summary>
float3 ACESFilm(float3 x)
{
    return saturate((x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f));
}

/// <summary>
/// 同じ位置のHDRのシーンカラーをトーンマップします。描画先がsRGBのため、線形のまま出力します。
/// </summary>
float4 PSMain(VSOutput input) : SV_TARGET
{
    const float4 hdr = scene_color.Load(int3(input.position.xy, 0));
    return float4(ACESFilm(hdr.rgb * exposure), hdr.a);
}
