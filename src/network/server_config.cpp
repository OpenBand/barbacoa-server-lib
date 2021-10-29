#include <server_lib/network/server_config.h>

#include <server_lib/platform_config.h>

#ifdef SERVER_LIB_PLATFORM_LINUX
#include <unistd.h>
#include <fcntl.h>
#endif

namespace server_lib {
namespace network {

    std::string unix_local_server_config::preserve_socket_file(const std::string& folder_path)
    {
        namespace fs = boost::filesystem;
        fs::path result(folder_path);
        if (result.empty())
        {
#ifdef SERVER_LIB_PLATFORM_LINUX
            result = "/dev/shm";
            if (!fs::is_directory(result) || access(result.generic_string().c_str(), S_IRWXO))
                result.clear();
            if (result.empty())
#endif
            {

                result = boost::filesystem::temp_directory_path();
            }
        }

        result /= boost::filesystem::unique_path();

        return result.generic_string();
    }

} // namespace network
} // namespace server_lib
