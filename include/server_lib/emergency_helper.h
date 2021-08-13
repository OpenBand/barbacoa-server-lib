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
    static void save_dump(const char* dump_file_path);
    static std::string load_dump(const char* dump_file_path, bool remove = true);
    static bool test_for_write(const char* dump_file_path);
};
} // namespace server_lib
