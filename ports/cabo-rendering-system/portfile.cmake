# CaboRenderingSystem は DLL としてだけ提供する
vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)

# 新しいバージョンを使うときは、REF を push 済みのコミットに変え、SHA512 を 0 にして一度インストールする。
# vcpkg がエラーで正しいハッシュ（Actual hash）を表示するので、それを SHA512 に貼る。
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO cabotyanonimono/CaboRenderingSystem
    REF 5c09f279957fd0f04b3f4729e11d9103819e97b9
    SHA512 0
    HEAD_REF main
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
