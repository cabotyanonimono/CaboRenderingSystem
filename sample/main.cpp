// CaboRenderingSystem.dll の動作確認用サンプル
// ウィンドウを開き、グリッドと回転する立方体の枠線を描きます。
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <chrono>
#include <format>
#include <memory>
#include <string>
#include <vector>

#include <CaboRenderingSystem/Material/compute_material.h>
#include <CaboRenderingSystem/render_engine.h>

using namespace CRS;

namespace
{
constexpr uint32_t kWidth = 1280;
constexpr uint32_t kHeight = 720;

std::unique_ptr<RenderEngine> g_engine;

/// <summary>
/// 実行ファイルのあるフォルダです（末尾に区切り文字つき）。
/// </summary>
std::wstring ExecutableDirectory()
{
    wchar_t module_path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, module_path, MAX_PATH);
    std::wstring directory = module_path;
    return directory.substr(0, directory.find_last_of(L"\\/") + 1);
}

/// <summary>
/// コンピュートシェーダーの動作確認です。最初のフレームでバッファに id * 2 を書き込み、読み戻して確かめます。
/// </summary>
struct ComputeTest
{
    static constexpr uint32_t kCount = 256;
    static constexpr uint32_t kMultiplier = 2;

    ShaderHandle shader;
    ComputeMaterial material;
    BufferHandle buffer;
    ReadbackTicket ticket;
    bool dispatched = false;
    bool finished = false;
    bool passed = false;

    /// <summary>
    /// シェーダー、PSO、書き込み先のバッファを作ります。
    /// </summary>
    bool Init(IRenderDevice &device)
    {
        ShaderSource source;
        source.file_path = ExecutableDirectory() + L"compute_test.hlsl";
        source.entry_point = L"CSMain";
        source.stage = ShaderStage::Compute;
        shader = device.CreateShader(source);
        const bool pipeline_ok = material.SetShader(device, shader);

        BufferDesc desc;
        desc.size = kCount * sizeof(uint32_t);
        desc.stride = sizeof(uint32_t);
        desc.usage = BufferUsage::Structured | BufferUsage::ReadWrite;
        desc.debug_name = "ComputeTest Output";
        buffer = device.CreateBuffer(desc);
        material.SetUInt("g_count", kCount);
        material.SetUInt("g_multiplier", kMultiplier);
        material.SetBuffer("g_output", buffer);
        return pipeline_ok && buffer.IsValid();
    }

    /// <summary>
    /// まだなら、Dispatchと読み戻しの依頼を記録します。BeginFrameとEndFrameの間で呼びます。
    /// </summary>
    void Record(IRenderDevice &device, ICommandList &command_list)
    {
        if (dispatched)
            return;

        ProfileScope scope(command_list, "ComputeTest");
        command_list.Transition(buffer, ResourceState::UnorderedAccess);
        material.Apply(command_list);
        command_list.Dispatch((kCount + 63) / 64);
        ticket = device.RequestReadback(buffer);
        dispatched = true;
    }

    /// <summary>
    /// 読み戻しが終わっていれば結果を確かめ、デバッグ出力に書きます。
    /// </summary>
    void Poll(IRenderDevice &device)
    {
        if (!dispatched || finished)
            return;

        std::vector<uint8_t> data;
        if (!device.TryGetReadback(ticket, data))
            return;

        finished = true;
        passed = data.size() == kCount * sizeof(uint32_t);
        const auto values = reinterpret_cast<const uint32_t *>(data.data());
        for (uint32_t i = 0; passed && i < kCount; ++i)
            passed = values[i] == i * kMultiplier;

        OutputDebugStringA(passed ? "ComputeTest: OK\n" : "ComputeTest: NG\n");
    }

    /// <summary>
    /// 作ったものを破棄します。
    /// </summary>
    void Destroy(IRenderDevice &device)
    {
        device.Destroy(buffer);
        device.Destroy(material.PipelineState());
        device.Destroy(shader);
    }

    /// <summary>
    /// ウィンドウタイトルに出す結果です。
    /// </summary>
    const wchar_t *Status() const
    {
        if (!finished)
            return L"running";
        return passed ? L"OK" : L"NG";
    }
};

ComputeTest g_compute_test;

/// <summary>
/// コンピュートシェーダーで動かすパーティクルの噴水です。毎フレームCSで位置を更新し、同じバッファを頂点シェーダーで読んで加算合成で描きます。
/// </summary>
struct ParticleEffect
{
    static constexpr uint32_t kCount = 32768;
    static constexpr uint32_t kParticleStride = 32;

    struct Params
    {
        Matrix view_projection;
        Vector3 camera_right;
        float time;
        Vector3 camera_up;
        float delta_time;
        uint32_t count;
        float padding[3];
    };

    ShaderHandle compute_shader;
    ShaderHandle vertex_shader;
    ShaderHandle pixel_shader;
    PipelineStateHandle compute_pipeline;
    PipelineStateHandle draw_pipeline;
    BufferHandle particles;

