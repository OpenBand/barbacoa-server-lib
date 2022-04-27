#include "tests_common.h"

#include <server_lib/logging_helper.h>
#include <server_lib/observer.h>
#include <server_lib/simple_observer.h>


namespace server_lib {
namespace tests {

    BOOST_AUTO_TEST_SUITE(observer_tests)

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
        print_current_test_name();

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

            // Type protection
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
        print_current_test_name();

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

        loop0.on_start([&done, &done_cond_guard, &done_cond,
                        &testing_observable, test_int_value, test_str_value]() {
                 testing_observable.notify(&test_sink_i::on_test1);
                 testing_observable.notify(&test_sink_i::on_test2, test_int_value);
                 testing_observable.notify(&test_sink_i::on_test3, test_str_value);
                 testing_observable.notify(&test_sink_i::on_test4, test_int_value, test_str_value);

                 // Finish test
                 std::unique_lock<std::mutex> lck(done_cond_guard);
                 done = true;
                 done_cond.notify_one();
             })
            .start();

        std::unique_lock<std::mutex> lck(done_cond_guard);
        if (!done)
        {
            done_cond.wait_for(lck, std::chrono::seconds(10), [&done]() {
                return done;
            });
        }
        BOOST_REQUIRE(done);

        // Push events in queue
        auto waiting_for_loop_ready = []() {
            return true;
        };
        loop1.wait_result(false, waiting_for_loop_ready);
        loop2.wait_result(false, waiting_for_loop_ready);

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
        print_current_test_name();

        test_renter_protection_sink::observer_type testing_observable;

        test_renter_protection_sink observer { testing_observable };

        testing_observable.subscribe(observer);

        BOOST_REQUIRE_NO_THROW(testing_observable.notify(&test_renter_protection_sink::on_test_ok));
        BOOST_REQUIRE_THROW(testing_observable.notify(&test_renter_protection_sink::on_test_renter), std::logic_error);
    }

    BOOST_AUTO_TEST_CASE(simple_observer_void_check)
    {
        print_current_test_name();

        using callback_type = std::function<void(void)>;
        using observer_type = simple_observable<callback_type>;

        observer_type observer;

        bool callback_invoked = false;
        auto callback = [&]() {
            callback_invoked = true;
        };

        observer.subscribe(callback);

        observer.notify();

        BOOST_REQUIRE(callback_invoked);
    }

    BOOST_AUTO_TEST_CASE(simple_observer_with_args_check)
    {
        print_current_test_name();

        using callback_type = std::function<void(int, int)>;
        using observer_type = simple_observable<callback_type>;

        observer_type observer;

        const int arg1_val(1);
        const int arg2_val(2);

        bool callback_invoked = false;
        auto callback = [&](int arg1, int arg2) {
            BOOST_REQUIRE_EQUAL(arg1, arg1_val);
            BOOST_REQUIRE_EQUAL(arg2, arg2_val);
            callback_invoked = true;
        };

        observer.subscribe(callback);

        observer.notify(arg1_val, arg2_val);

        BOOST_REQUIRE(callback_invoked);
    }

    BOOST_AUTO_TEST_CASE(simple_observer_with_ref_args_check)
    {
        print_current_test_name();

        using callback_type = std::function<void(std::string&, std::string&)>;
        using observer_type = simple_observable<callback_type>;

        observer_type observer;

        const std::string arg1_val("ref string A");
        const std::string arg2_val("ref string B");

        bool callback_invoked = false;
        auto callback = [&](std::string& arg1, std::string& arg2) {
            arg1 = arg1_val;
            arg2 = arg2_val;
            callback_invoked = true;
        };

        observer.subscribe(callback);

        std::string arg1;
        std::string arg2;
        observer.notify_ref(arg1, arg2);

        BOOST_REQUIRE(callback_invoked);
        BOOST_REQUIRE_EQUAL(arg1, arg1_val);
        BOOST_REQUIRE_EQUAL(arg2, arg2_val);
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace server_lib
