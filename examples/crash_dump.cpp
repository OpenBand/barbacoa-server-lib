#include <server_lib/application.h>
#include <server_lib/asserts.h>

#include <server_lib/options_helper.h>
#include <server_lib/emergency_helper.h>

#include "err_emulator.h"

#include <iostream>

#include <cstdio>
#include <cassert>

#include <boost/filesystem.hpp>

#if defined(SERVER_LIB_PLATFORM_LINUX)
#include <unistd.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#endif

#include <ctime>
#include <iomanip> // std::put_time, std::get_time
#include <sstream>

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

static const char* SSL_HELPERS_TIME_FORMAT = "%Y-%m-%dT%H:%M:%S";

std::string to_iso_string(const time_t t)
{
    std::stringstream ss;

    ss << std::put_time(std::localtime(&t), SSL_HELPERS_TIME_FORMAT);

    return ss.str();
}

int main(int argc, char* argv[])
{
    using namespace server_lib;

    Config config;

    if (!config.load(argc, argv))
        return 1;

    namespace fs = boost::filesystem;

    fs::path app_path { argv[0] };
    app_path = app_path.parent_path() / emergency_helper::create_dump_filename();
    auto dump_file = app_path.generic_string();

    auto&& app = application::init(dump_file.c_str());

    event_loop separated_loop;

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
#if defined(SERVER_LIB_PLATFORM_LINUX)
            // Core dump creation is OS responsibility and OS specific
            try
            {

                struct rlimit old_r, new_r;

                new_r.rlim_cur = RLIM_INFINITY;
                new_r.rlim_max = RLIM_INFINITY;

                SRV_ASSERT(prlimit(getpid(), RLIMIT_CORE, &new_r, &old_r) != -1);

                auto prev_core = app_path.parent_path() / "core";
                if (fs::exists(prev_core))
                {
                    auto tm = to_iso_string(fs::last_write_time(prev_core));
                    auto renamed = prev_core;
                    renamed = renamed.parent_path() / (std::string("core.") + tm);
                    fs::rename(prev_core, renamed);
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << e.what() << std::endl;
            }
#endif
            auto fail_emit = [&]() {
                if (try_fail(config.test_case))
                {
                    std::cerr << "Uops:(. Test case was vanished by compiler optimization" << std::endl;
                }
                else
                {
                    config.print_help();
                }
            };

            if (!config.in_separate_thread)
                fail_emit();
            else
            {
                separated_loop.start([&]() {
                    fail_emit();
                });
            }
        }

        if (app.is_running())
            app.stop();
        else
            exit(0);
    };

    auto exit_callback = [](const int) {
        std::cout << "Normal exit" << std::endl;
    };

    auto fail_callback = [](const int signo, const char* dump_file_path) {
        std::cerr << "Fail exit by singal " << signo << std::endl;
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