    /// <summary>
    /// シェーダー、PSO、パーティクルのバッファを作ります。
    /// </summary>
    bool Init(IRenderDevice &device)
    {
        const auto path = ExecutableDirectory() + L"particles.hlsl";
        compute_shader = device.CreateShader({path, L"CSUpdate", ShaderStage::Compute});
        vertex_shader = device.CreateShader({path, L"VSMain", ShaderStage::Vertex});
        pixel_shader = device.CreateShader({path, L"PSMain", ShaderStage::Pixel});
        compute_pipeline = device.CreateComputePipelineState({compute_shader});

        PipelineStateDesc desc;
        desc.vertex_shader = vertex_shader;
        desc.pixel_shader = pixel_shader;
        desc.render_state.blend = BlendMode::Additive;
        desc.render_state.cull = CullMode::None;
        desc.render_state.depth.write_enable = false;
        desc.render_target_formats[0] = device.GetBackBufferFormat();
        desc.depth_format = Format::D32_Float;
        draw_pipeline = device.CreatePipelineState(desc);

        const std::vector<uint8_t> zeros(static_cast<size_t>(kCount) * kParticleStride, 0);
        BufferDesc buffer_desc;
        buffer_desc.size = zeros.size();
        buffer_desc.stride = kParticleStride;
        buffer_desc.usage = BufferUsage::Structured | BufferUsage::ReadWrite;
        buffer_desc.initial_data = zeros.data();
        buffer_desc.debug_name = "Particles";
        particles = device.CreateBuffer(buffer_desc);

        return compute_pipeline.IsValid() && draw_pipeline.IsValid() && particles.IsValid();
    }

    /// <summary>
    /// CSでパーティクルを進め、カメラの描画先に加算合成で描きます。エンジンのRenderの後に呼びます。
    /// </summary>
    void Render(ICommandList &command_list, const CameraCommon &camera, TextureHandle depth, float time, float delta)
    {
        Params params = {};
        params.view_projection = camera.view * camera.projection;
        params.camera_right = Vector3(camera.view._11, camera.view._21, camera.view._31);
        params.camera_up = Vector3(camera.view._12, camera.view._22, camera.view._32);
        params.time = time;
        params.delta_time = delta;
        params.count = kCount;

        Bindings bindings;
        bindings.constants[0] = ConstantBinding::FromValue(params);

        {
            ProfileScope scope(command_list, "ParticleUpdate");
            command_list.Transition(particles, ResourceState::UnorderedAccess);
            command_list.SetComputePipelineState(compute_pipeline);
            bindings.unordered_access[0].buffer = particles;
            command_list.SetComputeBindings(bindings);
            command_list.Dispatch((kCount + 255) / 256);
        }

        ProfileScope scope(command_list, "ParticleDraw");
        command_list.Transition(particles, ResourceState::ShaderResource);
        command_list.Transition(camera.color_target, ResourceState::RenderTarget);
        command_list.Transition(depth, ResourceState::DepthWrite);

        RenderPassDesc pass;
        pass.color_targets[0] = camera.color_target;
        pass.color_target_count = 1;
        pass.depth_target = depth;
        command_list.BeginRenderPass(pass);
        command_list.SetViewport({camera.viewport.x, camera.viewport.y, camera.viewport.width,
                                  camera.viewport.height, 0, 1});
        command_list.SetScissor({0, 0, static_cast<int32_t>(camera.viewport.width),
                                 static_cast<int32_t>(camera.viewport.height)});
        command_list.SetPipelineState(draw_pipeline);
        bindings.unordered_access[0] = {};
        bindings.resources[0].buffer = particles;
        command_list.SetBindings(bindings);
        command_list.Draw(kCount * 6);
        command_list.EndRenderPass();
    }

    /// <summary>
    /// 作ったものを破棄します。
    /// </summary>
    void Destroy(IRenderDevice &device)
    {
        device.Destroy(particles);
        device.Destroy(draw_pipeline);
        device.Destroy(compute_pipeline);
        device.Destroy(pixel_shader);
        device.Destroy(vertex_shader);
        device.Destroy(compute_shader);
    }
};

ParticleEffect g_particles;

