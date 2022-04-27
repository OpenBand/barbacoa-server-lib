#pragma once

#include <functional>
#include <string>
#include <sstream>
#include <vector>
#include <atomic>
#include <cstdint>
#include <utility>

#include <server_lib/singleton.h>

namespace server_lib {

/**
 * \ingroup common
 *
 * \brief This is the Logger with different destinations
 * (standard output, files, syslog)
 * with extension ability.
 *
 * It is used with helpers macros:
 * \li \c LOG_TRACE
 * \li \c LOG_DEBUG
 * \li \c LOG_INFO
 * \li \c LOG_WARN
 * \li \c LOG_ERROR
 * \li \c LOG_FATAL
 * \n
 * \n
 * \li \c LOGC_TRACE (with log context locally defined by SRV_LOG_CONTEXT_)
 * \li \c LOGC_DEBUG
 * \li \c LOGC_INFO
 * \li \c LOGC_WARN
 * \li \c LOGC_ERROR
 * \li \c LOGC_FATAL
 *
 * \code{.c}
 *
 * LOG_TRACE("Foo");
 * LOG_WARN("Foo " << 42 << '!');
 *
 * \endcode
 *
 * \code{.c}
 * #define SRV_LOG_CONTEXT_ "payments> " << SRV_FUNCTION_NAME_ << ": "
 *
 * LOGC_ERROR("Invalid ticket. Id = " << id);
 *
 * \endcode
*/
class logger : public singleton<logger>
{
public:
    enum class level
    {
        fatal = 0x0, //not filtered
        error = 0x1,
        warning = 0x2,
        info = 0x4,
        debug = 0x8,
        trace = 0x10,
    };

    struct log_context
    {
        unsigned long id;
        level lv;
        std::string file;
        int line;
        std::string method;
        using thread_info_type = std::pair<uint64_t, std::string>;
        thread_info_type thread_info;

        log_context();

    private:
        static std::atomic_ulong s_id_counter;
    };

    struct log_message
    {
        log_context context;
        std::stringstream message;
    };

    using log_handler_type = std::function<void(const log_message&)>;

protected:
    logger();

    friend class singleton<logger>;

public:
    static constexpr const char* DEFAULT_DATE_TIME_FORMAT = "%Y-%m-%d_%H:%M:%S.%f";

    void init_sys_log();
    void init_file_log(const char* file_path, const size_t rotation_size_kb, const bool flush, const char* dtf = DEFAULT_DATE_TIME_FORMAT);
    void init_debug_log(bool async = false, bool cerr = false, const char* dtf = DEFAULT_DATE_TIME_FORMAT);
    void lock();
    void unlock();

    //suppress trace logs by default
    void set_level_filter(int filter = 0x10);

    void add_destination(log_handler_type&& handler);

    void write(log_message& msg);

    size_t get_appender_count() const
    {
        return _appenders.size();
    }

    void flush();

private:
    void add_syslog_destination();
    void add_stdout_destination();
    template <typename SinkTypePtr>
    void add_boost_log_destination(const SinkTypePtr& sink, const std::string& dtf);

private:
    std::vector<log_handler_type> _appenders;
    int _filter = 0;
    std::atomic_bool _logs_on;
};

} // namespace server_lib
