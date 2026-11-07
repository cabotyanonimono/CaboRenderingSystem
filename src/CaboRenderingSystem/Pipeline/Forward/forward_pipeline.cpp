#include "pch.h"
#include "CaboRenderingSystem/Pipeline/Forward/forward_pipeline.h"

#include <algorithm>

#include "CaboRenderingSystem/Core/log.h"
#include "CaboRenderingSystem/Pipeline/Forward/forward_passes.h"

namespace CRS
{
namespace
{
// シェーダーを読むフォルダを決める。指定があればそれを、なければこのDLLと同じフォルダの Shaders/ を使う
std::wstring ResolveShaderDirectory(const std::wstring &configured)
{
    if (!configured.empty())
    {
        std::wstring directory = configured;
        if (directory.back() != L'/' && directory.back() != L'\\')
            directory += L'/';
        return directory;
    }

    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ResolveShaderDirectory), &module);
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(module, path, MAX_PATH);
    std::wstring directory(path);
    directory.resize(directory.find_last_of(L"\\/") + 1);
    return directory + L"Shaders/";
}

Shader LoadShader(IRenderDevice &device, const std::wstring &directory, const wchar_t *file_name, const wchar_t *entry_point,
                  const ShaderStage stage)
{
    Shader shader(device, {directory + file_name, entry_point, stage, {}});
    if (!shader.IsValid())
        LogError("Failed to load shader '{}'", std::string(file_name, file_name + wcslen(file_name)));
    return shader;
}
}

ForwardPipeline::ForwardPipeline(IRenderDevice &device, const ForwardRenderingSettings &settings) :
    m_device_(device), m_settings_(settings), m_draw_list_builder_(device),
    m_shadow_system_(device, settings.shadow_map_size, settings.max_shadow_maps)
{
    const std::wstring shader_directory = ResolveShaderDirectory(settings.shader_directory);
    m_standard_vertex_shader_ = LoadShader(device, shader_directory, L"forward_standard.hlsl", L"VSMain", ShaderStage::Vertex);
    m_standard_pixel_shader_ = LoadShader(device, shader_directory, L"forward_standard.hlsl", L"PSMain", ShaderStage::Pixel);
    m_shadow_vertex_shader_ = LoadShader(device, shader_directory, L"forward_standard.hlsl", L"VSShadow", ShaderStage::Vertex);
    m_skybox_vertex_shader_ = LoadShader(device, shader_directory, L"skybox.hlsl", L"VSMain", ShaderStage::Vertex);
    m_skybox_pixel_shader_ = LoadShader(device, shader_directory, L"skybox.hlsl", L"PSMain", ShaderStage::Pixel);
    m_line_vertex_shader_ = LoadShader(device, shader_directory, L"line.hlsl", L"VSMain", ShaderStage::Vertex);
    m_line_pixel_shader_ = LoadShader(device, shader_directory, L"line.hlsl", L"PSMain", ShaderStage::Pixel);
    m_ui_vertex_shader_ = LoadShader(device, shader_directory, L"ui.hlsl", L"VSMain", ShaderStage::Vertex);
    m_ui_pixel_shader_ = LoadShader(device, shader_directory, L"ui.hlsl", L"PSMain", ShaderStage::Pixel);
    m_tonemap_vertex_shader_ = LoadShader(device, shader_directory, L"tonemap.hlsl", L"VSMain", ShaderStage::Vertex);
    m_tonemap_pixel_shader_ = LoadShader(device, shader_directory, L"tonemap.hlsl", L"PSMain", ShaderStage::Pixel);

    constexpr uint32_t white = 0xFFFFFFFF;
    TextureDesc white_desc;
    white_desc.format = Format::R8G8B8A8_UNorm;
    white_desc.usage = TextureUsage::ShaderResource;
    white_desc.initial_data = &white;
    white_desc.initial_row_pitch = sizeof(white);
    white_desc.debug_name = "CRS WhiteTexture";
    m_white_texture_ = Texture(device, white_desc);

    m_default_sampler_ = Sampler(device, SamplerDesc{Filter::Linear, AddressMode::Wrap, 8});

    AddPass(std::make_unique<ShadowPass>());
    AddPass(std::make_unique<SkyboxPass>(m_skybox_vertex_shader_.GetHandle(), m_skybox_pixel_shader_.GetHandle()));
    AddPass(std::make_unique<ForwardPass>());
    AddPass(std::make_unique<LinePass>(m_line_vertex_shader_.GetHandle(), m_line_pixel_shader_.GetHandle()));
    AddPass(std::make_unique<TonemapPass>(m_tonemap_vertex_shader_.GetHandle(), m_tonemap_pixel_shader_.GetHandle()));
    AddPass(std::make_unique<UiPass>(m_ui_vertex_shader_.GetHandle(), m_ui_pixel_shader_.GetHandle()));
}

