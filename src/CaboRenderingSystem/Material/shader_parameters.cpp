#include "pch.h"
#include "CaboRenderingSystem/Material/shader_parameters.h"

#include <cstring>

#include "CaboRenderingSystem/Core/log.h"

namespace CRS
{
    void ShaderParameters::BuildTable(const IRenderDevice& device, const std::initializer_list<ShaderHandle> shaders,
                                      const bool allow_unordered_access)
    {
        const auto old_properties = std::move(m_properties_);
        const auto old_constants = std::move(m_constants_);
        m_properties_.clear();
        m_resource_slots_.clear();
        m_resource_kinds_ = {};
        m_sampler_slots_.clear();
        m_unordered_access_slots_.clear();
        m_unordered_access_kinds_ = {};

        for (auto& constants : m_constants_)
            constants.clear();

        std::array<uint32_t, kConstantSlotCount> constant_sizes = {};
        for (const auto shader : shaders)
        {
            if (!shader.IsValid())
                continue;

            const auto& reflection = device.GetReflection(shader);
            for (const auto& cb : reflection.constant_buffers)
            {
                // space1はエンジン用なので、表には入れない
                if (cb.space != 0)
                    continue;
                if (cb.slot >= kConstantSlotCount)
                {
                    LogWarning("cbuffer '{}' uses b{}, but up to b{} is supported", cb.name, cb.slot,
                               kConstantSlotCount - 1);
                    continue;
                }

                constant_sizes[cb.slot] = std::max(constant_sizes[cb.slot], cb.size);
                for (const auto& variable : cb.variables)
                    m_properties_[variable.name] = {cb.slot, variable.offset, variable.size, variable.type};
            }

            for (const auto& resource : reflection.resources)
            {
                // space1はフレーム・オブジェクト用なので、表には入れない
                if (resource.space != 0)
                    continue;

                switch (resource.kind)
                {
                case ShaderResourceKind::Sampler:
                    m_sampler_slots_[resource.name] = resource.slot;
                    break;
                case ShaderResourceKind::Texture:
                case ShaderResourceKind::TextureCube:
                case ShaderResourceKind::StructuredBuffer:
                case ShaderResourceKind::ByteAddressBuffer:
                    if (resource.slot >= kResourceSlotCount)
                    {
                        LogWarning("'{}' uses t{}, but up to t{} is supported", resource.name, resource.slot,
                                   kResourceSlotCount - 1);
                        break;
                    }
                    m_resource_slots_[resource.name] = resource.slot;
                    m_resource_kinds_[resource.slot] = resource.kind;
                    break;
                case ShaderResourceKind::ReadWriteTexture:
                case ShaderResourceKind::ReadWriteBuffer:
                    if (!allow_unordered_access)
                    {
                        LogWarning("'{}' is a read-write resource, which materials do not support", resource.name);
                        break;
                    }
                    if (resource.slot >= kUnorderedAccessSlotCount)
                    {
                        LogWarning("'{}' uses u{}, but up to u{} is supported", resource.name, resource.slot,
                                   kUnorderedAccessSlotCount - 1);
                        break;
                    }
                    m_unordered_access_slots_[resource.name] = resource.slot;
                    m_unordered_access_kinds_[resource.slot] = resource.kind;
                    break;
                }
            }
        }

        for (uint32_t slot = 0; slot < kConstantSlotCount; ++slot)
            m_constants_[slot].resize(constant_sizes[slot]);

        // 名前と型が同じ値は引き継ぐ
        for (const auto& [name, property] : m_properties_)
        {
            const auto old = old_properties.find(name);
            if (old == old_properties.end() || old->second.type != property.type || old->second.size != property.size)
                continue;

            const auto& old_data = old_constants[old->second.slot];
            if (old->second.offset + old->second.size <= old_data.size())
                std::memcpy(m_constants_[property.slot].data() + property.offset, old_data.data() + old->second.offset,
                            property.size);
        }
    }

    bool ShaderParameters::Write(const std::string& name, const ShaderVariableType type, const void* data,
                                 const uint32_t size)
    {
        const auto itr = m_properties_.find(name);
        if (itr == m_properties_.end())
            return false;

        const auto& property = itr->second;
        auto& constants = m_constants_[property.slot];
        if (property.type != type || property.size < size || property.offset + size > constants.size())
        {
            LogWarning("Shader property '{}' type mismatch", name);
            return false;
        }

        std::memcpy(constants.data() + property.offset, data, size);
        return true;
    }

    bool ShaderParameters::SetInt(const std::string& name, const int32_t value)
    {
        return Write(name, ShaderVariableType::Int, &value, sizeof(value));
    }

    bool ShaderParameters::SetUInt(const std::string& name, const uint32_t value)
    {
        return Write(name, ShaderVariableType::UInt, &value, sizeof(value));
    }

    bool ShaderParameters::SetFloat(const std::string& name, const float value)
    {
        return Write(name, ShaderVariableType::Float, &value, sizeof(value));
    }

