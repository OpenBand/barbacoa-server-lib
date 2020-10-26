#pragma once

#include <iosfwd>
#include <set>
#include <map>
#include <vector>
#include <array>
#include <string>
#include <sstream>

#include <server_lib/logger.h>
#include <server_lib/platform_config.h>

#ifndef SRV_FUNCTION_NAME_
#if defined(SERVER_LIB_PLATFORM_WINDOWS)
#define SRV_FUNCTION_NAME_ __FUNCTION__
#else //*NIX
#define SRV_FUNCTION_NAME_ __func__
#endif
#endif

#define TO_STR2(X) #X
#define TO_STR(X) TO_STR2(X)

#if defined(USE_NO_LOG)

#define LOG_LOG(LEVEL, FILE, LINE, FUNC, ARG) \
    SRV_EXPAND_MACRO(                         \
        SRV_MULTILINE_MACRO_BEGIN {           \
        } SRV_MULTILINE_MACRO_END)

#else

namespace server_lib {

class trim_file_path
{
    std::string _file;

public:
    trim_file_path(const char* FILE)
        : _file(FILE)
    {
#if defined(PROJECT_SOURCE_DIR)
#if defined(SERVER_LIB_PLATFORM_WINDOWS)
        std::replace(m_file.begin(), _file.end(), '\\', '/');
#endif
        auto start_pos = _file.find(PROJECT_SOURCE_DIR);

        if (start_pos != std::string::npos)
            _file.erase(start_pos, sizeof(PROJECT_SOURCE_DIR));
#endif
    }