// GPUプロファイルとFPSをデバッグ出力に書き、ウィンドウタイトルにも出す
void ReportGpuProfile(const HWND hwnd, const float fps)
{
    GpuProfileResult profile;
    if (!g_engine->GetGpuProfile(profile))
        return;

    std::string report = std::format("---- GPU frame {} : {:.1f} FPS ({:.3f} ms), GPU {:.3f} ms, {} draws ----\n",
                                     profile.frame_number, fps, 1000.0f / fps, profile.frame_gpu_ms,
                                     profile.draw_calls);
    for (const auto &scope : profile.scopes)
    {
        report += std::format("{:{}}{:<20} {:8.3f} ms  draws {:5}  verts {:8}  indices {:8}\n", "", scope.depth * 2,
                              scope.name, scope.gpu_ms, scope.draw_calls, scope.vertices, scope.indices);
    }
    OutputDebugStringA(report.c_str());

    const auto title = std::format(
        L"CaboRenderingSystem Sample - {:.1f} FPS ({:.2f} ms) / GPU {:.3f} ms / {} draws / compute {}", fps,
        1000.0f / fps, profile.frame_gpu_ms, profile.draw_calls, g_compute_test.Status());
    SetWindowTextW(hwnd, title.c_str());
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_SIZE:
        if (g_engine && wparam != SIZE_MINIMIZED)
            g_engine->Resize(LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

HWND CreateSampleWindow(HINSTANCE instance)
{
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"CaboSample";
    RegisterClassExW(&wc);

    RECT rect = {0, 0, static_cast<LONG>(kWidth), static_cast<LONG>(kHeight)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    return CreateWindowExW(0, wc.lpszClassName, L"CaboRenderingSystem Sample", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                           CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance,
                           nullptr);
}

// 立方体の12本の辺を描く
void DrawCube(ImmediateDrawStore &immediate, const Matrix &world, const Color &color)
{
    Vector3 v[8];
    for (int i = 0; i < 8; ++i)
        v[i] = Vector3::Transform(Vector3((i & 1) ? 1.f : -1.f, (i & 2) ? 1.f : -1.f, (i & 4) ? 1.f : -1.f), world);
    constexpr int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                  {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto &e : edges)
        immediate.AddLine(v[e[0]], v[e[1]], color);
}

void DrawGrid(ImmediateDrawStore &immediate)
{
    const Color color(0.35f, 0.35f, 0.35f, 1.f);
    for (int i = -10; i <= 10; ++i)
    {
        const auto f = static_cast<float>(i);
        immediate.AddLine(Vector3(f, 0, -10), Vector3(f, 0, 10), color);
        immediate.AddLine(Vector3(-10, 0, f), Vector3(10, 0, f), color);
    }
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    const HWND hwnd = CreateSampleWindow(instance);
    if (hwnd == nullptr)
        return 1;

    g_engine = CreateDefaultRenderEngine(hwnd, kWidth, kHeight);
    if (g_engine == nullptr)
    {
        MessageBoxW(hwnd, L"DX12の初期化に失敗しました", L"CaboRenderingSystem", MB_ICONERROR);
        return 1;
    }
    ShowWindow(hwnd, show);

    g_engine->SetGpuProfilingEnabled(true);
    if (!g_compute_test.Init(g_engine->Device()))
        OutputDebugStringA("ComputeTest: init failed\n");
    const bool particles_ready = g_particles.Init(g_engine->Device());
    if (!particles_ready)
        OutputDebugStringA("ParticleEffect: init failed\n");

    const CameraId camera = g_engine->CreateCamera();
    const auto start = std::chrono::steady_clock::now();
    auto previous = start;
    auto last_report = start;
    uint32_t frames_since_report = 0;

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        const float time = std::chrono::duration<float>(now - start).count();
        const float delta = std::chrono::duration<float>(now - previous).count();
        previous = now;

        const auto width = static_cast<float>(g_engine->Width());
        const auto height = static_cast<float>(g_engine->Height());
        if (width <= 0 || height <= 0)
            continue;

        // カメラ（毎フレーム更新しないと描かれない）
        CameraCommon common;
        common.position = Vector3(6.f, 4.f, 8.f);
        common.view = Matrix::CreateLookAt(common.position, Vector3::Zero, Vector3::Up);
        common.projection = Matrix::CreatePerspectiveFieldOfView(DirectX::XM_PIDIV4, width / height, 0.1f, 100.f);
        common.viewport = {0, 0, width, height, 0, 1};
        common.color_target = g_engine->Device().GetBackBuffer();
        common.depth_target = g_engine->MainDepth();
        common.clear_color = {0.10f, 0.12f, 0.16f, 1.f};
        common.draw_overlays = true;
        g_engine->Cameras().Update(camera, common);

        // 描くもの
        auto &immediate = g_engine->Immediate();
        DrawGrid(immediate);
        const Matrix world = Matrix::CreateRotationY(time) * Matrix::CreateRotationX(time * 0.5f) *
                             Matrix::CreateTranslation(0, 1.5f, 0);
        DrawCube(immediate, world, Color(1.f, 0.6f, 0.1f, 1.f));

        g_engine->BeginFrame();
        if (!g_compute_test.dispatched && g_compute_test.material.PipelineState().IsValid())
            g_compute_test.Record(g_engine->Device(), g_engine->CommandList());
        g_engine->Render(time, delta);
        if (particles_ready)
            g_particles.Render(g_engine->CommandList(), common, g_engine->MainDepth(), time, delta);
        g_engine->EndFrame();
        g_compute_test.Poll(g_engine->Device());
        ++frames_since_report;

        if (now - last_report >= std::chrono::seconds(1))
        {
            const float elapsed = std::chrono::duration<float>(now - last_report).count();
            ReportGpuProfile(hwnd, static_cast<float>(frames_since_report) / elapsed);
            frames_since_report = 0;
            last_report = now;
        }
    }

    g_engine->DestroyCamera(camera);
    g_compute_test.Destroy(g_engine->Device());
    g_particles.Destroy(g_engine->Device());
    g_engine.reset();
    return 0;
}
