function(check_libtorrent_upload_accounting)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
    # try_compile does not inherit the parent's single-config build type.
    # A default Debug probe cannot link a release-only MSVC static library.
    if (NOT CMAKE_TRY_COMPILE_CONFIGURATION AND CMAKE_BUILD_TYPE)
        set(CMAKE_TRY_COMPILE_CONFIGURATION "${CMAKE_BUILD_TYPE}")
    endif()
    # CheckPackages runs before CommonConfig creates qbt_common_cfg. Apply
    # the same Windows header, compiler and runtime settings to this probe.
    if (MSVC)
        if (MSVC_RUNTIME_DYNAMIC)
            set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        else()
            set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        endif()
        string(APPEND CMAKE_CXX_FLAGS " /permissive- /utf-8 /Zc:__cplusplus /D_WIN32_WINNT=0x0A00 /DNOMINMAX /DWIN32_LEAN_AND_MEAN")
    endif()
    set(probeSource "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/qbt_upload_accounting.cpp")
    set(probeLog "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/qbt_upload_accounting.log")
    # Recheck if the user selects another installation in an existing build tree.
    unset(QBT_HAS_UPLOAD_ACCOUNTING CACHE)
    file(WRITE "${probeSource}" "
    #include <libtorrent/version.hpp>
    #include <libtorrent/settings_pack.hpp>
    #include <libtorrent/add_torrent_params.hpp>
    #include <libtorrent/torrent_status.hpp>
    #include <libtorrent/alert_types.hpp>
    #if !defined(LIBTORRENT_FAKE_UPLOAD_ABI) || LIBTORRENT_FAKE_UPLOAD_ABI != 1
    #error Missing custom upload accounting
    #endif
    int main()
    {
        libtorrent::settings_pack settings;
        settings.set_int(libtorrent::settings_pack::fake_upload_ratio, 1);
        settings.set_bool(libtorrent::settings_pack::fake_upload_ratio_randomization, true);
        libtorrent::add_torrent_params params;
        params.total_real_uploaded = 0;
        libtorrent::torrent_status status;
        status.all_time_real_uploaded = 0;
        (void)params;
        (void)status;
        auto real = &libtorrent::tracker_announce_alert::total_real_uploaded;
        auto reported = &libtorrent::tracker_announce_alert::total_uploaded;
        (void)real;
        (void)reported;
        return libtorrent::fake_upload_abi_v1() != LIBTORRENT_FAKE_UPLOAD_ABI;
    }
    ")
    message(STATUS "Performing Test QBT_HAS_UPLOAD_ACCOUNTING")
    try_compile(QBT_HAS_UPLOAD_ACCOUNTING
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/qbt_upload_accounting"
        "${probeSource}"
        LINK_LIBRARIES LibtorrentRasterbar::torrent-rasterbar OpenSSL::Crypto
        OUTPUT_VARIABLE probeOutput
    )
    file(WRITE "${probeLog}"
        "LibtorrentRasterbar_DIR: ${LibtorrentRasterbar_DIR}\n"
        "LibtorrentRasterbar_VERSION: ${LibtorrentRasterbar_VERSION}\n"
        "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}\n"
        "CMAKE_TRY_COMPILE_CONFIGURATION: ${CMAKE_TRY_COMPILE_CONFIGURATION}\n"
        "CMAKE_MSVC_RUNTIME_LIBRARY: ${CMAKE_MSVC_RUNTIME_LIBRARY}\n"
        "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}\n\n"
        "${probeOutput}"
    )
    if (NOT QBT_HAS_UPLOAD_ACCOUNTING)
        message(STATUS "Performing Test QBT_HAS_UPLOAD_ACCOUNTING - Failed")
        message(FATAL_ERROR
            "The custom libtorrent compile/link check failed.\n"
            "${probeOutput}\n"
            "Full probe output: ${probeLog}\n"
            "Use the custom libtorrent from deps/libtorrent/2.0.15 or 2.1.2 and a matching build configuration. "
            "Set LibtorrentRasterbar_DIR to its installation. Official libraries with the same version are not ABI compatible."
        )
    endif()
    message(STATUS "Performing Test QBT_HAS_UPLOAD_ACCOUNTING - Success")
endfunction()
check_libtorrent_upload_accounting()