    bool ShaderParameters::SetVector(const std::string& name, const Vector4& value)
    {
        return Write(name, ShaderVariableType::Float4, &value, sizeof(value));
    }

    bool ShaderParameters::SetColor(const std::string& name, const Color& value)
    {
        return Write(name, ShaderVariableType::Float4, &value, sizeof(value));
    }

    bool ShaderParameters::SetMatrix(const std::string& name, const Matrix& value)
    {
        return Write(name, ShaderVariableType::Float4x4, &value, sizeof(value));
    }

    bool ShaderParameters::SetData(const std::string& name, const void* data, const uint32_t size)
    {
        const auto itr = m_properties_.find(name);
        if (itr == m_properties_.end())
            return false;

        const auto& property = itr->second;
        auto& constants = m_constants_[property.slot];
        const auto copy_size = std::min(size, property.size);
        if (property.offset + copy_size > constants.size())
            return false;

        std::memcpy(constants.data() + property.offset, data, copy_size);
        return true;
    }

    bool ShaderParameters::SetTexture(const std::string& name, const TextureHandle texture)
    {
        if (const auto itr = m_resource_slots_.find(name); itr != m_resource_slots_.end())
        {
            const auto kind = m_resource_kinds_[itr->second];
            if (kind != ShaderResourceKind::Texture && kind != ShaderResourceKind::TextureCube)
                return false;

            m_resources_[itr->second] = {texture, {}};
            return true;
        }

        if (const auto itr = m_unordered_access_slots_.find(name); itr != m_unordered_access_slots_.end())
        {
            if (m_unordered_access_kinds_[itr->second] != ShaderResourceKind::ReadWriteTexture)
                return false;

            m_unordered_access_[itr->second] = {texture, {}};
            return true;
        }

        return false;
    }

    bool ShaderParameters::SetBuffer(const std::string& name, const BufferHandle buffer)
    {
        if (const auto itr = m_resource_slots_.find(name); itr != m_resource_slots_.end())
        {
            const auto kind = m_resource_kinds_[itr->second];
            if (kind != ShaderResourceKind::StructuredBuffer && kind != ShaderResourceKind::ByteAddressBuffer)
                return false;

            m_resources_[itr->second] = {{}, buffer};
            return true;
        }

        if (const auto itr = m_unordered_access_slots_.find(name); itr != m_unordered_access_slots_.end())
        {
            if (m_unordered_access_kinds_[itr->second] != ShaderResourceKind::ReadWriteBuffer)
                return false;

            m_unordered_access_[itr->second] = {{}, buffer};
            return true;
        }

        return false;
    }

    bool ShaderParameters::SetSampler(const std::string& name, const SamplerHandle sampler)
    {
        const auto itr = m_sampler_slots_.find(name);
        if (itr == m_sampler_slots_.end() || itr->second >= kSamplerSlotCount)
            return false;

        m_samplers_[itr->second] = sampler;
        return true;
    }

    bool ShaderParameters::HasProperty(const std::string& name) const
    {
        return m_properties_.contains(name);
    }

    bool ShaderParameters::HasResource(const std::string& name) const
    {
        return m_resource_slots_.contains(name) || m_unordered_access_slots_.contains(name);
    }

    bool ShaderParameters::IsResourceSet(const uint32_t slot) const
    {
        return slot < kResourceSlotCount && (m_resources_[slot].texture.IsValid() || m_resources_[slot].buffer.
            IsValid());
    }

    void ShaderParameters::BindConstants(Bindings& bindings) const
    {
        for (uint32_t slot = 0; slot < kConstantSlotCount; ++slot)
        {
            const auto& constants = m_constants_[slot];
            if (!constants.empty())
                bindings.constants[slot] = ConstantBinding::FromData(constants.data(),
                                                                     static_cast<uint32_t>(constants.size()));
        }
    }

    void ShaderParameters::BindResources(Bindings& bindings, const TextureHandle default_texture) const
    {
        for (uint32_t slot = 0; slot < kResourceSlotCount; ++slot)
        {
            if (IsResourceSet(slot))
            {
                bindings.resources[slot] = m_resources_[slot];
                continue;
            }

            // Texture2Dは白にしておく（0だと真っ黒になるため）。それ以外の種類は空のSRVにする
            const auto kind = m_resource_kinds_[slot];
            if (!kind.has_value() || *kind == ShaderResourceKind::Texture)
                bindings.resources[slot] = {default_texture, {}};
            else
                bindings.resources[slot] = {};
        }
    }

    void ShaderParameters::BindUnorderedAccess(Bindings& bindings) const
    {
        bindings.unordered_access = m_unordered_access_;
    }

    void ShaderParameters::Bind(Bindings& bindings, const TextureHandle default_texture) const
    {
        BindConstants(bindings);
        BindResources(bindings, default_texture);
        bindings.samplers = m_samplers_;
        BindUnorderedAccess(bindings);
    }
}
