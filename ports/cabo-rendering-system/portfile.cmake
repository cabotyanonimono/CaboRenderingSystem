# CaboRenderingSystem は DLL としてだけ提供する
vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)

# 新しいバージョンを出すときは、REF を push 済みのコミットに変え、versions/ も更新する（README 参照）
vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/cabotyanonimono/CaboRenderingSystem.git
    REF eebc68df31565b2dc4e6b9c3c352c3f77f331acc
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCRS_BUILD_SAMPLE=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME CaboRenderingSystem CONFIG_PATH share/CaboRenderingSystem)

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
# LICENSE ファイルを追加したら vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE") に置き換える
file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright" "Copyright (c) cabotya_nimono. All rights reserved.\n")
