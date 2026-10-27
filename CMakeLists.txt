cmake_minimum_required(VERSION 3.31)
project(CaboRenderingSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(directx-headers CONFIG REQUIRED)
find_package(directxtk12 CONFIG REQUIRED)
find_package(directx-dxc CONFIG REQUIRED)

file(GLOB_RECURSE CRS_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(CaboRenderingSystem SHARED ${CRS_SOURCES})

# DLLから公開するのは CRS_API（include/CaboRenderingSystem/export.h）を付けたものだけ

# include/ : 利用側に見せるヘッダ（DX12の型を含まない）
# src/     : 内部用（RHI/D3D12 など）。利用側には見えない
target_include_directories(CaboRenderingSystem
        PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_precompile_headers(CaboRenderingSystem PRIVATE src/pch.h)
target_compile_options(CaboRenderingSystem PRIVATE /utf-8)

target_link_libraries(CaboRenderingSystem
        PUBLIC  Microsoft::DirectXTK12
        PRIVATE Microsoft::DirectX-Headers
        Microsoft::DirectXShaderCompiler
        d3d12 dxgi dxguid)

add_subdirectory(sample)