    operator std::string() { return _file; }
};

#define LOG_LOG(LEVEL, FILE, LINE, FUNC, ARG)                    \
    SRV_EXPAND_MACRO(                                            \
        SRV_MULTILINE_MACRO_BEGIN {                              \
            server_lib::logger::log_message msg;                 \
            msg.context.lv = LEVEL;                              \
            msg.context.file = server_lib::trim_file_path(FILE); \
            msg.context.line = LINE;                             \
            msg.context.method = FUNC;                           \
            msg.message << ARG;                                  \
            server_lib::logger::instance().write(msg);           \
        } SRV_MULTILINE_MACRO_END)
#endif

#define LOG_TRACE(ARG) LOG_LOG(server_lib::logger::level::trace, __FILE__, __LINE__, SRV_FUNCTION_NAME_, ARG)
#define LOG_DEBUG(ARG) LOG_LOG(server_lib::logger::level::debug, __FILE__, __LINE__, SRV_FUNCTION_NAME_, ARG)
#define LOG_INFO(ARG) LOG_LOG(server_lib::logger::level::info, __FILE__, __LINE__, SRV_FUNCTION_NAME_, ARG)
#define LOG_WARN(ARG) LOG_LOG(server_lib::logger::level::warning, __FILE__, __LINE__, SRV_FUNCTION_NAME_, ARG)
#define LOG_ERROR(ARG) LOG_LOG(server_lib::logger::level::error, __FILE__, __LINE__, SRV_FUNCTION_NAME_, ARG)
#define LOG_FATAL(ARG) LOG_LOG(server_lib::logger::level::fatal, __FILE__, __LINE__, SRV_FUNCTION_NAME_, ARG)

#define LOGC_TRACE(ARG) LOG_TRACE(SRV_LOG_CONTEXT_ << ARG)
#define LOGC_DEBUG(ARG) LOG_DEBUG(SRV_LOG_CONTEXT_ << ARG)
#define LOGC_INFO(ARG) LOG_INFO(SRV_LOG_CONTEXT_ << ARG)
#define LOGC_WARN(ARG) LOG_WARN(SRV_LOG_CONTEXT_ << ARG)
#define LOGC_ERROR(ARG) LOG_ERROR(SRV_LOG_CONTEXT_ << ARG)
#define LOGC_FATAL(ARG) LOG_FATAL(SRV_LOG_CONTEXT_ << ARG)

#define PRINT_ENUM_VALUE(X, Y) \
    case X::Y:                 \
        stream << #Y;          \
        break;

template <typename K, typename V>
std::string to_string(const std::map<K, V>& value)
{
    std::stringstream stream;

    bool is_first = true;
    stream << "{";
    for (const auto& _item : value)
    {
        if (is_first)
        {
            is_first = false;
        }
        else
        {
            stream << ", ";
        }
        stream << _item.first << ": " << _item.second;
    }
    stream << "}";

    return stream.str();
}


template <typename T>
std::string to_string(const std::set<T>& value)
{
    std::stringstream stream;

    stream << "(";
    for (const auto& _item : value)
    {
        stream << " " << _item;
    }
    stream << " )";

    return stream.str();
}


template <typename T>
const std::string to_json(const std::set<T>& value)
{
    std::stringstream stream;

    bool is_first = true;
    stream << "[";
    for (const auto& item : value)
    {
        if (is_first)
        {
            is_first = false;
        }
        else
        {
            stream << ",";
        }
        stream << "\"" << item << "\"";
    }
    stream << " ]";

    return stream.str();
}


template <typename T>
std::string to_string(const std::vector<T>& value)
{
    std::stringstream stream;

    stream << "[";
    for (const auto& _item : value)
    {
        stream << " " << _item;
    }
    stream << " ]";

    return stream.str();
}


template <typename T, size_t S>
std::string to_string(const std::array<T, S>& value)
{
    std::stringstream stream;

    stream << "[";
    for (const auto& _item : value)
    {
        stream << " " << _item;
    }
    stream << " ]";

    return stream.str();
}


template <typename T, size_t S>
std::string to_json(const std::array<T, S>& value)
{
    std::stringstream stream;

    bool is_first = true;
    stream << "[";
    for (const auto& item : value)
    {
        if (is_first)
        {
            is_first = false;
        }
        else
        {
            stream << ",";
        }
        stream << "\"" << item << "\"";
    }
    stream << " ]";

    return stream.str();
}

#if defined(SERVER_LIB_SUPPRESS_LOGS)
#if defined(SERVER_LIB_LOGS)
#undef SERVER_LIB_LOGS
#endif
#else // SERVER_LIB_SUPPRESS_LOGS
#ifndef NDEBUG
#if !defined(SERVER_LIB_LOGS)
#define SERVER_LIB_LOGS
#endif
#endif //DEBUG
#endif // !SERVER_LIB_SUPPRESS_LOGS

#ifdef SERVER_LIB_LOGS
#define SRV_LOG_TRACE(ARG) LOG_TRACE(ARG)
#define SRV_LOG_DEBUG(ARG) LOG_DEBUG(ARG)
#define SRV_LOG_INFO(ARG) LOG_INFO(ARG)
#define SRV_LOG_WARN(ARG) LOG_WARN(ARG)
#define SRV_LOG_ERROR(ARG) LOG_ERROR(ARG)
#define SRV_LOG_FATAL(ARG) LOG_FATAL(ARG)

#define SRV_LOGC_TRACE(ARG) LOGC_TRACE(ARG)
#define SRV_LOGC_DEBUG(ARG) LOGC_DEBUG(ARG)
#define SRV_LOGC_INFO(ARG) LOGC_INFO(ARG)
#define SRV_LOGC_WARN(ARG) LOGC_WARN(ARG)
#define SRV_LOGC_ERROR(ARG) LOGC_ERROR(ARG)
#define SRV_LOGC_FATAL(ARG) LOGC_FATAL(ARG)
#else
#define SRV_LOG_TRACE(ARG)
#define SRV_LOG_DEBUG(ARG)
#define SRV_LOG_INFO(ARG)
#define SRV_LOG_WARN(ARG)
#define SRV_LOG_ERROR(ARG)
#define SRV_LOG_FATAL(ARG)

#define SRV_LOGC_TRACE(ARG)
#define SRV_LOGC_DEBUG(ARG)
#define SRV_LOGC_INFO(ARG)
#define SRV_LOGC_WARN(ARG)
#define SRV_LOGC_ERROR(ARG)
#define SRV_LOGC_FATAL(ARG)
#endif

} // namespace server_lib
