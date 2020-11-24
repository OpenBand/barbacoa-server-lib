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
            BOOST_REQUIRE_NO_THROW(emergency_helper::save_dump(p.generic_string().c_str()));
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

        void expect_on_test1(bool expect = true)
        {
            if (expect)
            {
                BOOST_REQUIRE(_on_test1);
            }
            else
            {
                BOOST_REQUIRE(!_on_test1);
            }
        }

        void expect_on_test2(const int value, bool expect = true)
        {
            if (expect)
            {
                BOOST_REQUIRE(std::get<0>(_on_test2));
                BOOST_REQUIRE_EQUAL(std::get<1>(_on_test2), value);
            }
            else
            {
                BOOST_REQUIRE(!std::get<0>(_on_test2));
            }
        }

        void expect_on_test3(const std::string& value, bool expect = true)
        {
            if (expect)
            {
                BOOST_REQUIRE(std::get<0>(_on_test3));
                BOOST_REQUIRE_EQUAL(std::get<1>(_on_test3), value);
            }
            else
            {
                BOOST_REQUIRE(!std::get<0>(_on_test3));
            }
        }

        void expect_on_test4(const int value1, const std::string& value2, bool expect = true)
        {
            if (expect)
            {
                BOOST_REQUIRE(std::get<0>(_on_test4));
                BOOST_REQUIRE_EQUAL(std::get<1>(_on_test4), value1);
                BOOST_REQUIRE_EQUAL(std::get<2>(_on_test4), value2);
            }
            else
            {
                BOOST_REQUIRE(!std::get<0>(_on_test4));
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

    private:
        bool _on_test1 = false;
        std::tuple<bool, int> _on_test2 = std::make_tuple(false, 0);
        std::tuple<bool, std::string> _on_test3 = std::make_tuple(false, std::string {});
        std::tuple<bool, int, std::string> _on_test4 = std::make_tuple(false, 0, std::string {});
    };

    BOOST_AUTO_TEST_CASE(observer_check)
    {
        server_lib::observable<test_sink_i> testing_observable;

        const int test_int_value = 12;
        const std::string test_str_value = "test";

        {
            test_observer_impl observer1;

            testing_observable.subscribe(observer1);

            testing_observable.notify(&test_sink_i::on_test1);
            testing_observable.notify(&test_sink_i::on_test2, test_int_value);

            observer1.expect_on_test1();
            observer1.expect_on_test2(test_int_value);

            test_observer_impl observer2;

            testing_observable.subscribe(observer2);

            testing_observable.notify(&test_sink_i::on_test3, test_str_value);
            testing_observable.notify(&test_sink_i::on_test4, test_int_value, test_str_value);

            observer1.expect_on_test3(test_str_value);
            observer1.expect_on_test4(test_int_value, test_str_value);
            observer2.expect_on_test3(test_str_value);
            observer2.expect_on_test4(test_int_value, test_str_value);

            testing_observable.unsubscribe(observer1);
            testing_observable.unsubscribe(observer2);
        }

        {
            test_observer_impl observer1, observer2;

            testing_observable.subscribe(observer1);
            testing_observable.subscribe(observer2);

            testing_observable.notify(&test_sink_i::on_test3, test_str_value);
            testing_observable.notify(&test_sink_i::on_test1);

            observer1.expect_on_test3(test_str_value);
            observer2.expect_on_test3(test_str_value);
            observer1.expect_on_test1();
            observer2.expect_on_test1();
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

        observer1.expect_on_test1();
        observer2.expect_on_test1(false);
        observer3.expect_on_test1(false);
        observer1.expect_on_test2(test_int_value);
        observer2.expect_on_test2(test_int_value, false);
        observer3.expect_on_test2(test_int_value, false);
        observer1.expect_on_test3(test_str_value);
        observer2.expect_on_test3(test_str_value, false);
        observer3.expect_on_test3(test_str_value, false);
        observer1.expect_on_test4(test_int_value, test_str_value);
        observer2.expect_on_test4(test_int_value, test_str_value, false);
        observer3.expect_on_test4(test_int_value, test_str_value, false);

        loop1.start();
        loop2.start();

        auto wait_ = []() {
            return true;
        };
        loop1.wait_async(false, wait_);
        loop2.wait_async(false, wait_);

        observer2.expect_on_test1();
        observer3.expect_on_test1();
        observer2.expect_on_test2(test_int_value);
        observer3.expect_on_test2(test_int_value);
        observer2.expect_on_test3(test_str_value);
        observer3.expect_on_test3(test_str_value);
        observer2.expect_on_test4(test_int_value, test_str_value);
        observer3.expect_on_test4(test_int_value, test_str_value);
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace server_lib
