#include <server_lib/application.h>
#include <server_lib/asserts.h>

#include <server_lib/options_helper.h>
#include <server_lib/emergency_helper.h>

#include "err_emulator.h"

#include <iostream>

#include <cstdio>
#include <cassert>

#include <boost/filesystem.hpp>

namespace bpo = server_lib::bpo;

struct Config_i
{
    std::string test_case = "double_delete";
    bool show_crash_dump = false;
    bool in_separate_thread = false;
};

class Config : public Config_i,
               protected server_lib::options
{
    void set_program_options(bpo::options_description& cli)
    {
        // clang-format off
        cli.add_options()
                ("try,t", bpo::value<std::string>()->default_value(test_case),
                         (std::string{"Case from list ["} + server_lib::get_fail_cases() + "]").c_str())
                ("show,s", "Print crash dump and clear")
                ("async,a", "Emulate crash in separate thread")
                ("help,h", "Display help message")
                ("version,v", "Print version number and exit");
        // clang-format on
    }

public:
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

            test_case = options["try"].as<std::string>();
            show_crash_dump = options.count("show");
            in_separate_thread = options.count("async");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error parsing command line: " << e.what() << std::endl;
            return false;
        }

        return true;
    }

    void print_help()
    {
        bpo::options_description cli_options(get_application_name());

        boost::program_options::variables_map options;

        bpo::options_description cli;
        set_program_options(cli);
        cli_options.add(cli);

        cli_options.print(std::cerr);
    }
};

int main(int argc, char* argv[])
{
    using namespace server_lib;

    Config config;

    if (!config.load(argc, argv))
        return 1;

    boost::filesystem::path app_path { argv[0] };
    app_path = app_path.parent_path() / emergency_helper::create_dump_filename();
    auto dump_file = app_path.generic_string();

    auto&& app = application::init(dump_file.c_str());

    auto payload = [&]() {
        if (config.show_crash_dump)
        {
            auto cr = emergency_helper::load_dump(dump_file.c_str());
            if (!cr.empty())
            {
                std::cerr << cr << std::endl;
            }
            else
            {
                std::cerr << "There is no crash dump in '" << dump_file << "'" << std::endl;
            }
        }
        else
        {
            bool result = false;
            if (!config.in_separate_thread)
            {
                result = try_fail(config.test_case);
            }
            else
            {
            }
            if (!result)
            {
                config.print_help();
                exit(1);
            }
            std::cerr << "Uops:(. Test case was vanished by compiler optimization" << std::endl;
        }

        if (app.is_running())
            app.stop();
        else
            exit(0);
    };

    auto exit_callback = []() {
        std::cout << "Normal exit" << std::endl;
    };

    auto fail_callback = [](const char* dump_file_path) {
        std::cerr << "Fail exit" << std::endl;
        if (dump_file_path)
        {
            std::cerr << "Dump saved to '" << dump_file_path
                      << "'. Preview:\n";

            if (emergency_helper::save_demangled_dump(dump_file_path, dump_file_path))
            {
                auto dump = emergency_helper::load_dump(dump_file_path, false);
                std::cerr << dump.c_str();
            }
        }
    };

    return app.on_start(payload).on_exit(exit_callback).on_fail(fail_callback).run();
}
