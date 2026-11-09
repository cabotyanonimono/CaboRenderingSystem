struct Particle
{
    float3 position;
    float life;
    float3 velocity;
    float seed;
};

cbuffer ParticleParams : register(b0)
{
    row_major float4x4 g_view_projection;
    float3 g_camera_right;
    float g_time;
    float3 g_camera_up;
    float g_delta_time;
    uint g_count;
    float3 g_padding;
};

RWStructuredBuffer<Particle> g_particles_rw : register(u0);
StructuredBuffer<Particle> g_particles : register(t0);

float Hash(float n)
{
    return frac(sin(n) * 43758.5453);
}

float3 HueToRgb(float h)
{
    float3 rgb = saturate(abs(frac(h + float3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0);
    return rgb;
}

[numthreads(256, 1, 1)]
void CSUpdate(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= g_count)
        return;

    Particle p = g_particles_rw[id.x];
    float dt = min(g_delta_time, 0.05);

    if (p.life <= 0.0)
    {
        float s = id.x * 1.618 + g_time * 13.37;
        float angle = Hash(s) * 6.2831853;
        float spread = 0.6 + Hash(s + 1.0) * 1.4;
        float swirl = g_time * 1.5;
        p.position = float3(cos(swirl) * 0.3, 1.5, sin(swirl) * 0.3);
        p.velocity = float3(cos(angle) * spread, 3.0 + Hash(s + 2.0) * 2.0, sin(angle) * spread);
        p.life = 1.5 + Hash(s + 3.0) * 1.5;
        p.seed = Hash(s + 4.0);
    }

    float3 to_axis = float3(-p.position.x, 0.0, -p.position.z);
    float3 tangent = float3(-p.position.z, 0.0, p.position.x);
    p.velocity += (float3(0.0, -6.0, 0.0) + tangent * 2.0 + to_axis * 0.5) * dt;
    p.position += p.velocity * dt;

    if (p.position.y < 0.0)
    {
        p.position.y = 0.0;
        p.velocity.y = -p.velocity.y * 0.5;
        p.velocity.xz *= 0.8;
    }

    p.life -= dt;
    g_particles_rw[id.x] = p;
}

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VSOutput VSMain(uint vertex_id : SV_VertexID)
{
    static const float2 corners[6] = {
        float2(-1, -1), float2(1, -1), float2(1, 1),
        float2(-1, -1), float2(1, 1), float2(-1, 1)
    };

    Particle p = g_particles[vertex_id / 6];
    float2 corner = corners[vertex_id % 6];
    float alive = p.life > 0.0 ? 1.0 : 0.0;
    float size = 0.06 * saturate(p.life) * alive;
    float3 world = p.position + (g_camera_right * corner.x + g_camera_up * corner.y) * size;

    VSOutput output;
    output.position = mul(float4(world, 1.0), g_view_projection);
    output.uv = corner;
    float3 rgb = HueToRgb(frac(p.seed * 0.3 + g_time * 0.1));
    output.color = float4(rgb * 1.2, saturate(p.life) * 0.6);
    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float falloff = saturate(1.0 - dot(input.uv, input.uv));
    falloff *= falloff;
    return float4(input.color.rgb, input.color.a * falloff);
}
