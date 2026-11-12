#include "pch.h"
#include "CaboRenderingSystem/Material/material.h"

#include "CaboRenderingSystem/Core/log.h"

namespace CRS
{
void Material::SetPass(const PassTag tag, const MaterialPass &pass)
{
    m_passes_[static_cast<size_t>(tag)] = pass;
    ++m_version_;
}

void Material::RemovePass(const PassTag tag)
{
    m_passes_[static_cast<size_t>(tag)].reset();
    ++m_version_;
}

const MaterialPass *Material::GetPass(const PassTag tag) const
{
    const auto &pass = m_passes_[static_cast<size_t>(tag)];
    return pass.has_value() ? &*pass : nullptr;
}

void Material::BuildPropertyTable(const IRenderDevice &device)
{
    const auto forward = GetPass(PassTag::Forward);
    if (forward == nullptr)
    {
        LogWarning("BuildPropertyTable: material has no Forward pass");
        return;
    }

    BuildTable(device, {forward->vertex_shader, forward->pixel_shader}, false);
}

MaterialId MaterialRegistry::Add(Material material)
{
    return m_materials_.Add(std::move(material));
}

void MaterialRegistry::Remove(const MaterialId id)
{
    m_materials_.Remove(id);
}

Material *MaterialRegistry::Get(const MaterialId id)
{
    return m_materials_.Get(id);
}

const Material *MaterialRegistry::Get(const MaterialId id) const
{
    return m_materials_.Get(id);
}

size_t MaterialRegistry::Count() const
{
    return m_materials_.Count();
}
}
