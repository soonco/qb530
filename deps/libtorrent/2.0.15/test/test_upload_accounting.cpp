// Upload accounting regression tests. See LICENSE for the BSD license.
#include "test.hpp"
#include "setup_transfer.hpp"
#include "udp_tracker.hpp"
#include "libtorrent/version.hpp"
#include "libtorrent/session.hpp"
#include "libtorrent/session_params.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/read_resume_data.hpp"
#include "libtorrent/write_resume_data.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/entry.hpp"
#include "libtorrent/aux_/session_impl.hpp"
#if LIBTORRENT_VERSION_NUM >= 20100
#include "libtorrent/aux_/torrent.hpp"
#include "libtorrent/aux_/random.hpp"
#else
#include "libtorrent/torrent.hpp"
#include "libtorrent/random.hpp"
#endif
#include <future>
#include <random>
#include <thread>

using namespace lt;

namespace {
settings_pack isolated_settings()
{
    settings_pack p;
    p.set_str(settings_pack::listen_interfaces, "");
    p.set_bool(settings_pack::enable_dht, false);
    p.set_bool(settings_pack::enable_lsd, false);
    p.set_bool(settings_pack::enable_upnp, false);
    p.set_bool(settings_pack::enable_natpmp, false);
    p.set_bool(settings_pack::fake_upload_ratio_randomization, false);
    return p;
}

template <typename F>
void with_torrent(F f)
{
    lt::session ses(isolated_settings());
    add_torrent_params p;
    p.info_hashes.v1 = sha1_hash("abcdefghijklmnopqrst");
    p.save_path = ".";
    p.flags = {};
    auto const h = ses.add_torrent(std::move(p));
    auto impl = ses.native_handle();
    auto done = std::make_shared<std::promise<void>>();
    auto result = done->get_future();
    post(impl->get_context(), [h, impl, done, f] {
        try
        {
            f(h.native_handle(), impl);
            done->set_value();
        }
        catch (...) { done->set_exception(std::current_exception()); }
    });
    TEST_CHECK(result.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
    result.get();
}
}

TORRENT_TEST(upload_accounting_lifecycle)
{
    with_torrent([](auto t, auto ses) {
        auto ratio = [&](int r) {
            settings_pack p;
            p.set_int(settings_pack::fake_upload_ratio, r);
            ses->apply_settings_pack_impl(p);
        };
        auto check = [&](std::int64_t real, std::int64_t reported) {
            torrent_status s;
            t->status(&s, {});
            TEST_EQUAL(s.all_time_real_uploaded, real);
            TEST_EQUAL(s.all_time_upload, reported);
        };
        ratio(2);
        t->sent_bytes(100, 17);
        t->second_tick(1000);
        check(100, 200);
        t->announce_with_tracker();
        t->announce_with_tracker();
        t->second_tick(1000);
        check(100, 200);
        ratio(5);
        t->sent_bytes(200, 23);
        t->second_tick(1000);
        check(300, 1200);
        ratio(1);
        t->sent_bytes(50, 0);
        t->second_tick(1000);
        check(350, 1250);
#if LIBTORRENT_VERSION_NUM >= 20100
        TEST_EQUAL(t->build_tracker_request(event_t::none).uploaded, 1250);
#endif
        t->stop_announcing();
        t->start_announcing();
        check(350, 1250);
#if LIBTORRENT_VERSION_NUM >= 20100
        TEST_EQUAL(t->build_tracker_request(event_t::none).uploaded, 0);
#endif
        t->sent_bytes(10, 0);
        t->second_tick(1000);
        check(360, 1260);
        add_torrent_params resume;
        t->write_resume_data({}, resume);
        auto restored = read_resume_data(write_resume_data_buf(resume));
        TEST_EQUAL(restored.total_real_uploaded, 360);
        TEST_EQUAL(restored.total_uploaded, 1260);

        // Bytes sent immediately before pause must survive the next stats tick.
        t->sent_bytes(7, 0);
        t->pause();
        t->second_tick(1000);
        check(367, 1267);
        t->resume();
        t->second_tick(1000);
        check(367, 1267);

        ratio(5);
        t->sent_bytes(1000000000, 0);
        t->second_tick(1000);
        check(1000000367LL, 5000001267LL);
    });
}

TORRENT_TEST(upload_accounting_randomization)
{
    with_torrent([](auto t, auto ses) {
        settings_pack p;
        p.set_int(settings_pack::fake_upload_ratio, 5);
        p.set_bool(settings_pack::fake_upload_ratio_randomization, true);
        ses->apply_settings_pack_impl(p);
        aux::random_engine().seed(12345);
        auto expected_rng = aux::random_engine();
        std::int64_t reported = 0;
        for (int bytes : {100, 0, 200, 50})
        {
            reported += std::int64_t(bytes) * std::uniform_int_distribution<int>(1, 5)(expected_rng);
            t->sent_bytes(bytes, 0);
            t->second_tick(1000);
        }
        torrent_status s;
        t->status(&s, {});
        TEST_EQUAL(s.all_time_real_uploaded, 350);
        TEST_EQUAL(s.all_time_upload, reported);
        TEST_CHECK(aux::random_engine() == expected_rng);
        p.set_int(settings_pack::fake_upload_ratio, 0);
        ses->apply_settings_pack_impl(p);
        t->sent_bytes(11, 0);
        t->second_tick(1000);
        t->status(&s, {});
        TEST_EQUAL(s.all_time_upload, reported + 11);
        TEST_EQUAL(s.all_time_real_uploaded, 361);
        TEST_CHECK(aux::random_engine() == expected_rng);
    });
}

TORRENT_TEST(upload_accounting_resume_compatibility)
{
    add_torrent_params p;
    p.info_hashes.v1 = sha1_hash("abcdefghijklmnopqrst");
    p.total_uploaded = 5000000000LL;
    p.total_real_uploaded = 1000000000LL;
    auto rd = write_resume_data(p);
    auto encode = [](entry const& e) {
        std::vector<char> bytes;
        bencode(std::back_inserter(bytes), e);
        return bytes;
    };
    auto restored = read_resume_data(encode(rd));
    TEST_EQUAL(restored.total_uploaded, 5000000000LL);
    TEST_EQUAL(restored.total_real_uploaded, 1000000000LL);
    rd.dict().erase("total_real_uploaded");
    restored = read_resume_data(encode(rd));
    TEST_EQUAL(restored.total_real_uploaded, 5000000000LL);
    TEST_CHECK(rd.dict().count("total_tracker_uploaded") == 0);
    TEST_EQUAL(fake_upload_abi_v1(), LIBTORRENT_FAKE_UPLOAD_ABI);
    TEST_EQUAL(setting_by_name("fake_upload_ratio"), settings_pack::fake_upload_ratio);
    TEST_EQUAL(setting_by_name("fake_upload_ratio_randomization"), settings_pack::fake_upload_ratio_randomization);
}

TORRENT_TEST(upload_accounting_tracker_snapshots)
{
    int const http_port = start_web_server(false, false, true, -1);
    int const udp_port = start_udp_tracker(address_v4::loopback());
    auto pack = isolated_settings();
    pack.set_str(settings_pack::listen_interfaces, "127.0.0.1:0");
    pack.set_bool(settings_pack::announce_to_all_trackers, true);
    pack.set_bool(settings_pack::announce_to_all_tiers, true);
    pack.set_int(settings_pack::alert_mask, static_cast<int>(static_cast<std::uint32_t>(alert_category::tracker)));
    pack.set_int(settings_pack::stop_tracker_timeout, 1);
    pack.set_int(settings_pack::fake_upload_ratio, 5);
    lt::session ses(pack);
    add_torrent_params p;
    p.info_hashes.v1 = sha1_hash("abcdefghijklmnopqrst");
    p.info_hashes.v2 = sha256_hash("123456abcdefghijklmnopqrstuvwxyz");
    p.save_path = ".";
    p.flags = {};
    p.trackers = {"http://127.0.0.1:" + std::to_string(http_port) + "/announce"
        , "udp://127.0.0.1:" + std::to_string(udp_port) + "/announce"};
    auto h = ses.add_torrent(std::move(p));
    for (int i = 0; i < 100 && ses.listen_port() == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    TEST_CHECK(ses.listen_port() != 0);

    int old_cycle = 0;
    int new_cycle = 0;
    auto collect = [&](int expected_replies) {
        int replies = 0;
        for (int i = 0; i < 100 && replies < expected_replies; ++i)
        {
            ses.wait_for_alert(std::chrono::milliseconds(100));
            std::vector<alert*> alerts;
            ses.pop_alerts(&alerts);
            for (auto a : alerts)
            {
                if (alert_cast<tracker_reply_alert>(a)) ++replies;
                auto announce = alert_cast<tracker_announce_alert>(a);
                if (!announce || announce->event != event_t::stopped) continue;
                if (announce->total_real_uploaded == 107)
                {
                    TEST_EQUAL(announce->total_uploaded, 535);
                    ++old_cycle;
                }
                else if (announce->total_real_uploaded == 10)
                {
                    TEST_EQUAL(announce->total_uploaded, 50);
                    ++new_cycle;
                }
            }
        }
        TEST_CHECK(replies >= expected_replies);
    };
    collect(4);
    auto done = std::make_shared<std::promise<void>>();
    auto result = done->get_future();
    auto impl = ses.native_handle();
    post(impl->get_context(), [h, done] {
        auto t = h.native_handle();
        t->sent_bytes(100, 21);
        t->second_tick(1000);
        t->sent_bytes(7, 3);
        // Stop must include the unfinished batch in both request and alert.
        t->stop_announcing();
        done->set_value();
    });
    result.get();
    collect(4);
    done = std::make_shared<std::promise<void>>();
    result = done->get_future();
    post(impl->get_context(), [h, done] {
        h.native_handle()->start_announcing();
        done->set_value();
    });
    result.get();
    // Successful started responses are required by upstream before stopped.
    collect(4);
    done = std::make_shared<std::promise<void>>();
    result = done->get_future();
    post(impl->get_context(), [h, done] {
        auto t = h.native_handle();
        t->sent_bytes(10, 0);
        t->second_tick(1000);
        t->stop_announcing();
        done->set_value();
    });
    result.get();
    collect(4);
    // HTTP and UDP, each for v1 and v2; requests must not multiply again.
    TEST_EQUAL(old_cycle, 4);
    TEST_EQUAL(new_cycle, 4);
    stop_web_server();
    stop_udp_tracker();
}

TORRENT_TEST(upload_accounting_save_tail)
{
    with_torrent([](auto t, auto ses) {
        settings_pack p;
        p.set_int(settings_pack::fake_upload_ratio, 3);
        ses->apply_settings_pack_impl(p);
        t->sent_bytes(13, 5);
        auto saved = t->get_resume_data({});
        TEST_EQUAL(saved.total_real_uploaded, 13);
        TEST_EQUAL(saved.total_uploaded, 39);
        t->second_tick(1000);
        saved = t->get_resume_data({});
        TEST_EQUAL(saved.total_uploaded, 39);
        t->sent_bytes(2, 0);
        t->save_resume_data({});
        saved = t->get_resume_data({});
        TEST_EQUAL(saved.total_real_uploaded, 15);
        TEST_EQUAL(saved.total_uploaded, 45);
        t->sent_bytes(3, 0);
        t->abort();
        torrent_status s;
        t->status(&s, {});
        TEST_EQUAL(s.all_time_real_uploaded, 18);
        TEST_EQUAL(s.all_time_upload, 54);
    });
}


