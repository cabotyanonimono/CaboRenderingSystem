cmake_minimum_required(VERSION 3.24)
project(CaboRenderingSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(directx-headers CONFIG REQUIRED)
find_package(directxtk12 CONFIG REQUIRED)
find_package(directx-dxc CONFIG REQUIRED)

file(GLOB_RECURSE CRS_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(CaboRenderingSystem SHARED ${CRS_SOURCES})
add_library(CRS::CaboRenderingSystem ALIAS CaboRenderingSystem)

# DLLから公開するのは CRS_API（include/CaboRenderingSystem/export.h）を付けたものだけ

# include/ : 利用側に見せるヘッダ（DX12の型は include/.../RHI/D3D12 だけ）
# src/     : 内部用。利用側には見えない
target_include_directories(CaboRenderingSystem
        PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_precompile_headers(CaboRenderingSystem PRIVATE src/pch.h)
target_compile_options(CaboRenderingSystem PRIVATE /utf-8)

# RHI/D3D12 のヘッダを公開しているので、DirectX-Headers と d3d12 / dxgi も利用側に伝える
target_link_libraries(CaboRenderingSystem
        PUBLIC  Microsoft::DirectXTK12
                Microsoft::DirectX-Headers
                d3d12 dxgi dxguid
        PRIVATE Microsoft::DirectXShaderCompiler)

# ---- インストール（vcpkg などから find_package(CaboRenderingSystem) で使えるようにする） ----
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CRS_CONFIG_DIR ${CMAKE_INSTALL_DATADIR}/CaboRenderingSystem)

install(TARGETS CaboRenderingSystem EXPORT CaboRenderingSystemTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
# シェーダーは share/CaboRenderingSystem/Shaders に置く（利用側は CaboRenderingSystem_SHADER_DIR で参照）
install(DIRECTORY src/CaboRenderingSystem/Shaders DESTINATION ${CRS_CONFIG_DIR})

install(EXPORT CaboRenderingSystemTargets
        NAMESPACE CRS::
        DESTINATION ${CRS_CONFIG_DIR})
configure_package_config_file(cmake/CaboRenderingSystemConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/CaboRenderingSystemConfig.cmake
        INSTALL_DESTINATION ${CRS_CONFIG_DIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CaboRenderingSystemConfig.cmake DESTINATION ${CRS_CONFIG_DIR})

# ---- サンプル（このリポジトリを直接ビルドしたときだけ） ----
option(CRS_BUILD_SAMPLE "Build the sample application" ${PROJECT_IS_TOP_LEVEL})
if (CRS_BUILD_SAMPLE)
    add_subdirectory(sample)
endif ()
