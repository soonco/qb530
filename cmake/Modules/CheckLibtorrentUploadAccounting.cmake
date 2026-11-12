include(CheckCXXSourceCompiles)
include(CMakePushCheckState)
function(check_libtorrent_upload_accounting)
    set(CMAKE_CXX_STANDARD 20)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_LIBRARIES LibtorrentRasterbar::torrent-rasterbar OpenSSL::Crypto)
    # Recheck if the user selects another installation in an existing build tree.
    unset(QBT_HAS_UPLOAD_ACCOUNTING CACHE)
    check_cxx_source_compiles("
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
    " QBT_HAS_UPLOAD_ACCOUNTING)
    cmake_pop_check_state()
    if (NOT QBT_HAS_UPLOAD_ACCOUNTING)
        message(FATAL_ERROR
            "This build requires the custom libtorrent from deps/libtorrent/2.0.15 or 2.1.2. "
            "Build and install it first, then set LibtorrentRasterbar_DIR to that installation. "
            "Official libraries with the same version are not ABI compatible.")
    endif()
endfunction()
check_libtorrent_upload_accounting()
