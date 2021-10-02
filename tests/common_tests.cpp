#include "tests_common.h"

#include <server_lib/asserts.h>
#include <server_lib/emergency_helper.h>
#include <server_lib/logging_helper.h>
#include <server_lib/observer.h>

#include <boost/filesystem.hpp>

namespace server_lib {
namespace tests {

    namespace bf = boost::filesystem;

#if !defined(SERVER_LIB_PLATFORM_WINDOWS)
    namespace {
        void test_stack_anonymous_namespace(const bf::path& p)
        {
            BOOST_REQUIRE_NO_THROW(emergency_helper::save_raw_dump_s(p.generic_string().c_str()));
        }
    } // namespace
#endif

    BOOST_AUTO_TEST_SUITE(common_tests)

#if !defined(NDEBUG) && defined(SERVER_LIB_PLATFORM_WINDOWS)
    BOOST_AUTO_TEST_CASE(memory_leaks_base_check)
    {
        print_current_test_name();

        // memory leaks detection
    }
#endif

    BOOST_AUTO_TEST_CASE(assert_check)
    {
        print_current_test_name();

        auto test_true = [] {
            SRV_ASSERT(99999 > 100, "Test positive condition");
        };

        auto test_false = []() {
            SRV_ASSERT(100 > 99999, "Test negative condition");
        };

        BOOST_REQUIRE_NO_THROW(test_true());
        BOOST_CHECK_THROW(test_false(), std::logic_error);
    }

#if !defined(SERVER_LIB_PLATFORM_WINDOWS)
    BOOST_AUTO_TEST_CASE(stack_check)
    {
        print_current_test_name();

        auto dump_path = bf::temp_directory_path();
        dump_path /= bf::unique_path();

        test_stack_anonymous_namespace(dump_path);
        BOOST_REQUIRE(bf::exists(dump_path));

        auto stack = emergency_helper::load_dump(dump_path.generic_string().c_str(), true);

        DUMP_STR(stack);

        BOOST_REQUIRE(!bf::exists(dump_path));

        BOOST_REQUIRE_GT(stack.size(), 0u);
    }
#endif

    class log_fixture
    {
    public:
        log_fixture()
        {
            server_lib::logger::instance().set_level_filter(0);
        }
        ~log_fixture()
        {
            server_lib::logger::instance().set_level_filter(0);
        }
    };

    BOOST_FIXTURE_TEST_CASE(log_filter_check, log_fixture)
    {
        int test = 0;

        auto test_logs = [&]() {
            LOG_TRACE(test);
            LOG_DEBUG(test);
            LOG_INFO(test);
            LOG_WARN(test);
            LOG_ERROR(test);
            LOG_FATAL(test);
            ++test;
        };

        BOOST_REQUIRE_THROW(server_lib::logger::instance().set_level_filter(0xffff), std::logic_error);
        BOOST_REQUIRE_THROW(server_lib::logger::instance().set_level_filter(-1), std::logic_error);
        BOOST_REQUIRE_THROW(server_lib::logger::instance().set_level_filter(-0xffff), std::logic_error);

        server_lib::logger::instance().set_level_filter(0); //remove filter
        test_logs();
        server_lib::logger::instance().set_level_filter(); //default filter = 0x10
        test_logs();
        server_lib::logger::instance().set_level_filter(0x10 + 0x8);
        test_logs();
        server_lib::logger::instance().set_level_filter(0x10 + 0x8 + 0x4);
        test_logs();
        server_lib::logger::instance().set_level_filter(0x10 + 0x8 + 0x4 + 0x2);
        test_logs();
        server_lib::logger::instance().set_level_filter(0x10 + 0x8 + 0x4 + 0x2 + 0x1);
        test_logs();
    }

    struct test_sink_i
    {
        virtual ~test_sink_i() = default;

        virtual void on_test1() = 0;
        virtual void on_test2(int) = 0;
        virtual void on_test3(const std::string&) = 0;
        virtual void on_test4(int, const std::string&) = 0;
    };

    class test_observer_impl : public test_sink_i
    {
    public:
        test_observer_impl() = default;
        ~test_observer_impl() override {}

        bool expect_on_test1(bool expect = true)
        {
            if (expect)
            {
                return _on_test1;
            }
            else
            {
                return !_on_test1;
            }
        }

        bool expect_on_test2(const int value, bool expect = true)
        {
            if (expect)
            {
                if (!std::get<0>(_on_test2))
                    return false;
                return std::get<1>(_on_test2) == value;
            }
            else
            {
                return !std::get<0>(_on_test2);
            }
        }

        bool expect_on_test3(const std::string& value, bool expect = true)
        {
            if (expect)
            {
                if (!std::get<0>(_on_test3))
                    return false;
                return std::get<1>(_on_test3) == value;
            }
            else
            {
                return !std::get<0>(_on_test3);
            }
        }

