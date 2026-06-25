// Pure sync-over-async primitives: awaitResult / parseJsonResponse.

#include <logos_test.h>
#include <plugin.h>

#include <chrono>
#include <future>
#include <thread>

using namespace std::chrono;

LOGOS_TEST(await_result_returns_ready_value) {
    SyncPromise p;
    auto f = p.get_future();
    p.set_value({true, "done", {}, nullptr});

    auto r = awaitResult(f, 1000);
    LOGOS_ASSERT_TRUE(r.ok);
    LOGOS_ASSERT_TRUE(r.message == "done");
}

LOGOS_TEST(await_result_waits_for_late_value) {
    SyncPromise p;
    auto f = p.get_future();
    std::thread setter([&p] {
        std::this_thread::sleep_for(milliseconds(20));
        p.set_value({true, "late", {}, nullptr});
    });

    auto r = awaitResult(f, 2000);
    setter.join();
    LOGOS_ASSERT_TRUE(r.ok);
    LOGOS_ASSERT_TRUE(r.message == "late");
}

// awaitResult bounds the wait by its own timeoutMs and nothing else. This is the
// timeout-mismatch pin: callSyncWith hardcodes the default timeout, so a caller's
// connect timeout never widens (or shortens) the wrapper's actual wait.
LOGOS_TEST(await_result_times_out_at_its_own_timeout) {
    SyncPromise p;  // never satisfied
    auto f = p.get_future();

    auto start = steady_clock::now();
    auto r = awaitResult(f, 50);
    auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start).count();

    LOGOS_ASSERT_FALSE(r.ok);
    LOGOS_ASSERT_TRUE(r.message == "timeout");
    // Returns at roughly the requested timeout, not later.
    LOGOS_ASSERT_GE(elapsed, 40);
    LOGOS_ASSERT_LT(elapsed, 2000);
}

LOGOS_TEST(parse_json_response_valid) {
    auto r = parseJsonResponse(R"({"peerId": "16Uiu2abc", "addrs": []})", "peerInfo");
    LOGOS_ASSERT_TRUE(r.success);
    LOGOS_ASSERT_TRUE(r.value["peerId"].get<std::string>() == "16Uiu2abc");
    LOGOS_ASSERT_TRUE(r.error.empty());
}

LOGOS_TEST(parse_json_response_invalid_fails_without_throwing) {
    auto r = parseJsonResponse("{ truncated", "peerInfo");
    LOGOS_ASSERT_FALSE(r.success);
    LOGOS_ASSERT_TRUE(r.error == "peerInfo: invalid JSON");
}

LOGOS_TEST(parse_json_response_empty_fails) {
    auto r = parseJsonResponse("", "peerInfo");
    LOGOS_ASSERT_FALSE(r.success);
    LOGOS_ASSERT_TRUE(r.error == "peerInfo: invalid JSON");
}
