#include "tests_common.h"

#include <server_lib/asserts.h>
#include <server_lib/emergency_helper.h>
#include <server_lib/logging_helper.h>

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

    BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace server_lib
