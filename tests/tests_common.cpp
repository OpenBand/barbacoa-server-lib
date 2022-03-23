#include "tests_common.h"

#include <boost/filesystem.hpp>

#include <fstream>
#include <sstream>

#include <cstring>
#include <set>


namespace server_lib {
namespace tests {

    boost::filesystem::path create_binary_data_file(const size_t file_size)
    {
        BOOST_REQUIRE_GT(file_size, 0u);

        boost::filesystem::path temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
        std::ofstream output { temp.generic_string(), std::ofstream::binary };

        constexpr size_t BUFF_SZ = 1024;
        unsigned char buff[BUFF_SZ];
        size_t ci = 0;
        size_t total = 0;
        while (true)
        {
            std::memset(buff, 0, sizeof(buff));
            auto* pbuff_offset = buff;
            for (auto cj = 0; cj < BUFF_SZ / sizeof(float); ++cj)
            {
                float fl = 9999 + ci++;
                std::memcpy(pbuff_offset, &fl, sizeof(float));
                pbuff_offset += sizeof(float);
            }

            size_t to_write = std::min(sizeof(buff), file_size - total);
            output.write(reinterpret_cast<char*>(buff), to_write);
            total += to_write;
            if (total >= file_size)
                break;
        }

        return temp;
    }

    void print_current_test_name()
    {
        static uint32_t test_counter = 0;

        std::stringstream ss;

        ss << "TEST (";
        ss << ++test_counter;
        ss << ") - [";
        ss << boost::unit_test::framework::current_test_case().p_name;
        ss << "]";
        DUMP_STR(ss.str());
    }

    bool waiting_for_asynch_test(bool& done,
                                 std::condition_variable& done_cond,
                                 std::mutex& done_cond_guard,
                                 size_t sec_timeout)
    {
        std::unique_lock<std::mutex> lck(done_cond_guard);
        if (!done)
        {
            done_cond.wait_for(lck, std::chrono::seconds(sec_timeout), [&done]() {
                return done;
            });
        }
        return done;
    }

#if 1 // Copy & Paste from git@github.com:Andrei-Masilevich/barbacoa-ssl-helpers.git
    namespace {
        class printable_index
        {
        public:
            printable_index(const char* data)
            {
                const char* pch = data;
                while (*pch)
                {
                    _index.emplace(*pch);
                    pch++;
                }
            }

            bool operator()(char ch)
            {
                auto it = _index.find(ch);
                return _index.end() != it;
            }

        private:
            std::set<char> _index;

            // Python string.printable:
        } __printable_bytes = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "!\"#$%&\'()*+,-./:;<=>?@[\\]^_`{|}~ \t\n\r\x0b\x0c";
    } // namespace

    std::string to_printable(const std::string& data, char replace, const std::string& exclude)
    {
        // TODO: for better performance use ASCII index comparison algorithm instead that
        // but this must have enough performance for most cases

        std::string converted(data.begin(), data.end());
        printable_index indexed_exclude(exclude.c_str()); //suppose this list is short enough

        std::replace_if(
            converted.begin(), converted.end(), [&](char ch) {
                return !__printable_bytes(ch) || indexed_exclude(ch);
            },
            replace);

        return converted;
    }
#endif

} // namespace tests
} // namespace server_lib
