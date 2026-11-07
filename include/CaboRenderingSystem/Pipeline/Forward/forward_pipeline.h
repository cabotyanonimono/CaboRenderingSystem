#pragma once
#include "CaboRenderingSystem/export.h"
#include <memory>
#include <string>
#include <vector>

#include "CaboRenderingSystem/Pipeline/i_render_pipeline.h"
#include "CaboRenderingSystem/Pipeline/render_stages.h"
#include "CaboRenderingSystem/Pipeline/shadow_system.h"
#include "CaboRenderingSystem/RHI/gpu_resources.h"

namespace CRS
{
/// <summary>
/// Forwardパイプライン用のカメラ設定です。
/// sample_countはMSAAのサンプル数で、描画先のテクスチャも同じサンプル数で作る必要があります。
/// exposureはトーンマップの前にシーンの色へ掛ける露出です。
/// </summary>
class ForwardCameraSettings final : public ICameraSettings
{
public:
    uint32_t sample_count = 1;
    bool depth_prepass = false;
    float exposure = 1.0f;

    std::unique_ptr<ICameraSettings> Clone() const override
    {
        return std::make_unique<ForwardCameraSettings>(*this);
    }
};

/// <summary>
/// シーン全体で1つのパイプライン設定です。
/// shader_directoryが空なら、CaboRenderingSystem.dll と同じフォルダの Shaders/ を使います。
/// </summary>
struct ForwardRenderingSettings
{
    uint32_t shadow_map_size = 2048;
    uint32_t max_shadow_maps = 10;
    std::wstring shader_directory;
};

/// <summary>
/// Forwardパイプラインです。カメラごとに、影のビューを描いてから、登録されたパスを登録順で実行します。
/// シーンはHDRのシーンカラーに描き、Compositeのビューでトーンマップしてカメラの描画先に書き出します。
/// </summary>
class CRS_API ForwardPipeline final : public IRenderPipeline
{
public:
    ForwardPipeline(const ForwardPipeline &) = delete;
    ForwardPipeline &operator=(const ForwardPipeline &) = delete;

private:
    IRenderDevice &m_device_;
    ForwardRenderingSettings m_settings_;
    std::vector<std::unique_ptr<IRenderPass>> m_passes_;
    CullingStage m_culling_stage_;
    DrawListBuilder m_draw_list_builder_;
    ShadowSystem m_shadow_system_;

    Shader m_standard_vertex_shader_;
    Shader m_standard_pixel_shader_;
    Shader m_shadow_vertex_shader_;
    Shader m_skybox_vertex_shader_;
    Shader m_skybox_pixel_shader_;
    Shader m_line_vertex_shader_;
    Shader m_line_pixel_shader_;
    Shader m_ui_vertex_shader_;
    Shader m_ui_pixel_shader_;
    Shader m_tonemap_vertex_shader_;
    Shader m_tonemap_pixel_shader_;
    Texture m_white_texture_;
    Sampler m_default_sampler_;
    std::vector<Texture> m_scene_colors_;

    // スナップショットのカメラを、priorityの小さい順にRenderViewへ変換する
    std::vector<RenderView> BuildCameraViews(const FrameSnapshot &frame) const;
    /// <summary>
    /// index番目のカメラが使うHDRのシーンカラーを、描画先と同じ大きさで用意します。大きさが変わったら作り直します。
    /// </summary>
    TextureHandle AcquireSceneColor(size_t index, const TextureDesc &target_desc);
    /// <summary>
    /// カメラの描画先を、ビューの設定でクリアします。以降のパスは上に重ねて描きます。
    /// </summary>
    static void ClearView(ICommandList &command_list, const RenderView &view);
    // 1つのビューについて、受け付けるパスを順に実行する（カリングと描画リストの作成もここで行う）
    void RenderSingleView(const FrameSnapshot &frame, ICommandList &command_list, const MaterialRegistry &materials,
                          const RenderView &view, const FrameLighting &lighting);

public:
    /// <summary>
    /// 組み込みのシェーダーをコンパイルし、影のテクスチャなどを作って、標準のパス（Shadow → Skybox → Forward → Line → Tonemap → UI）を登録します。
    /// </summary>
    ForwardPipeline(IRenderDevice &device, const ForwardRenderingSettings &settings = {});

    std::unique_ptr<ICameraSettings> CreateCameraSettings() const override;
    /// <summary>
    /// 組み込みのforward_standard.hlslを使い、Forwardパス（ライティングあり）とShadowパスを持つマテリアルを作ります。
    /// </summary>
    Material CreateDefaultMaterial() const override;
    void AddPass(std::unique_ptr<IRenderPass> pass) override;
    void Render(const FrameSnapshot &frame, ICommandList &command_list, const MaterialRegistry &materials) override;

    /// <summary>
    /// 作成時に渡した設定です。
    /// </summary>
    const ForwardRenderingSettings &Settings() const
    {
        return m_settings_;
    }
};
}
