#include "tests_common.h"

#include <cstdint>
#include <sstream>

#include <server_lib/network/raw_builder.h>
#include <server_lib/network/msg_builder.h>
#include <server_lib/network/dstream_builder.h>

namespace server_lib {
namespace tests {

    using namespace server_lib::network;

    BOOST_AUTO_TEST_SUITE(network_tests)

    BOOST_AUTO_TEST_CASE(raw_builder_recive_check)
    {
        print_current_test_name();

        raw_builder builder;

        const std::string chunk1 = "test1";
        const std::string chunk2 = chunk1 + "test2";

        std::string input_data;

        builder << input_data;

        BOOST_REQUIRE(!builder.unit_ready());
        BOOST_REQUIRE(!builder.get_unit().ok());

        input_data = chunk1;

        builder << input_data;

        BOOST_REQUIRE(builder.unit_ready());
        BOOST_REQUIRE(input_data.empty());

        BOOST_REQUIRE(builder.get_unit().ok());
        BOOST_REQUIRE_EQUAL(builder.get_unit().as_string(), chunk1);

        input_data = chunk2;

        builder << input_data;

        BOOST_REQUIRE(builder.unit_ready());
        BOOST_REQUIRE(!input_data.empty());

        builder.reset();

        builder << input_data;

        BOOST_REQUIRE(builder.unit_ready());
        BOOST_REQUIRE(input_data.empty());

        BOOST_REQUIRE(builder.get_unit().ok());
        BOOST_REQUIRE_EQUAL(builder.get_unit().as_string(), chunk2);
    }

    BOOST_AUTO_TEST_CASE(raw_builder_send_check)
    {
        print_current_test_name();

        raw_builder builder;

        const std::string chunk1 = "test";

        auto unit = builder.create(chunk1);

        BOOST_REQUIRE(unit.ok());
        BOOST_REQUIRE_EQUAL(unit.as_string(), chunk1);
    }

    BOOST_AUTO_TEST_CASE(raw_builder_like_protocol_check)
    {
        print_current_test_name();

        raw_builder builder;

        auto protocol = std::shared_ptr<app_unit_builder_i>(builder.clone());

        BOOST_REQUIRE(protocol);
    }

    BOOST_AUTO_TEST_CASE(int_builder_recive_check)
    {
        print_current_test_name();

        integer_builder builder;

        const uint32_t chunk1 = 11;
        const uint32_t chunk2 = 67099;

        std::string input_data;

        builder << input_data;

        BOOST_REQUIRE(!builder.unit_ready());
        BOOST_REQUIRE(!builder.get_unit().ok());

        input_data = integer_builder::pack(chunk1);

        builder << input_data;

        BOOST_REQUIRE(builder.unit_ready());
        BOOST_REQUIRE(input_data.empty());

        BOOST_REQUIRE(builder.get_unit().ok());
        BOOST_REQUIRE_EQUAL(builder.get_unit().as_integer(), chunk1);

        input_data = integer_builder::pack(chunk2);

        builder << input_data;

        BOOST_REQUIRE(builder.unit_ready());
        BOOST_REQUIRE(!input_data.empty());

        builder.reset();

        builder << input_data;

        BOOST_REQUIRE(builder.unit_ready());
        BOOST_REQUIRE(input_data.empty());

        BOOST_REQUIRE(builder.get_unit().ok());
        BOOST_REQUIRE_EQUAL(builder.get_unit().as_integer(), chunk2);
    }

