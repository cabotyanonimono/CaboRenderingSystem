cbuffer ComputeTestParams : register(b0)
{
    uint g_count;
    uint g_multiplier;
};

RWStructuredBuffer<uint> g_output : register(u0);

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= g_count)
        return;

    g_output[id.x] = id.x * g_multiplier;
}