        bool expect_on_test4(const int value1, const std::string& value2, bool expect = true)
        {
            if (expect)
            {
                if (!std::get<0>(_on_test4))
                    return false;
                if (std::get<1>(_on_test4) != value1)
                    return false;
                if (std::get<2>(_on_test4) != value2)
                    return false;
                return true;
            }
            else
            {
                return !std::get<0>(_on_test4);
            }
        }

        void on_test1() override
        {
            LOG_TRACE(__FUNCTION__);
            _on_test1 = true;
        }
        void on_test2(int value) override
        {
            LOG_TRACE(__FUNCTION__ << ", " << value);
            _on_test2 = std::make_tuple(true, value);
        }
        void on_test3(const std::string& value) override
        {
            LOG_TRACE(__FUNCTION__ << ", " << value);
            _on_test3 = std::make_tuple(true, value);
        }
        void on_test4(int value1, const std::string& value2) override
        {
            LOG_TRACE(__FUNCTION__ << ", " << value1 << ", " << value2);
            _on_test4 = std::make_tuple(true, value1, value2);
        }

        void reset()
        {
            _on_test1 = false;
            _on_test2 = std::make_tuple(false, 0);
            _on_test3 = std::make_tuple(false, std::string {});
            _on_test4 = std::make_tuple(false, 0, std::string {});
        }

    private:
        bool _on_test1 = false;
        std::tuple<bool, int> _on_test2 = std::make_tuple(false, 0);
        std::tuple<bool, std::string> _on_test3 = std::make_tuple(false, std::string {});
        std::tuple<bool, int, std::string> _on_test4 = std::make_tuple(false, 0, std::string {});
    };

    struct test_wrong_sink_i
    {
        virtual ~test_wrong_sink_i() = default;

        virtual void on_any_test() = 0;
    };

    BOOST_AUTO_TEST_CASE(observer_check)
    {
        server_lib::observable<test_sink_i> testing_observable;

        const int test_int_value = 12;
        const std::string test_str_value = "test";

        {
            test_observer_impl observer1;

            testing_observable.subscribe(observer1);

#if 0 //check for compilation errors (std::bind protection)
            testing_observable.notify(&test_wrong_sink_i::on_any_test);
            testing_observable.notify(&test_wrong_sink_i::on_any_test, 111);
            testing_observable.notify(&test_sink_i::on_test1, 111);
            testing_observable.notify(&test_sink_i::on_test2, 111, test_int_value);
            testing_observable.notify(&test_sink_i::on_test2, 111, test_str_value);

            //type protection
            auto wrong_callback = []() {
            };
            testing_observable.notify(wrong_callback);
#endif
            testing_observable.notify(&test_sink_i::on_test1);
            testing_observable.notify(&test_sink_i::on_test2, test_int_value);

            BOOST_REQUIRE(observer1.expect_on_test1());
            BOOST_REQUIRE(observer1.expect_on_test2(test_int_value));

            test_observer_impl observer2;

            testing_observable.subscribe(observer2);

            testing_observable.notify(&test_sink_i::on_test3, test_str_value);
            testing_observable.notify(&test_sink_i::on_test4, test_int_value, test_str_value);

            BOOST_REQUIRE(observer1.expect_on_test3(test_str_value));
            BOOST_REQUIRE(observer1.expect_on_test4(test_int_value, test_str_value));
            BOOST_REQUIRE(observer2.expect_on_test3(test_str_value));
            BOOST_REQUIRE(observer2.expect_on_test4(test_int_value, test_str_value));

            testing_observable.unsubscribe(observer1);
            testing_observable.unsubscribe(observer2);
        }

        {
            test_observer_impl observer1, observer2;

            testing_observable.subscribe(observer1);
            testing_observable.subscribe(observer2);

            testing_observable.notify(&test_sink_i::on_test3, test_str_value);
            testing_observable.notify(&test_sink_i::on_test1);

            BOOST_REQUIRE(observer1.expect_on_test3(test_str_value));
            BOOST_REQUIRE(observer2.expect_on_test3(test_str_value));
            BOOST_REQUIRE(observer1.expect_on_test1());
            BOOST_REQUIRE(observer2.expect_on_test1());
        }
    }

