find_path(SDKLIB_INCLUDE_DIR NAMES megaapi.h PATH_SUFFIXES include)
find_library(SDKLIB_LIBRARY_RELEASE NAMES SDKlib PATH_SUFFIXES lib)
find_library(SDKLIB_LIBRARY_DEBUG NAMES SDKlibd SDKlib PATH_SUFFIXES debug/lib lib)
find_library(CCRONEXPR_LIBRARY_RELEASE NAMES ccronexpr PATH_SUFFIXES lib)
find_library(CCRONEXPR_LIBRARY_DEBUG NAMES ccronexpr PATH_SUFFIXES debug/lib lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDKlib
    REQUIRED_VARS SDKLIB_LIBRARY_RELEASE SDKLIB_INCLUDE_DIR CCRONEXPR_LIBRARY_RELEASE
)

if(SDKlib_FOUND)
    set(SDKLIB_INCLUDE_DIRS ${SDKLIB_INCLUDE_DIR})
    set(SDKLIB_LIBRARIES ${SDKLIB_LIBRARY_RELEASE} ${CCRONEXPR_LIBRARY_RELEASE})

    if(NOT TARGET MEGA::SDKlib)
        add_library(MEGA::SDKlib STATIC IMPORTED)
        set_target_properties(MEGA::SDKlib PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${SDKLIB_INCLUDE_DIRS}"
        )
        if(SDKLIB_LIBRARY_DEBUG)
            set_target_properties(MEGA::SDKlib PROPERTIES
                IMPORTED_LOCATION_DEBUG "${SDKLIB_LIBRARY_DEBUG}"
                IMPORTED_LOCATION_RELEASE "${SDKLIB_LIBRARY_RELEASE}"
                IMPORTED_LOCATION "${SDKLIB_LIBRARY_RELEASE}"
            )
        else()
            set_target_properties(MEGA::SDKlib PROPERTIES
                IMPORTED_LOCATION "${SDKLIB_LIBRARY_RELEASE}"
            )
        endif()

        # Resolve static dependencies
        find_package(cryptopp CONFIG REQUIRED)
        find_package(unofficial-sodium CONFIG REQUIRED)
        find_package(unofficial-sqlite3 CONFIG REQUIRED)
        find_package(CURL REQUIRED)
        find_package(OpenSSL REQUIRED)
        find_package(c-ares CONFIG REQUIRED)
        find_package(ZLIB REQUIRED)
        find_package(ICU COMPONENTS uc data REQUIRED)

        target_link_libraries(MEGA::SDKlib INTERFACE
            cryptopp::cryptopp
            unofficial-sodium::sodium
            unofficial::sqlite3::sqlite3
            CURL::libcurl
            OpenSSL::SSL OpenSSL::Crypto
            c-ares::cares
            ZLIB::ZLIB
            ICU::uc
            ICU::data
        )

        if(WIN32)
            target_link_libraries(MEGA::SDKlib INTERFACE
                ws2_32
                winhttp
                Shlwapi
                Secur32
                crypt32
                Wldap32
            )
        endif()

        if(CCRONEXPR_LIBRARY_RELEASE)
            if(CCRONEXPR_LIBRARY_DEBUG)
                target_link_libraries(MEGA::SDKlib INTERFACE
                    "$<$<CONFIG:Debug>:${CCRONEXPR_LIBRARY_DEBUG}>"
                    "$<$<NOT:$<CONFIG:Debug>>:${CCRONEXPR_LIBRARY_RELEASE}>"
                )
            else()
                target_link_libraries(MEGA::SDKlib INTERFACE "${CCRONEXPR_LIBRARY_RELEASE}")
            endif()
        endif()

        if(UNIX AND NOT APPLE)
            target_link_libraries(MEGA::SDKlib INTERFACE udev)
        endif()
    endif()
endif()
