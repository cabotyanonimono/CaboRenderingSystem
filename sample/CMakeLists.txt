# CaboRenderingSystem.dll の動作確認用。DX12のヘッダやlibは一切指定しない
add_executable(Sample WIN32 main.cpp)
target_link_libraries(Sample PRIVATE CaboRenderingSystem)
target_compile_options(Sample PRIVATE /utf-8)
target_compile_definitions(Sample PRIVATE UNICODE _UNICODE)

# 実行に必要なものを exe の隣にコピーする
#  - CaboRenderingSystem.dll / DirectXTK12.dll / dxcompiler.dll（リンクしているDLL）
#  - dxil.dll（DXCが実行時に読み込む。リンクされないので個別にコピー）
#  - Shaders/（実行時にDXCでコンパイルする）
add_custom_command(TARGET Sample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:Sample>
            $<TARGET_FILE:CaboRenderingSystem>
            $<TARGET_FILE:Microsoft::DirectXShaderCompiler>
            $<TARGET_FILE_DIR:Microsoft::DirectXShaderCompiler>/dxil.dll
            $<TARGET_FILE_DIR:Sample>
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${PROJECT_SOURCE_DIR}/src/CaboRenderingSystem/Shaders
            $<TARGET_FILE_DIR:Sample>/Shaders
        COMMAND_EXPAND_LISTS)
