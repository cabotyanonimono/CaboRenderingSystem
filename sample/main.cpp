// CaboRenderingSystem.dll の動作確認用サンプル
// ウィンドウを開き、グリッドと回転する立方体の枠線を描きます。
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <chrono>
#include <memory>

#include <CaboRenderingSystem/render_engine.h>

using namespace CRS;

namespace
{
constexpr uint32_t kWidth = 1280;
constexpr uint32_t kHeight = 720;

std::unique_ptr<RenderEngine> g_engine;

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

    const CameraId camera = g_engine->CreateCamera();
    const auto start = std::chrono::steady_clock::now();
    auto previous = start;

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

        g_engine->RenderFrame(time, delta);
    }

    g_engine->DestroyCamera(camera);
    g_engine.reset();
    return 0;
}
