#pragma once

#include "tests_common.h"

#include <cstdint>
#include <string>
#include <condition_variable>
#include <mutex>

namespace server_lib {
namespace tests {

    class basic_network_fixture
    {
    public:
        basic_network_fixture() = default;

        std::string get_default_address()
        {
            return "127.0.0.1";
        }
        uint16_t get_default_port()
        {
            return 9999;
        }

        uint16_t get_free_port();

        bool waiting_for(bool& done,
                         std::condition_variable& done_cond,
                         std::mutex& done_cond_guard,
                         size_t sec_timeout = 10);
    };

} // namespace tests
} // namespace server_lib
