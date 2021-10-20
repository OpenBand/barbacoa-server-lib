#pragma once

#include <boost/test/unit_test.hpp>
#include <boost/filesystem/path.hpp>

#include <string>
#include <condition_variable>
#include <mutex>

namespace server_lib {
namespace tests {

    template <typename Stream>
    void dump_str(Stream& s, const std::string& str)
    {
        s << ">>>\n";
        s << str << '\n';
        s << "<<<\n";
        s << std::flush;
    }

    inline std::string create_test_data()
    {
        static std::string data_ = "test";
        std::string data { data_ };
        size_t sz = data.size();
        data.push_back(1);
        data.push_back(2);
        data.push_back(0);
        data.append(data_);
        BOOST_REQUIRE_EQUAL(data.size(), sz * 2 + 3);
        return data;
    }

    boost::filesystem::path create_binary_data_file(const size_t file_size);

    void print_current_test_name();

    bool waiting_for_asynch_test(bool& done,
                                 std::condition_variable& done_cond,
                                 std::mutex& done_cond_guard,
                                 size_t sec_timeout = 10000000);
} // namespace tests
} // namespace server_lib

#ifdef NDEBUG
#define DUMP_STR(str) (str)
#else
#include <iostream>

#define DUMP_STR(str) \
    server_lib::tests::dump_str(std::cerr, str)
#endif
