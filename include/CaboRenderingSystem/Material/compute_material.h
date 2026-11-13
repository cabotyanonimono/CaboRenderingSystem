#pragma once
#include "CaboRenderingSystem/export.h"

#include "CaboRenderingSystem/Material/shader_parameters.h"

namespace CRS
{
/// <summary>
/// コンピュートシェーダーとその値を持ちます。マテリアルと同じく、定数・リソース・サンプラー・UAVを名前で設定します。
/// </summary>
class CRS_API ComputeMaterial : public ShaderParameters
{
    ShaderHandle m_shader_;
    PipelineStateHandle m_pipeline_state_;
    // シェーダーの[numthreads]
    std::array<uint32_t, 3> m_thread_group_size_ = {};

public:
    /// <summary>
    /// コンピュートシェーダーを設定し、PSOとプロパティ表を作り直します。既に書き込まれている値は、名前と型が同じであれば引き継ぎます。
    /// </summary>
    /// <returns>PSOが作れなかった場合はfalse</returns>
    bool SetShader(IRenderDevice &device, ShaderHandle shader);

    /// <summary>
    /// PSOと値をコマンドリストに設定します。この後にDispatchを呼びます。
    /// </summary>
    void Apply(ICommandList &command_list, TextureHandle default_texture = {}) const;
    /// <summary>
    /// Applyしてから実行します。引数はスレッドグループの数です。
    /// </summary>
    void Dispatch(ICommandList &command_list, uint32_t group_count_x, uint32_t group_count_y = 1,
                  uint32_t group_count_z = 1, TextureHandle default_texture = {}) const;
    /// <summary>
    /// Applyしてから実行します。引数はスレッドの総数で、[numthreads]で割って切り上げたグループ数で実行します。
    /// </summary>
    void DispatchThreads(ICommandList &command_list, uint32_t thread_count_x, uint32_t thread_count_y = 1,
                         uint32_t thread_count_z = 1, TextureHandle default_texture = {}) const;

    ShaderHandle Shader() const
    {
        return m_shader_;
    }

    PipelineStateHandle PipelineState() const
    {
        return m_pipeline_state_;
    }

    /// <summary>
    /// シェーダーの[numthreads]です。
    /// </summary>
    const std::array<uint32_t, 3> &ThreadGroupSize() const
    {
        return m_thread_group_size_;
    }
};
}
