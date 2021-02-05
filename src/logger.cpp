#include <server_lib/logger.h>
#include <server_lib/platform_config.h>
#include <server_lib/asserts.h>

#if defined(PLATFORM_IOS)
//logger not used
#define USE_NO_LOG
#if defined(USE_SERVER_LOGS)
#undef USE_SERVER_LOGS
#endif
#endif

#if defined(USE_SERVER_LOGS)

#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>

#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/trivial.hpp>

#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>

#include <boost/core/null_deleter.hpp>

#if defined(SERVER_LIB_PLATFORM_LINUX)
#include <pthread.h>
#include <sys/syscall.h> //SYS_gettid
#include <syslog.h>
#include <unistd.h>
#include <sys/syscall.h>
#elif defined(SERVER_LIB_PLATFORM_WINDOWS)
#include <windows.h>
#endif //SERVER_LIB_PLATFORM_LINUX

#include <cassert>

#endif // defined(USE_SERVER_LOGS)

namespace server_lib {
namespace impl {
    auto get_thread_info()
    {
        uint64_t thread_id = 0;
        std::string thread_name;
#if defined(USE_SERVER_LOGS) && defined(SERVER_LIB_PLATFORM_LINUX)
        thread_id = static_cast<decltype(thread_id)>(syscall(SYS_gettid));
        static int MAX_THREAD_NAME_SZ = 15;
        char buff[MAX_THREAD_NAME_SZ + 1];
        if (!pthread_getname_np(pthread_self(),
                                buff, sizeof(buff)))
        {
            thread_name = buff;
        }
#endif
        return std::make_pair(thread_id, thread_name);
    }

