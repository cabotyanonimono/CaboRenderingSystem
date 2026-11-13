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
    m_thread_group_size_ = shader.IsValid() ? device.GetReflection(shader).thread_group_size
                                            : std::array<uint32_t, 3>{};
    return m_pipeline_state_.IsValid();
}

void ComputeMaterial::Apply(ICommandList &command_list, const TextureHandle default_texture) const
{
    Bindings bindings;
    Bind(bindings, default_texture);
    command_list.SetComputePipelineState(m_pipeline_state_);
    command_list.SetComputeBindings(bindings);
}

void ComputeMaterial::Dispatch(ICommandList &command_list, const uint32_t group_count_x,
                               const uint32_t group_count_y, const uint32_t group_count_z,
                               const TextureHandle default_texture) const
{
    Apply(command_list, default_texture);
    command_list.Dispatch(group_count_x, group_count_y, group_count_z);
}

void ComputeMaterial::DispatchThreads(ICommandList &command_list, const uint32_t thread_count_x,
                                      const uint32_t thread_count_y, const uint32_t thread_count_z,
                                      const TextureHandle default_texture) const
{
    const auto groups = [](const uint32_t count, const uint32_t size) { return size == 0 ? 0 : (count + size - 1) / size; };
    Dispatch(command_list, groups(thread_count_x, m_thread_group_size_[0]),
             groups(thread_count_y, m_thread_group_size_[1]), groups(thread_count_z, m_thread_group_size_[2]),
             default_texture);
}
}
