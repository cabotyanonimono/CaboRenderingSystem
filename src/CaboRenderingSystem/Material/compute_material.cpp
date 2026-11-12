#include "pch.h"
#include "CaboRenderingSystem/Material/compute_material.h"

#include "CaboRenderingSystem/RHI/i_command_list.h"

namespace CRS
{
bool ComputeMaterial::SetShader(IRenderDevice &device, const ShaderHandle shader)
{
    m_shader_ = shader;
    m_pipeline_state_ = device.CreateComputePipelineState({shader});
    BuildTable(device, {shader}, true);
    return m_pipeline_state_.IsValid();
}

void ComputeMaterial::Apply(ICommandList &command_list, const TextureHandle default_texture) const
{
    Bindings bindings;
    Bind(bindings, default_texture);
    command_list.SetComputePipelineState(m_pipeline_state_);
    command_list.SetComputeBindings(bindings);
}
}
