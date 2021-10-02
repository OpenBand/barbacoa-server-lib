#pragma once

#include <string>

namespace server_lib {

/**
 * \ingroup common
 *
 * \brief This is the set to create and manage crash dumps
 */
struct emergency_helper
{
    static bool test_file_for_write(const char*);

    //this functions async-signal-safe
    static void __print_trace_s(const char*, int out_fd);
    static bool save_raw_dump_s(const char* raw_dump_file_path);

    static std::string load_raw_dump(const char* raw_dump_file_path, bool remove = true);

    static bool save_demangled_dump(const char* raw_dump_file_path, const char* demangled_file_path);

    static std::string load_dump(const char* dump_file_path, bool remove = true);

    static std::string get_executable_name();

    static std::string create_dump_filename(const std::string& extension = "dump");
};

} // namespace server_lib