std::unique_ptr<ICameraSettings> ForwardPipeline::CreateCameraSettings() const
{
    return std::make_unique<ForwardCameraSettings>();
}

Material ForwardPipeline::CreateDefaultMaterial() const
{
    Material material;

    MaterialPass forward;
    forward.vertex_shader = m_standard_vertex_shader_.GetHandle();
    forward.pixel_shader = m_standard_pixel_shader_.GetHandle();
    material.SetPass(PassTag::Forward, forward);

    MaterialPass shadow;
    shadow.vertex_shader = m_shadow_vertex_shader_.GetHandle();
    shadow.render_state.cull = CullMode::None;
    material.SetPass(PassTag::Shadow, shadow);

    material.BuildPropertyTable(m_device_);
    material.SetColor("_Color", Color(1, 1, 1, 1));
    return material;
}

void ForwardPipeline::AddPass(std::unique_ptr<IRenderPass> pass)
{
    m_passes_.push_back(std::move(pass));
}

std::vector<RenderView> ForwardPipeline::BuildCameraViews(const FrameSnapshot &frame) const
{
    std::vector<const CameraSnapshot *> cameras;
    for (const auto &camera : frame.cameras)
        cameras.push_back(&camera);

    std::ranges::stable_sort(cameras, [](const CameraSnapshot *a, const CameraSnapshot *b) {
        return a->common.priority < b->common.priority;
    });

    std::vector<RenderView> views;
    views.reserve(cameras.size());
    for (const auto camera : cameras)
    {
        const auto &common = camera->common;
        const auto settings = static_cast<const ForwardCameraSettings *>(camera->settings.get());
        if (!common.color_target.IsValid())
            continue;

        RenderView view;
        view.kind = RenderViewKind::Camera;
        view.view = common.view;
        view.projection = common.projection;
        view.view_projection = common.view * common.projection;
        view.previous_view_projection = common.previous_view_projection;
        view.position = common.position;
        view.viewport = common.viewport;
        view.scissor = {
            static_cast<int32_t>(common.viewport.x), static_cast<int32_t>(common.viewport.y),
            static_cast<int32_t>(common.viewport.x + common.viewport.width),
            static_cast<int32_t>(common.viewport.y + common.viewport.height)
        };
        view.color_target = common.color_target;
        view.depth_target = common.depth_target;
        view.color_format = m_device_.GetTextureDesc(common.color_target).format;
        view.depth_format = common.depth_target.IsValid()
                                ? m_device_.GetTextureDesc(common.depth_target).format
                                : Format::Unknown;
        view.sample_count = settings ? settings->sample_count : 1;
        view.color_load = common.color_load;
        view.depth_load = common.depth_load;
        view.clear_color = common.clear_color;
        view.culling_mask = common.culling_mask;
        view.draw_overlays = common.draw_overlays;
        view.camera_settings = settings;
        views.push_back(view);
    }
    return views;
}

TextureHandle ForwardPipeline::AcquireSceneColor(const size_t index, const TextureDesc &target_desc)
{
    if (m_scene_colors_.size() <= index)
        m_scene_colors_.resize(index + 1);

    auto &scene_color = m_scene_colors_[index];
    if (scene_color.IsValid())
    {
        const auto desc = m_device_.GetTextureDesc(scene_color.GetHandle());
        if (desc.width == target_desc.width && desc.height == target_desc.height)
            return scene_color.GetHandle();
    }

    TextureDesc desc;
    desc.width = target_desc.width;
    desc.height = target_desc.height;
    desc.format = Format::R16G16B16A16_Float;
    desc.usage = TextureUsage::RenderTarget | TextureUsage::ShaderResource;
    desc.debug_name = "CRS SceneColor";
    scene_color = Texture(m_device_, desc);
    return scene_color.GetHandle();
}

