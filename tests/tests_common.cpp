#include "tests_common.h"

#include <boost/filesystem.hpp>

#include <fstream>
#include <sstream>

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
            memset(buff, 0, sizeof(buff));
            auto* pbuff_offset = buff;
            for (auto cj = 0; cj < BUFF_SZ / sizeof(float); ++cj)
            {
                float fl = 9999 + ci++;
                memcpy(pbuff_offset, &fl, sizeof(float));
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

} // namespace tests
} // namespace server_lib
