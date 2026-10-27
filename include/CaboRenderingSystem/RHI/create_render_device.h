#pragma once
#include "CaboRenderingSystem/export.h"
#include <cstdint>
#include <memory>

#include "CaboRenderingSystem/RHI/i_render_device.h"

namespace CRS
{
/// <summary>
/// D3D12のデバイスを作成します。DX12の型を利用側に見せないための入口です。
/// window_handleにはHWNDを渡します。失敗した場合はnullptrを返します。
/// </summary>
CRS_API std::unique_ptr<IRenderDevice> CreateD3D12RenderDevice(void *window_handle, uint32_t width, uint32_t height);
}