    BOOST_AUTO_TEST_CASE(observer_in_thread_check)
    {
        server_lib::observable<test_sink_i> testing_observable;

        const int test_int_value = 12;
        const std::string test_str_value = "test";

        server_lib::event_loop loop1;

        loop1.change_thread_name("!L1");
        server_lib::event_loop loop2;

        loop2.change_thread_name("!L2");

        test_observer_impl observer1;
        test_observer_impl observer2;
        test_observer_impl observer3;

        testing_observable.subscribe(observer1);
        testing_observable.subscribe(observer2, loop1);
        testing_observable.subscribe(observer3, loop2);

        testing_observable.notify(&test_sink_i::on_test1);
        testing_observable.notify(&test_sink_i::on_test2, test_int_value);
        testing_observable.notify(&test_sink_i::on_test3, test_str_value);
        testing_observable.notify(&test_sink_i::on_test4, test_int_value, test_str_value);

        BOOST_REQUIRE(observer1.expect_on_test1());
        BOOST_REQUIRE(observer2.expect_on_test1());
        BOOST_REQUIRE(observer3.expect_on_test1());
        BOOST_REQUIRE(observer1.expect_on_test2(test_int_value));
        BOOST_REQUIRE(observer2.expect_on_test2(test_int_value));
        BOOST_REQUIRE(observer3.expect_on_test2(test_int_value));
        BOOST_REQUIRE(observer1.expect_on_test3(test_str_value));
        BOOST_REQUIRE(observer2.expect_on_test3(test_str_value));
        BOOST_REQUIRE(observer3.expect_on_test3(test_str_value));
        BOOST_REQUIRE(observer1.expect_on_test4(test_int_value, test_str_value));
        BOOST_REQUIRE(observer2.expect_on_test4(test_int_value, test_str_value));
        BOOST_REQUIRE(observer3.expect_on_test4(test_int_value, test_str_value));

        loop1.start();
        loop2.start();

        observer1.reset();
        observer2.reset();
        observer3.reset();

        server_lib::event_loop loop0;

        loop0.change_thread_name("!L0");

        bool done = false;
        std::mutex done_cond_guard;
        std::condition_variable done_cond;

        loop0.start([&done, &done_cond_guard, &done_cond,
                     &testing_observable, test_int_value, test_str_value]() {
            testing_observable.notify(&test_sink_i::on_test1);
            testing_observable.notify(&test_sink_i::on_test2, test_int_value);
            testing_observable.notify(&test_sink_i::on_test3, test_str_value);
            testing_observable.notify(&test_sink_i::on_test4, test_int_value, test_str_value);

            //done test
            std::unique_lock<std::mutex> lck(done_cond_guard);
            done = true;
            done_cond.notify_one();
        });

        std::unique_lock<std::mutex> lck(done_cond_guard);
        if (!done)
        {
            done_cond.wait_for(lck, std::chrono::seconds(10), [&done]() {
                return done;
            });
        }
        BOOST_REQUIRE(done);

        //push events in queue
        auto waiting_for_loop_ready = []() {
            return true;
        };
        loop1.wait_async(false, waiting_for_loop_ready);
        loop2.wait_async(false, waiting_for_loop_ready);

        BOOST_REQUIRE(observer2.expect_on_test1());
        BOOST_REQUIRE(observer3.expect_on_test1());
        BOOST_REQUIRE(observer2.expect_on_test2(test_int_value));
        BOOST_REQUIRE(observer3.expect_on_test2(test_int_value));
        BOOST_REQUIRE(observer2.expect_on_test3(test_str_value));
        BOOST_REQUIRE(observer3.expect_on_test3(test_str_value));
        BOOST_REQUIRE(observer2.expect_on_test4(test_int_value, test_str_value));
        BOOST_REQUIRE(observer3.expect_on_test4(test_int_value, test_str_value));
    }

    struct test_renter_protection_sink
    {
        using observer_type = server_lib::observable<test_renter_protection_sink>;

        test_renter_protection_sink(observer_type& observable)
            : _observable(observable)
        {
        }

        void on_test_ok()
        {
            LOG_TRACE(__FUNCTION__);
        }

        void on_test_renter()
        {
            LOG_TRACE(__FUNCTION__);
            try
            {
                _observable.notify(&test_renter_protection_sink::on_test_renter);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR(e.what());
                throw;
            }
        }

    private:
        observer_type& _observable;
    };

    BOOST_AUTO_TEST_CASE(observer_renter_protection_check)
    {
        test_renter_protection_sink::observer_type testing_observable;

        test_renter_protection_sink observer { testing_observable };

        testing_observable.subscribe(observer);

        BOOST_REQUIRE_NO_THROW(testing_observable.notify(&test_renter_protection_sink::on_test_ok));
        BOOST_REQUIRE_THROW(testing_observable.notify(&test_renter_protection_sink::on_test_renter), std::logic_error);
    }

    BOOST_AUTO_TEST_CASE(wait_async_timeout_check)
    {
        server_lib::event_loop loop1;

        loop1.change_thread_name("!L1");
        loop1.start();

        BOOST_REQUIRE(loop1.wait_async(false, [] { return true; }));

        auto payload = [] {
            LOG_INFO("Payload start");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            LOG_INFO("Payload end");
            return true;
        };
        BOOST_REQUIRE(loop1.wait_async(false, payload));

        BOOST_REQUIRE(loop1.wait_async(false, payload, std::chrono::milliseconds(1500)));

        BOOST_REQUIRE(!loop1.wait_async(false, payload, std::chrono::milliseconds(500)));

        server_lib::event_loop loop2;

        loop2.change_thread_name("!L2");
        loop2.start();

        BOOST_REQUIRE(!loop2.wait_async(false, payload, std::chrono::milliseconds(500)));

        loop2.stop();
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace server_lib
