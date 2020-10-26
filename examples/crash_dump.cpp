#include <server_lib/mt_server.h>
#include <server_lib/asserts.h>

#include <server_lib/options_helper.h>
#include <server_lib/emergency_helper.h>

#include "err_emulator.h"

#include <iostream>

#include <cstdio>
#include <cassert>

#include <boost/filesystem.hpp>

namespace bpo = server_lib::bpo;

class Config : protected server_lib::server_options
{
    void set_program_options(bpo::options_description& cli)
    {
        // clang-format off
        cli.add_options()
                ("try", bpo::value<uint16_t>()->default_value(0), "Fail number")
                ("show,s", "Print crash dump and clear")
                ("help,h", "Display help message")
                ("version,v", "Print version number and exit");
        // clang-format on
    }

public:
    server_lib::fail test_fail = server_lib::fail::try_none;
    bool show_crash_dump = false;

    bool load(int argc, char* argv[])
    {
        try
        {
            bpo::options_description cli_options(get_application_name());

            boost::program_options::variables_map options;

            bpo::options_description cli;
            set_program_options(cli);
            cli_options.add(cli);

            bpo::store(bpo::parse_command_line(argc, argv, cli_options), options);
            bpo::notify(options);

            if (options.count("help"))
            {
                cli_options.print(std::cout);
                exit(0);
            }

            if (options.count("version"))
            {
                print_version(std::cout);
                exit(0);
            }

            if (options.count("try"))
            {
                auto opt = options["try"].as<uint16_t>();

                test_fail = static_cast<server_lib::fail>(opt);
            }

            show_crash_dump = options.count("show");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error parsing command line: " << e.what() << std::endl;
            return false;
        }

        return true;
    }
};

int main(int argc, char* argv[])
{
    using namespace server_lib;

    Config config;

    if (!config.load(argc, argv))
        return 1;

    auto&& server = mt_server::instance();
    server.init();

    boost::filesystem::path p_bin { argv[0] };
    auto bin_name = p_bin.filename().generic_string();
    bin_name.append(".dump");
    p_bin = p_bin.parent_path() / bin_name;
    auto dump_file = p_bin.generic_string();

    server.set_crash_dump_file_name(dump_file.c_str());

    main_loop ml;

    auto payload = [&config, &dump_file, &ml]() {
        if (config.show_crash_dump)
        {
            auto cr = emergency_helper::load_dump(dump_file.c_str());
            if (!cr.empty())
            {
                std::cerr << cr << std::endl;
            }
            else
            {
                std::cerr << "There is not crash dump at '" << dump_file << "'" << std::endl;
            }
        }
        else
        {
            if (server_lib::fail::try_none != config.test_fail)
            {
                std::cout << "Try fail test " << static_cast<int>(config.test_fail) << std::endl;
                try_fail(config.test_fail);
            }
            else
            {
                std::cout << "Try normal exit" << std::endl;
            }
        }
        ml.exit(0);
    };

    ml.post(payload);

    auto exit_callback = [&server, &ml]() {
        std::cout << "In normal exit" << std::endl;
        assert(event_loop::is_main_thread());
        server.stop(ml);
    };

    auto fail_callback = [](const char* dump_file_path) {
        fprintf(stderr, "emergency_helper dump saved to '%s'. Preview:\n", dump_file_path);

        auto dump = emergency_helper::load_dump(dump_file_path, false);
        fprintf(stderr, "%s", dump.c_str());
    };

    return server.run(ml, exit_callback, fail_callback);
}