void ForwardPipeline::ClearView(ICommandList &command_list, const RenderView &view)
{
    if (view.color_load != LoadOp::Clear && view.depth_load != LoadOp::Clear)
        return;

    command_list.Transition(view.color_target, ResourceState::RenderTarget);
    if (view.depth_target.IsValid())
        command_list.Transition(view.depth_target, ResourceState::DepthWrite);

    RenderPassDesc pass_desc;
    pass_desc.color_targets[0] = view.color_target;
    pass_desc.color_target_count = 1;
    pass_desc.color_load = view.color_load;
    pass_desc.clear_color = view.clear_color;
    pass_desc.depth_target = view.depth_target;
    pass_desc.depth_load = view.depth_load;
    command_list.BeginRenderPass(pass_desc);
    command_list.EndRenderPass();
}

void ForwardPipeline::RenderSingleView(const FrameSnapshot &frame, ICommandList &command_list,
                                       const MaterialRegistry &materials, const RenderView &view,
                                       const FrameLighting &lighting)
{
    const auto visible = view.kind == RenderViewKind::Composite
                             ? std::vector<uint32_t>{}
                             : m_culling_stage_.Cull(view, frame.meshes);

    ViewConstants view_constants;
    view_constants.view = view.view;
    view_constants.projection = view.projection;
    view_constants.view_projection = view.view_projection;
    view_constants.previous_view_projection = view.previous_view_projection;
    view_constants.camera_position = Vector4(view.position.x, view.position.y, view.position.z, 1.0f);
    view_constants.time = Vector4(frame.time, frame.delta_time, 0, 0);

    static const DrawList kEmptyDrawList;
    for (const auto &pass : m_passes_)
    {
        if (!pass->AcceptsView(view, frame))
            continue;

        DrawList draw_list;
        if (pass->UsesDrawList())
            draw_list = m_draw_list_builder_.Build(pass->GetTag(), view, visible, frame, materials);

        const ProfileScope pass_scope(command_list, pass->GetName());
        const PassContext context = {
            command_list, m_device_, view, frame, pass->UsesDrawList() ? draw_list : kEmptyDrawList, materials,
            view_constants, lighting, m_white_texture_.GetHandle(), m_default_sampler_.GetHandle()
        };
        pass->Execute(context);
    }
}

void ForwardPipeline::Render(const FrameSnapshot &frame, ICommandList &command_list,
                             const MaterialRegistry &materials)
{
    const ProfileScope frame_scope(command_list, "ForwardPipeline");
    m_shadow_system_.BeginFrame();

    const auto camera_views = BuildCameraViews(frame);
    for (size_t i = 0; i < camera_views.size(); ++i)
    {
        const auto &camera_view = camera_views[i];
        const auto camera_name = std::format("Camera {}", i);
        const ProfileScope camera_scope(command_list, camera_name.c_str());
        const auto target_desc = m_device_.GetTextureDesc(camera_view.color_target);

        RenderView scene_view = camera_view;
        RenderView composite_view = camera_view;
        composite_view.kind = RenderViewKind::Composite;
        composite_view.depth_target = {};
        composite_view.depth_format = Format::Unknown;
        composite_view.color_load = LoadOp::Load;

        if (camera_view.sample_count == 1)
        {
            const auto scene_color = AcquireSceneColor(i, target_desc);
            scene_view.color_target = scene_color;
            scene_view.color_format = Format::R16G16B16A16_Float;
            if (camera_view.color_load == LoadOp::Load)
                scene_view.clear_color = {0, 0, 0, 0};
            scene_view.color_load = LoadOp::Clear;
            composite_view.scene_color = scene_color;
            composite_view.color_load = camera_view.color_load;
        }

        std::vector<RenderView> shadow_views;
        const auto lighting = m_shadow_system_.Prepare(frame, scene_view, shadow_views);
        if (!shadow_views.empty())
        {
            const ProfileScope shadow_scope(command_list, "Shadow Views");
            for (const auto &shadow_view : shadow_views)
                RenderSingleView(frame, command_list, materials, shadow_view, lighting);
        }

        {
            const ProfileScope scene_scope(command_list, "Scene");
            {
                const ProfileScope clear_scope(command_list, "Clear");
                ClearView(command_list, scene_view);
            }
            RenderSingleView(frame, command_list, materials, scene_view, lighting);
        }
        {
            const ProfileScope composite_scope(command_list, "Composite");
            RenderSingleView(frame, command_list, materials, composite_view, lighting);
        }

        if (HasFlag(target_desc.usage, TextureUsage::ShaderResource))
            command_list.Transition(camera_view.color_target, ResourceState::ShaderResource);
    }
}
}