    BOOST_AUTO_TEST_CASE(str_builder_recive_check)
    {
        print_current_test_name();

        string_builder builder;

        const std::string chunk1 = "test";
        const std::string chunk2 = "next test";

        BOOST_REQUIRE_GT(chunk2.size(), chunk1.size());

        std::string input_data;

        builder << input_data;

        BOOST_REQUIRE(builder.unit_ready());
        BOOST_REQUIRE(builder.get_unit().ok());
        BOOST_REQUIRE(builder.get_unit().is_null());

        input_data = chunk1;

        builder.reset();
        builder << input_data;

        BOOST_REQUIRE(builder.unit_ready());
        BOOST_REQUIRE(builder.get_unit().ok());
        BOOST_REQUIRE(builder.get_unit().is_null());
        BOOST_REQUIRE_EQUAL(input_data, chunk1);

        builder.set_size(input_data.size());
        builder.reset();
        builder << input_data;

        BOOST_REQUIRE(builder.unit_ready());
        BOOST_REQUIRE(builder.get_unit().ok());
        BOOST_REQUIRE_EQUAL(builder.get_unit().as_string(), chunk1);
        BOOST_REQUIRE(input_data.empty());

        input_data = chunk2;

        builder.reset();
        builder << input_data;

        BOOST_REQUIRE(builder.unit_ready());
        BOOST_REQUIRE(builder.get_unit().ok());
        BOOST_REQUIRE_EQUAL(builder.get_unit().as_string(), chunk2.substr(0, chunk1.size()));
        BOOST_REQUIRE_EQUAL(input_data, chunk2.substr(chunk1.size(), std::string::npos));
    }

    BOOST_AUTO_TEST_CASE(msg_builder_recive_check)
    {
        print_current_test_name();

        msg_builder builder { 1024 };

        const std::string chunk1 = "test";

        std::stringstream ss;

        ss << integer_builder::pack(chunk1.size()) << chunk1;

        std::string input_data { ss.str() };

        builder << input_data;

        BOOST_REQUIRE(builder.unit_ready());
        BOOST_REQUIRE(builder.get_unit().ok());
        BOOST_REQUIRE(builder.get_unit().is_string());
        BOOST_REQUIRE_EQUAL(builder.get_unit().as_string(), chunk1);
    }

    BOOST_AUTO_TEST_CASE(msg_builder_send_check)
    {
        print_current_test_name();

        msg_builder builder { 1024 };

        const std::string chunk1 = "test";

        auto unit = builder.create(chunk1);

        BOOST_REQUIRE(unit.ok());
        BOOST_REQUIRE(unit.is_root_for_nested_content());
        auto sub_units = unit.get_nested();
        BOOST_REQUIRE_EQUAL(sub_units.size(), 2u);
        BOOST_REQUIRE(sub_units[0].ok());
        BOOST_REQUIRE(sub_units[0].is_string());
        BOOST_REQUIRE_LE(sub_units[0].as_string().size(), sizeof(uint32_t));
        BOOST_REQUIRE(sub_units[1].ok());
        BOOST_REQUIRE(sub_units[1].is_string());
        BOOST_REQUIRE_EQUAL(sub_units[1].as_string(), chunk1);
    }

    BOOST_AUTO_TEST_CASE(msg_builder_sequence_check)
    {
        print_current_test_name();

        const std::string msg1 { "test" };
        const std::string msg2 { "next test" };

        msg_builder builder { 1024 };

        auto data = builder.create(msg1).to_network_string();
        data.append(builder.create(msg2).to_network_string());

        BOOST_REQUIRE_LE(data.size(), msg1.size() + msg2.size() + sizeof(uint32_t) * 2);

        std::vector<app_unit> units;
        while (!data.empty())
        {
            builder << data;
            if (builder.unit_ready())
            {
                units.push_back(builder.get_unit());
                builder.reset();
            }
        }

        BOOST_REQUIRE_EQUAL(units.size(), 2u);

        BOOST_REQUIRE_EQUAL(units[0].as_string(), msg1);
        BOOST_REQUIRE_EQUAL(units[1].as_string(), msg2);
    }