    static auto s_main_thread_info = get_thread_info();
} // namespace impl
#if defined(USE_SERVER_LOGS)
BOOST_LOG_GLOBAL_LOGGER(sysLogger, boost::log::sources::severity_logger<boost::log::trivial::severity_level>);
BOOST_LOG_GLOBAL_LOGGER_DEFAULT(sysLogger, boost::log::sources::severity_logger<boost::log::trivial::severity_level>)
#endif

std::atomic_ulong logger::log_context::s_id_counter(0u);

logger::log_context::log_context()
    : id(s_id_counter++)
{
    thread_info = impl::get_thread_info();
#if defined(SERVER_LIB_PLATFORM_LINUX)
    if (impl::s_main_thread_info.second == thread_info.second)
        thread_info.second.clear();
#endif
}

#if defined(SERVER_LIB_PLATFORM_WINDOWS)
namespace impl {
    static bool _opt_debugger_on = false;
    const char* get_debug_output_endl()
    {
        if (_opt_debugger_on)
        {
            return "\r\n";
        }
        return "";
    }
} // namespace impl
#endif

// clang-format off
template <typename SinkTypePtr>
void logger::add_boost_log_destination(const SinkTypePtr &sink, const std::string &dtf)
{
#if defined(USE_SERVER_LOGS)

    SRV_ASSERT(sink);

    auto to_boost_level = [](level lv) ->boost::log::trivial::severity_level {
        switch (lv)
        {
        case server_lib::logger::level::trace:
            return boost::log::trivial::trace;
        case server_lib::logger::level::debug:
            return boost::log::trivial::debug;
        case server_lib::logger::level::info:
            return boost::log::trivial::info;
        case server_lib::logger::level::warning:
            return boost::log::trivial::warning;
        case server_lib::logger::level::error:
            return boost::log::trivial::error;
        case server_lib::logger::level::fatal:
            return boost::log::trivial::fatal;
        }

        return boost::log::trivial::trace;
    };

    namespace expr = boost::log::expressions;
    using namespace boost::log;

    sink->set_formatter(
        expr::stream << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", dtf)
        << " [" << std::setw(5) << expr::attr<trivial::severity_level>("Severity") << "]"
        << expr::smessage);

    core::get()->add_sink(sink);

    add_common_attributes();

    sources::logger lg;

    auto boost_write = [=](const log_message& msg) {
        BOOST_LOG_SEV(sysLogger::get(), to_boost_level(msg.context.lv))
            << boost::log::add_value("Line", msg.context.line) 
            << boost::log::add_value("File", msg.context.file)
            << boost::log::add_value("Function", msg.context.method)
            << "[" << static_cast<unsigned long>(msg.context.thread_info.first)
            << (msg.context.thread_info.second.empty()?(msg.context.thread_info.second):(std::string("-") + msg.context.thread_info.second))
            << "]"
            << msg.message.str() 
            << " (from " << msg.context.file << ":" << msg.context.line << ")"
#if defined(SERVER_LIB_PLATFORM_WINDOWS)
            << impl::get_debug_output_endl()
#endif
            ;
    };

    add_destination(std::move(boost_write));

#endif //USE_SERVER_LOGS
}

void logger::add_stdout_destination()
{
    auto cout_write = [](const log_message& msg) {
        printf("[%ld] %s (from %s:%d)\n", msg.context.thread_info.first, msg.message.str().c_str(), msg.context.file.c_str(), msg.context.line);
    };
    add_destination(std::move(cout_write));
}

void logger::add_syslog_destination()
{
#if defined(USE_SERVER_LOGS) && defined(SERVER_LIB_PLATFORM_LINUX)
    auto to_syslog_level = [](level lv) ->int {
        switch (lv)
        {
        case server_lib::logger::level::trace:
            return LOG_DEBUG;
        case server_lib::logger::level::debug:
            return LOG_DEBUG;
        case server_lib::logger::level::info:
            return LOG_INFO;
        case server_lib::logger::level::warning:
            return LOG_WARNING;
        case server_lib::logger::level::error:
            return LOG_ERR;
        case server_lib::logger::level::fatal:
            return LOG_CRIT;
        default:;
        }
        return LOG_DEBUG;
    };

    #if defined(SYSLOG_WITHOUT_SOURCE_FROM)
        auto syslog_write = [=](const log_message& msg) {
            syslog(to_syslog_level(msg.context.lv), "[%ld] %s", msg.context.thread_id, msg.message.str().c_str());
        };

        add_destination(std::move(syslog_write));

    #else
        auto syslog_write = [=](const log_message& msg) {
            syslog(to_syslog_level(msg.context.lv), "[%ld] %s (from %s:%d)", msg.context.thread_info.first, msg.message.str().c_str(), msg.context.file.c_str(), msg.context.line);
        };
        add_destination(std::move(syslog_write));

    #endif
#endif
}
// clang-format on


void logger::init_sys_log()
{
    //Use init_sys_log to switch on syslog instead obsolete macro USE_NATIVE_SYSLOG
#if defined(USE_SERVER_LOGS) && defined(SERVER_LIB_PLATFORM_LINUX)
    openlog(NULL, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL2);
#endif
    add_syslog_destination();

#if defined(DUPLICATE_LOGS_TO_COUT)
    add_stdout_destination();
#endif
}

void logger::init_file_log(const char* file_path, const size_t rotation_size_kb, const bool flush, const char* dtf)
{
#if defined(USE_SERVER_LOGS)

    using namespace boost::log;

    boost::shared_ptr<sinks::text_file_backend> backend = boost::make_shared<sinks::text_file_backend>(
        keywords::file_name = file_path,
        keywords::rotation_size = rotation_size_kb * 1024);

    using text_file_sink = sinks::asynchronous_sink<sinks::text_file_backend>;

    boost::shared_ptr<text_file_sink> sink = boost::make_shared<text_file_sink>(backend);

    if (flush)
    {
        sink->locked_backend()->auto_flush(true);
    }

    add_boost_log_destination(sink, (dtf && strlen(dtf) > 1) ? std::string { dtf } : std::string { DEFAULT_DATE_TIME_FORMAT });

#if defined(DUPLICATE_LOGS_TO_COUT)
    add_stdout_destination();
#endif
#endif
}

void logger::init_debug_log(bool async, bool cerr, const char* dtf)
{
#if defined(USE_SERVER_LOGS)
    using namespace boost::log;

    using text_sink_base_type = sinks::aux::make_sink_frontend_base<sinks::text_ostream_backend>::type;

    boost::shared_ptr<text_sink_base_type> sink;
#if defined(SERVER_LIB_PLATFORM_WINDOWS)
    if (IsDebuggerPresent())
    {
        impl::_opt_debugger_on = true;
        auto sink_impl = boost::make_shared<sinks::asynchronous_sink<sinks::debug_output_backend>>();
        sink = sink_impl;
    }
    else
#endif
    {
        boost::shared_ptr<std::ostream> stream { (cerr) ? &std::cerr : &std::cout,
                                                 boost::null_deleter {} };

        if (async)
        {
            auto sink_impl = boost::make_shared<sinks::asynchronous_sink<sinks::text_ostream_backend>>();
            sink_impl->locked_backend()->add_stream(stream);
            sink_impl->locked_backend()->auto_flush(true);
            sink = sink_impl;
        }
        else
        {
            auto sink_impl = boost::make_shared<sinks::synchronous_sink<sinks::text_ostream_backend>>();
            sink_impl->locked_backend()->add_stream(stream);
            sink_impl->locked_backend()->auto_flush(true);
            sink = sink_impl;
        }
    }

    add_boost_log_destination(sink, (dtf && strlen(dtf) > 1) ? std::string { dtf } : std::string { DEFAULT_DATE_TIME_FORMAT });
#endif
}

void logger::set_level_filter(int filter)
{
    constexpr int max_filter = static_cast<int>(level::error) + static_cast<int>(level::warning) + static_cast<int>(level::info) + static_cast<int>(level::debug) + static_cast<int>(level::trace);
    SRV_ASSERT(filter >= 0 && filter <= max_filter);

    _filter = filter;
}

void logger::add_destination(log_handler_type&& handler)
{
    _appenders.push_back(std::move(handler));
}

void logger::write(log_message& msg)
{
    auto _lv = static_cast<int>(msg.context.lv);
    if (!_lv || ~_filter & _lv)
    {
        for (const auto& appender : _appenders)
        {
            appender(msg);
        }
    }
}

void logger::flush()
{
    if (!_appenders.empty())
    {
        using namespace boost::log;

        core::get()->flush();
    }
}

} // namespace server_lib
