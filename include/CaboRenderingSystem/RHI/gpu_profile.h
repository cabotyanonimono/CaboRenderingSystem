#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace CRS
{
/// <summary>
/// GPUプロファイラで測った区間1つ分の結果です。
/// draw_calls / vertices / indices は、この区間の中（子の区間を含む）で記録した描画の合計です。
/// depthは0がフレームの直下で、前にある区間のうちdepthが1つ小さい直近のものが親です。
/// </summary>
struct GpuProfileScope
{
    std::string name;
    uint32_t depth = 0;
    double gpu_ms = 0.0;
    uint32_t draw_calls = 0;
    uint64_t vertices = 0;
    uint64_t indices = 0;
};

/// <summary>
/// 1フレーム分のGPUプロファイルです。GPUの処理が終わってから読み戻すので、数フレーム前の結果になります。
/// scopesは区間を始めた順に並びます。
/// </summary>
struct GpuProfileResult
{
    uint64_t frame_number = 0;
    double frame_gpu_ms = 0.0;
    uint32_t draw_calls = 0;
    std::vector<GpuProfileScope> scopes;
};
}
