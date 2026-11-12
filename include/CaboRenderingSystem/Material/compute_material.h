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

    ShaderHandle Shader() const
    {
        return m_shader_;
    }

    PipelineStateHandle PipelineState() const
    {
        return m_pipeline_state_;
    }
};
}
