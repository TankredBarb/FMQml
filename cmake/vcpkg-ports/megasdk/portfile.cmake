vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO meganz/sdk
    REF v9.16.1
    SHA512 45cd2879fea342375f02d64c98b24ea7d115c7cbacc45ccb9331eab37dd914c317476cf77d8ffde0e1c2f577037110a3de9a95fa833fc6eb77a4cc82a5b6e276
    HEAD_REF master
)

vcpkg_replace_string("${SOURCE_PATH}/CMakeLists.txt"
    "include(vcpkg_management)"
    "# include(vcpkg_management)"
)
vcpkg_replace_string("${SOURCE_PATH}/CMakeLists.txt"
    "process_vcpkg_libraries(\${CMAKE_CURRENT_LIST_DIR}/cmake)"
    "# process_vcpkg_libraries(\${CMAKE_CURRENT_LIST_DIR}/cmake)"
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DVCPKG_ROOT=$ENV{VCPKG_ROOT}
        -DENABLE_SYNC=OFF
        -DENABLE_CHAT=OFF
        -DUSE_FREEIMAGE=OFF
        -DUSE_FFMPEG=OFF
        -DUSE_LIBUV=OFF
        -DUSE_PDFIUM=OFF
        -DUSE_READLINE=OFF
        -DENABLE_SDKLIB_EXAMPLES=OFF
        -DENABLE_SDKLIB_TESTS=OFF
        -DSDKLIB_STANDALONE=ON
)

# Build Debug and Release configurations
vcpkg_cmake_build(TARGET SDKlib)

# Copy headers
file(INSTALL "${SOURCE_PATH}/include/" DESTINATION "${CURRENT_PACKAGES_DIR}/include")

# Copy generated config.h header (it is in binary dir under mega/)
file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/mega/config.h" DESTINATION "${CURRENT_PACKAGES_DIR}/include/mega")

# Copy compiled libraries
if(VCPKG_TARGET_IS_WINDOWS)
    file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/SDKlib.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/lib")
    file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-dbg/SDKlibd.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib")
    file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/third_party/ccronexpr/ccronexpr.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/lib")
    file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-dbg/third_party/ccronexpr/ccronexpr.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib")
else()
    file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/libSDKlib.a" DESTINATION "${CURRENT_PACKAGES_DIR}/lib")
    file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-dbg/libSDKlibd.a" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib")
    file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/third_party/ccronexpr/libccronexpr.a" DESTINATION "${CURRENT_PACKAGES_DIR}/lib")
    file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-dbg/third_party/ccronexpr/libccronexpr.a" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib")
endif()

# Copy FindSDKlib.cmake module
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/FindSDKlib.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/sdklib")

# Install license
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