    BOOST_AUTO_TEST_CASE(msg_builder_invalid_check)
    {
        print_current_test_name();

        const std::string large_msg { "large message" };

        msg_builder builder { large_msg.size() / 2 };

        BOOST_REQUIRE_THROW(builder.create(large_msg), std::logic_error);

        msg_builder builder_for_large { large_msg.size() };

        auto data = builder_for_large.create(large_msg).to_network_string();

        BOOST_REQUIRE_LE(data.size(), large_msg.size() + sizeof(uint32_t));

        BOOST_REQUIRE_THROW(builder.operator<<(data), std::logic_error);
    }

    BOOST_AUTO_TEST_CASE(msg_builder_parse_by_chucks_check)
    {
        print_current_test_name();

        const std::string msg1 { "test" };
        const std::string msg2 { "next test" };

        msg_builder builder { 1024 };

        auto data = builder.create(msg1).to_network_string();
        data.append(builder.create(msg2).to_network_string());

        BOOST_REQUIRE_LE(data.size(), msg1.size() + msg2.size() + sizeof(uint32_t) * 2);

        const size_t little_chunk = sizeof(uint32_t) / 2;

        std::vector<app_unit> units;

        std::string chunk;
        for (size_t pos = 0; pos < data.size();)
        {
            auto start_pos = pos;
            pos += static_cast<decltype(pos)>(little_chunk);

            if (pos > data.size())
                pos = data.size();

            chunk += data.substr(start_pos, pos - start_pos);

            builder << chunk;

            if (builder.unit_ready())
            {
                units.push_back(builder.get_unit());
                builder.reset();
            }
        }

        BOOST_REQUIRE_EQUAL(units.size(), 2u);

        BOOST_REQUIRE_EQUAL(units[0].as_string(), msg1);
        BOOST_REQUIRE_EQUAL(units[1].as_string(), msg2);
    }

    BOOST_AUTO_TEST_CASE(msg_builder_like_protocol_check)
    {
        print_current_test_name();

        msg_builder builder { 1024 };

        auto protocol = std::shared_ptr<app_unit_builder_i>(builder.clone());

        BOOST_REQUIRE(protocol);
    }

    BOOST_AUTO_TEST_CASE(dstream_builder_recive_check)
    {
        print_current_test_name();

        const std::string delimeter = "ZZZ";
        const std::string chunk1 = "test";
        const std::string chunk2 = "next test";

        dstream_builder builder { delimeter.c_str() };

        std::string input_data { chunk1 };

        builder << input_data;

        BOOST_REQUIRE(!builder.unit_ready());
        BOOST_REQUIRE(!builder.get_unit().ok());

        input_data.append(delimeter);
        input_data.append(chunk2);

        builder << input_data;

        BOOST_REQUIRE(builder.unit_ready());
        BOOST_REQUIRE(builder.get_unit().ok());
        BOOST_REQUIRE_EQUAL(builder.get_unit().as_string(), chunk1);
        BOOST_REQUIRE_EQUAL(input_data, chunk2);

        builder.reset();

        input_data.append(delimeter);

        builder << input_data;

        BOOST_REQUIRE(builder.unit_ready());
        BOOST_REQUIRE(builder.get_unit().ok());
        BOOST_REQUIRE_EQUAL(builder.get_unit().as_string(), chunk2);
        BOOST_REQUIRE(input_data.empty());
    }

    BOOST_AUTO_TEST_CASE(dstream_builder_send_check)
    {
        print_current_test_name();

        const std::string delimeter = "ZZZ";

        dstream_builder builder { delimeter.c_str() };

        const std::string chunk1 = "test";

        auto unit = builder.create(chunk1);

        BOOST_REQUIRE(unit.ok());
        BOOST_REQUIRE_EQUAL(unit.as_string(), chunk1 + delimeter);
    }

    BOOST_AUTO_TEST_CASE(dstream_builder_like_protocol_check)
    {
        print_current_test_name();

        dstream_builder builder;

        auto protocol = std::shared_ptr<app_unit_builder_i>(builder.clone());

        BOOST_REQUIRE(protocol);
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace server_lib
