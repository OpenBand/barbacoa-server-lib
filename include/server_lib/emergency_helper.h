#pragma once

#include <string>

namespace server_lib {
namespace emergency {

    bool test_file_for_write(const char*);

    //*_s functions async-signal-safe
    bool save_raw_dump_s(const char* raw_dump_file_path);

    std::string load_raw_dump(const char* raw_dump_file_path, bool remove = true);

    bool save_demangled_dump(const char* raw_dump_file_path, const char* demangled_file_path);

    std::string load_dump(const char* dump_file_path, bool remove = true);

    std::string get_executable_name();

    std::string create_dump_filename(const std::string& extension = "dump");

} // namespace emergency
} // namespace server_lib
