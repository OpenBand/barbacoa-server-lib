#include <server_lib/log_files_watchdog.h>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>

#include <boost/date_time/time_facet.hpp>
#include <boost/exception/diagnostic_information.hpp>

#include <server_lib/asserts.h>

#include <iostream>
#include <locale>
#include <cstdlib>
#include <map>

#ifdef SERVER_LIB_PLATFORM_LINUX
#include <sys/types.h>
#include <sys/stat.h>

#include <linux/fs.h>
#endif

#include "logger_set_internal_group.h"

namespace server_lib {

namespace bfs = boost::filesystem;

using bfs::path;
using boost::posix_time::time_input_facet;

namespace impl {

    std::string normalize_extension(const std::string& extension)
    {
        std::string normalized;
        if (!extension.empty() && *begin(extension) != '.')
        {
            normalized = ".";
            normalized.append(extension);
        }
        else
        {
            normalized = extension;
        }
        return normalized;
    }

    template <typename Pos>
    void scroll(std::string::iterator& it, const std::string& ln, const Pos chars)
    {
        size_t ci = 0;
        while (it != ln.end() && ci++ < (size_t)chars)
        {
            ++it;
        }
    }

    void scroll_to_separator(std::string::iterator& it, const std::string& ln)
    {
        while (it != ln.end())
        {
            char ch = *it;
            if (ch == '%')
            {
                break;
            }
            ++it;
        }
    }

    void scroll_by_symbol(std::string::iterator& it, const std::string& ln)
    {
        scroll(it, ln, 1);
    }

    char read_symbol(std::string::iterator& it, const std::string& ln)
    {
        if (it == ln.end())
            return 0;
        return *it++;
    }

    void scroll_number(std::string::iterator& it, const std::string& ln)
    {
        std::string ss;
        char s = read_symbol(it, ln);
        bool none_zero = false;
        while (s && std::isdigit(s, std::locale("C")))
        {
            none_zero |= s != '0';
            ss.append(1, s);
            s = read_symbol(it, ln);
        }
        if (s)
            --it;
        if (std::atoi(ss.c_str()) < 1 && none_zero)
            throw std::ios_base::failure("Invalid number");
    }

    //read "%....s"
    char read_pattern(std::string::iterator& it, const std::string& ln)
    {
        if (it == ln.end() || *it != '%')
            return 0;

        scroll_by_symbol(it, ln);
        auto s = read_symbol(it, ln);
        while (s && std::isdigit(s, std::locale("C")))
        {
            s = read_symbol(it, ln);
        }
        return s;
    }

    bool check_pattern(const std::string& log_mute_pattern)
    {
        return log_mute_pattern.size() >= 2 && log_mute_pattern.find('%') != std::string::npos;
    }

    using file_sequence_id_type = std::tuple<std::time_t, unsigned, unsigned>;

    file_sequence_id_type get_file_sequence_id(const boost::filesystem::path& file_path)
    {
        auto time = boost::filesystem::last_write_time(file_path);

#ifdef SERVER_LIB_PLATFORM_LINUX
        struct stat s;
        int e(::stat(file_path.c_str(), &s));
        if (!e)
        {
            auto dev_id = s.st_dev;
            auto fs_id = s.st_ino;

            return std::make_tuple(time, dev_id, fs_id);
        }
#endif
        return std::make_tuple(time, 0, 0);
    }

    struct match_context
    {
        match_context(const std::string& log_variable_pattern_)
            : log_variable_pattern(log_variable_pattern_)
        {
        }

        bool valid() const
        {
            return !log_variable_pattern.empty();
        }

    protected:
        std::string log_variable_pattern;
        std::string time_pattern;

        friend bool match_file(const path&, const log_files_watchdog_config&, match_context&);
    };

