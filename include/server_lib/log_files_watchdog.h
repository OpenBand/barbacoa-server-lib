#pragma once

#include <server_lib/event_loop.h>

#include <boost/filesystem/path.hpp>
#include <string>

namespace server_lib {

/**
 * \ingroup common
 *
 * This class manages log files (if logs are been writing to files)
 */
struct log_files_watchdog_config
{
    /**
     * Accepted variable pattern for logs:
     *  \code
     *  file_%N.log
     *  file_%3N.log
     *  file_%Y%m%d.log
     *  file_(%N)_%Y-%m-%d_%H-%M-%S.log
     *  file_%Y-%m-%d_%H-%M-%S=%10N.log
     *  \endcode
    */
    log_files_watchdog_config(const boost::filesystem::path& dir_path,
                              const std::string& log_name,
                              const std::string& log_variable_pattern,
                              const std::string& log_extension,
                              const size_t max_files,
                              const std::string& pack_command = {});

    const boost::filesystem::path dir_path;
    const std::string log_name;
    const std::string log_variable_pattern;
    const std::string log_extension;
    const size_t max_files;
    const std::string pack_command;
};

class log_files_watchdog : public event_loop,
                           public log_files_watchdog_config
{
public:
    using log_files_watchdog_config::log_files_watchdog_config;

    static void remove_excess_logs(const log_files_watchdog_config& config);

    static void pack_excess_logs(const log_files_watchdog_config& config);

    using Strategy = std::function<void(const log_files_watchdog_config&)>;

    void watch(const size_t seconds, Strategy strategy)
    {
        start_timer(std::chrono::seconds(seconds),
                    [this, seconds, strategy]() {
                        strategy(*this);
                        watch(seconds, strategy);
                    });
    }

    void watch_once(const size_t seconds, Strategy strategy)
    {
        start_timer(std::chrono::seconds(seconds),
                    [this, strategy]() {
                        synch_watch(strategy);
                    });
    }

    void watch(Strategy strategy)
    {
        post([this, strategy]() {
            strategy(*this);
        });
    }

    void synch_watch(Strategy strategy)
    {
        strategy(*this);
    }

    /**
     * Listen OS filesystem events (inotify)
     * effective if the output of the observed files
     * is carried out in a separate folder
     */
    void auto_watch(Strategy strategy);
};
} // namespace server_lib
