#pragma once

#include <boost/program_options.hpp>
#include <boost/filesystem/path.hpp>

#include <string>

namespace server_lib {

namespace bpo = boost::program_options;

class server_options
{
protected:
    server_options() = default;

public:
    virtual ~server_options() {}

    using path = boost::filesystem::path;

    virtual const char* get_application_name() const;

    virtual const char* get_application_capitalized_name() const
    {
        return nullptr;
    }

    virtual const char* get_application_version() const;

    virtual const char* get_config_file_name() const;

    virtual void print_version(std::ostream& stream) const;

    virtual void print_options(std::ostream& stream, const bpo::options_description& options) const;

    path get_config_file_path(const bpo::variables_map& options, const char* opt_name) const;

    static void create_config_file_if_not_exist(std::ostream& stream,
                                                const path& config_ini_path,
                                                const bpo::options_description& cfg_options);

    static void load_config_file(std::ostream& stream,
                                 const path& config_ini_path,
                                 const bpo::options_description& cfg_options,
                                 bpo::variables_map& options);

    static std::string remove_quotes(const std::string& with_quotes);
    static std::wstring remove_quotes(const std::wstring& with_quotes);

    static std::string get_approximate_relative_time_string(const int64_t& event_time,
                                                            const int64_t& relative_to_time,
                                                            const std::string& default_ago = " ago");
};

} // namespace server_lib