    bool match_file(const path& file_path, const log_files_watchdog_config& config, match_context& context)
    {
        if (!bfs::is_regular_file(file_path) || !context.valid())
            return false;

        auto file_name = file_path.filename().string();

        if (!config.log_name.empty())
        {
            if (config.log_name.size() > file_name.size() || file_name.compare(0, config.log_name.size(), config.log_name))
                return false;
        }

        if (!config.log_extension.empty())
        {
            if (!file_path.has_extension() || file_path.extension().string() != config.log_extension)
                return false;
        }

        auto& log_variable_pattern_ = context.log_variable_pattern;
        auto& time_pattern_ = context.time_pattern;
        bool single_time_pattern_for_file = true;
        try
        {
            auto pattern_it = begin(log_variable_pattern_);
            auto file_it = begin(file_name);

            auto end_pattern_it = end(log_variable_pattern_);
            auto end_file_it = end(file_name);

            scroll(file_it, file_name, config.log_name.size());

            do
            {
                scroll_to_separator(pattern_it, log_variable_pattern_);
                auto separator_pattern_it = pattern_it;
                switch (read_pattern(pattern_it, log_variable_pattern_))
                {
                case 'N': {
                    scroll_number(file_it, file_name);
                    auto pred_pattern_it = pattern_it;
                    scroll_to_separator(pattern_it, log_variable_pattern_);
                    separator_pattern_it = pattern_it;
                    read_pattern(pattern_it, log_variable_pattern_);
                    std::string pattern_rest { pred_pattern_it, separator_pattern_it };
                    if (!pattern_rest.empty())
                    {
                        std::string file_rest { file_it, end_file_it };
                        if (file_rest.size() < pattern_rest.size() || file_rest.compare(0, pattern_rest.size(), pattern_rest))
                        {
                            throw std::ios_base::failure("Invalid rest");
                        }
                        scroll(file_it, file_name, pattern_rest.size());
                        pattern_it = pred_pattern_it;
                    }
                }
                break;
                case '%':
                    if ('%' != read_symbol(file_it, file_name))
                    {
                        throw std::ios_base::failure("Invalid symbol");
                    }
                    break;
                case 0:
                    break;
                default:
                    if (single_time_pattern_for_file)
                    {
                        if (time_pattern_.empty())
                        {
                            auto time_pattern_it = separator_pattern_it;
                            auto begin_time_pattern_it = time_pattern_it;
                            auto end_time_pattern_it = time_pattern_it;

                            char s = read_pattern(time_pattern_it, log_variable_pattern_);
                            while (s != 'N' && time_pattern_it != end_pattern_it)
                            {
                                scroll_to_separator(time_pattern_it, log_variable_pattern_);
                                end_time_pattern_it = time_pattern_it;
                                s = read_pattern(time_pattern_it, log_variable_pattern_);
                            }

                            if (time_pattern_it == end_pattern_it)
                            {
                                end_time_pattern_it = time_pattern_it;
                            }

                            time_pattern_ = std::string { begin_time_pattern_it, end_time_pattern_it };
                            if (time_pattern_.empty())
                            {
                                throw std::ios_base::failure("Invalid time pattern");
                            }

                            pattern_it = end_time_pattern_it;
                        }
                        else
                        {
                            pattern_it = separator_pattern_it;
                            scroll(pattern_it, log_variable_pattern_, time_pattern_.size());
                        }

                        std::string file_time { file_it, end_file_it };
                        std::istringstream ss { file_time };
                        ss.imbue(std::locale(std::locale("C"), new time_input_facet { time_pattern_ }));
                        ss.exceptions(std::ios_base::failbit);
                        boost::posix_time::ptime pt;
                        ss >> pt;
                        scroll(file_it, file_name, ss.tellg());

                        single_time_pattern_for_file = false;
                    }
                    else
                    {
                        throw std::ios_base::failure("Double time pattern");
                    }
                }
            } while (pattern_it != end_pattern_it && file_it != end_file_it);
        }
        catch (const std::exception&)
        {
            return false;
        }
        catch (const boost::exception&)
        {
            return false;
        }

        return true;
    }

    template <typename Action>
    void base_max_files_strategy(const log_files_watchdog_config& config, Action&& act)
    {
        if (!check_pattern(config.log_variable_pattern))
            return; //always single file

        std::string time_pattern;

        using matched_files_type = std::multimap<file_sequence_id_type, bfs::path>;
        matched_files_type matched_files;
        bfs::directory_iterator it(config.dir_path), endit;
        match_context context { config.log_variable_pattern };
        while (it != endit)
        {
            auto file = *it++;

            if (!match_file(file, config, context))
                continue;

            matched_files.emplace(get_file_sequence_id(file), file);
        }

        if (matched_files.size() > config.max_files)
        {
            auto to_remove = matched_files.size() - config.max_files;
            for (auto&& item : matched_files)
            {
                if (!to_remove--)
                    break;

                act(item.second);
            }
        }
    }

} // namespace impl

log_files_watchdog_config::log_files_watchdog_config(
    const boost::filesystem::path& dir_path_,
    const std::string& log_name_,
    const std::string& log_variable_pattern_,
    const std::string& log_extension_,
    const size_t max_files_,
    const std::string& pack_command_)
    : dir_path(dir_path_)
    , log_name(log_name_)
    , log_variable_pattern(log_variable_pattern_)
    , log_extension(impl::normalize_extension(log_extension_))
    , max_files(max_files_)
    , pack_command(pack_command_)
{
    if (!impl::check_pattern(log_variable_pattern))
        throw std::logic_error("Invalid log pattern");

    if (!max_files)
        throw std::logic_error("Invalid max files value");
}

void log_files_watchdog::remove_excess_logs(const log_files_watchdog_config& config)
{
    SRV_LOGC_TRACE(SRV_FUNCTION_NAME_);

    impl::base_max_files_strategy(config, [](const path& file) {
        bfs::remove(file);
    });
}

void log_files_watchdog::pack_excess_logs(const log_files_watchdog_config& config)
{
    SRV_LOGC_TRACE(SRV_FUNCTION_NAME_);

#ifdef SERVER_LIB_PLATFORM_LINUX
    SRV_ASSERT(!config.pack_command.empty());
    SRV_ASSERT(system(NULL), "Command processor is not available");

    impl::base_max_files_strategy(config, [&config](const path& file) {
        auto compress = config.pack_command;
        compress.append(" ");
        auto path = bfs::absolute(file);
        compress.append(path.c_str());
        auto r = system(compress.c_str());
        if (r)
        {
            SRV_LOGC_WARN("Comressor return " << r << ". It is should be error code");
        }
    });
#endif
}

void log_files_watchdog::auto_watch(Strategy)
{
    // TODO
}
} // namespace server_lib
