#include "pch.h"
#include "CaboRenderingSystem/RHI/create_render_device.h"

#include "CaboRenderingSystem/RHI/D3D12/d3d12_render_device.h"

namespace CRS
{
std::unique_ptr<IRenderDevice> CreateD3D12RenderDevice(void *window_handle, const uint32_t width,
                                                       const uint32_t height)
{
    return D3D12RenderDevice::Create(static_cast<HWND>(window_handle), width, height);
}
}
