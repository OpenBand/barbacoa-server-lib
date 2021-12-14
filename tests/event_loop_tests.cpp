#include "tests_common.h"

#include <server_lib/logging_helper.h>
#include <server_lib/mt_event_loop.h>


namespace server_lib {
namespace tests {

    using namespace std::chrono_literals;

    BOOST_AUTO_TEST_SUITE(event_loop_tests)

    BOOST_AUTO_TEST_CASE(event_loop_check)
    {
        print_current_test_name();

        server_lib::event_loop loop;

        BOOST_REQUIRE_NO_THROW(loop.change_thread_name("L").start());
        BOOST_REQUIRE_NO_THROW(loop.stop());

        LOG_INFO("Next case");

        // Event loop can't stop itself (stop will emit deadlock)
        // it will be done by creator or external loop.

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        server_lib::event_loop external_stopper;
        BOOST_REQUIRE_NO_THROW(external_stopper.change_thread_name("LS").start());
        auto stop_action = [&]() {
            LOG_INFO("Action is starting");
            external_stopper.post([&]() {
                LOG_INFO("Action has started");

                BOOST_REQUIRE_NO_THROW(loop.stop());

                // Finish test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();

                LOG_INFO("Action has finished");
            });
        };
        BOOST_REQUIRE_NO_THROW(loop.on_start(stop_action).start());
        // Loop external_stopper is stopping by destructor

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(wait_async_result_check)
    {
        print_current_test_name();

        server_lib::event_loop loop1;

        loop1.change_thread_name("!L1");
        loop1.start();

        BOOST_REQUIRE(loop1.wait_result(false, [] { return true; }));

        auto payload = [] {
            LOG_INFO("Payload start");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            LOG_INFO("Payload end");
            return true;
        };
        BOOST_REQUIRE(loop1.wait_result(false, payload));

        BOOST_REQUIRE(loop1.wait_result(false, payload, std::chrono::milliseconds(1500)));

        // Check timeout
        BOOST_REQUIRE(!loop1.wait_result(false, payload, std::chrono::milliseconds(500)));

        server_lib::event_loop loop2;

        loop2.change_thread_name("!L2");
        loop2.start();

        // Check timeout
        BOOST_REQUIRE(!loop2.wait_result(false, payload, std::chrono::milliseconds(500)));

        loop2.stop();
    }

    class AnyResult
    {
    public:
        AnyResult(int val1 = 0, const std::string& val2 = {})
        {
            _val1 = val1;
            _val2 = val2;
        }
        AnyResult(const AnyResult& other)
        {
            *this = other;
        }
        AnyResult(AnyResult&& other)
        {
            _val1 = std::move(other._val1);
            _val2 = std::move(other._val2);
        }

        AnyResult& operator=(const AnyResult& other)
        {
            _val1 = other._val1;
            _val2 = other._val2;
            return *this;
        }

        friend bool operator==(const AnyResult& a, const AnyResult& b)
        {
            return a._val1 == b._val1 && a._val2 == b._val2;
        }

        int _val1 = 0;
        std::string _val2 = {};
    };

    BOOST_AUTO_TEST_CASE(wait_async_obj_result_check)
    {
        print_current_test_name();

        const int VAL1_FAIL = -1;
        const int VAL1_PAYLOAD_DONE = 12;
        const std::string VAL2_PAYLOAD_DONE = "Payload done";

        server_lib::event_loop loop1;

        loop1.change_thread_name("!L1");
        loop1.start();

        auto payload = [val1 = VAL1_PAYLOAD_DONE, val2 = VAL2_PAYLOAD_DONE]() -> AnyResult {
            LOG_INFO("Payload start");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            LOG_INFO("Payload end");
            return AnyResult(val1, val2);
        };
        BOOST_REQUIRE(loop1.wait_result(AnyResult(VAL1_FAIL), payload) == AnyResult(VAL1_PAYLOAD_DONE, VAL2_PAYLOAD_DONE));

        BOOST_REQUIRE(loop1.wait_result(AnyResult(VAL1_FAIL), payload, std::chrono::milliseconds(1500)) == AnyResult(VAL1_PAYLOAD_DONE, VAL2_PAYLOAD_DONE));

        // Check timeout
        BOOST_REQUIRE(loop1.wait_result(AnyResult(VAL1_FAIL), payload, std::chrono::milliseconds(500)) == AnyResult(VAL1_FAIL));

        server_lib::event_loop loop2;

        loop2.change_thread_name("!L2");
        loop2.start();

        // Check timeout
        BOOST_REQUIRE(loop2.wait_result(AnyResult(VAL1_FAIL), payload, std::chrono::milliseconds(500)) == AnyResult(VAL1_FAIL));

        loop2.stop();
    }

    BOOST_AUTO_TEST_CASE(wait_async_result_exception_check)
    {
        print_current_test_name();

        auto payload = [] {
            LOG_INFO("Payload start");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            throw std::logic_error("Async exception");
            return true;
        };

        server_lib::event_loop loop1;

        loop1.change_thread_name("!L1");
        loop1.start();

        BOOST_REQUIRE_THROW(loop1.wait_result(false, payload), std::logic_error);

        BOOST_REQUIRE_THROW(loop1.wait_result(false, payload, std::chrono::milliseconds(1500)), std::logic_error);

        // Check timeout
        BOOST_REQUIRE_NO_THROW(loop1.wait_result(false, payload, std::chrono::milliseconds(500)));

        server_lib::event_loop loop2;

        loop2.change_thread_name("!L2");
        loop2.start();

        // Check timeout
        BOOST_REQUIRE_NO_THROW(loop2.wait_result(false, payload, std::chrono::milliseconds(500)));

        loop2.stop();
    }

    BOOST_AUTO_TEST_CASE(wait_async_check)
    {
        print_current_test_name();

        server_lib::event_loop loop1;

        loop1.change_thread_name("!L1");
        loop1.start();

        auto payload = []() -> void {
            LOG_INFO("Payload start");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            LOG_INFO("Payload end");
        };
        BOOST_REQUIRE_NO_THROW(loop1.wait(payload));

        BOOST_REQUIRE(loop1.wait(payload, std::chrono::milliseconds(1500)));

        // Check timeout
        BOOST_REQUIRE(!loop1.wait(payload, std::chrono::milliseconds(500)));

        server_lib::event_loop loop2;

        loop2.change_thread_name("!L2");
        loop2.start();

        // Check timeout
        BOOST_REQUIRE(!loop2.wait(payload, std::chrono::milliseconds(500)));

        loop2.stop();
    }

    BOOST_AUTO_TEST_CASE(wait_async_exception_check)
    {
        print_current_test_name();

        auto payload = []() -> void {
            LOG_INFO("Payload start");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            throw std::logic_error("Async exception");
        };

        server_lib::event_loop loop1;

        loop1.change_thread_name("!L1");
        loop1.start();

        BOOST_REQUIRE_THROW(loop1.wait(payload), std::logic_error);

        BOOST_REQUIRE_THROW(loop1.wait(payload, std::chrono::milliseconds(1500)), std::logic_error);

        // Check timeout
        BOOST_REQUIRE_NO_THROW(loop1.wait(payload, std::chrono::milliseconds(500)));

        server_lib::event_loop loop2;

        loop2.change_thread_name("!L2");
        loop2.start();

        // Check timeout
        BOOST_REQUIRE_NO_THROW(loop2.wait(payload, std::chrono::milliseconds(500)));

        loop2.stop();
    }

    BOOST_AUTO_TEST_CASE(mt_event_loop_check)
    {
        print_current_test_name();

        server_lib::mt_event_loop loop(5);

        BOOST_REQUIRE_NO_THROW(loop.change_thread_name("L").start());
        BOOST_REQUIRE_NO_THROW(loop.stop());

        LOG_INFO("Next case");

        // Event loop can't stop itself (stop will emit deadlock)
        // it will be done by creator or external loop

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        server_lib::event_loop external_stopper;
        BOOST_REQUIRE_NO_THROW(external_stopper.change_thread_name("LS").start());
        auto stop_action = [&]() {
            LOG_INFO("Stop action is starting");
            external_stopper.post([&]() {
                LOG_INFO("Stop action has started");

                BOOST_REQUIRE_NO_THROW(loop.stop());

                // Finish test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();

                LOG_INFO("Stop action has finished");
            });
        };
        BOOST_REQUIRE_NO_THROW(loop.on_start(stop_action).start());
        // Loop external_stopper is stopping by destructor

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(mt_event_loop_post_distribution_check)
    {
        print_current_test_name();

        server_lib::mt_event_loop loop(5);

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        server_lib::event_loop external_stopper;

        auto stop_action = [&]() {
            external_stopper.post([&]() {
                BOOST_REQUIRE_NO_THROW(loop.stop());

                // Finish test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();
            });
        };

        std::atomic<size_t> waiting_posts;

        auto test_action = [&] {
            LOG_INFO("Test action");

            std::atomic_fetch_sub<size_t>(&waiting_posts, 1);
            if (!waiting_posts.load())
                stop_action();
        };

        waiting_posts = 20; // total posts

        for (size_t ci = 0; ci < 10; ++ci) // first half
        {
            loop.post(test_action);
        }

        BOOST_REQUIRE_NO_THROW(external_stopper.change_thread_name("LS").start());
        BOOST_REQUIRE_NO_THROW(loop.change_thread_name("L").on_start([&]() {
                                                               for (size_t ci = 0; ci < 10; ++ci) //second half
                                                               {
                                                                   loop.post(test_action);
                                                               }
                                                           })
                                   .start());
        // Loop external_stopper is stopping by destructor

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(mt_event_loop_degenerate_case_check)
    {
        print_current_test_name();

        server_lib::mt_event_loop loop(1);

        BOOST_REQUIRE_NO_THROW(loop.change_thread_name("L").start());
        BOOST_REQUIRE_NO_THROW(loop.stop());

        LOG_INFO("Next case");

        // Event loop can't stop itself (stop will emit deadlock)
        // it will be done by creator or external loop

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        server_lib::event_loop external_stopper;
        BOOST_REQUIRE_NO_THROW(external_stopper.change_thread_name("LS").start());
        auto stop_action = [&]() {
            LOG_INFO("Action is starting");
            external_stopper.post([&]() {
                LOG_INFO("Action has started");

                BOOST_REQUIRE_NO_THROW(loop.stop());

                // Finish test
                std::unique_lock<std::mutex> lck(done_test_cond_guard);
                done_test = true;
                done_test_cond.notify_one();

                LOG_INFO("Action has finished");
            });
        };
        BOOST_REQUIRE_NO_THROW(loop.on_start(stop_action).start());
        // Loop external_stopper is stopping by destructor

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace server_lib
