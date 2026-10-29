#pragma once
// SimpleMathがwindows.hを読み込むので、min / maxマクロを無効にしておく
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <DirectXCollision.h>
#include <directxtk12/SimpleMath.h>

namespace CRS
{
using Vector2 = DirectX::SimpleMath::Vector2;
using Vector3 = DirectX::SimpleMath::Vector3;
using Vector4 = DirectX::SimpleMath::Vector4;
using Matrix = DirectX::SimpleMath::Matrix;
using Quaternion = DirectX::SimpleMath::Quaternion;
using Color = DirectX::SimpleMath::Color;
}
