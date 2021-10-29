#include "network_common.h"

#include <server_lib/platform_config.h>

#if defined(SERVER_LIB_PLATFORM_LINUX)
#include <sys/socket.h>
#include <netinet/ip.h>
#include <sys/types.h>
#endif

namespace server_lib {
namespace tests {

    uint16_t basic_network_fixture::get_free_port()
    {
        int result = 0;
#if defined(SERVER_LIB_PLATFORM_LINUX)
        int max_loop = 5;
        int socket_fd = -1;
        while (result < 1024 && --max_loop > 0)
        {
            struct sockaddr_in sin;
            socket_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (socket_fd == -1)
                return -1;
            sin.sin_family = AF_INET;
            sin.sin_port = 0;
            sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            if (bind(socket_fd, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) == -1)
                break;
            socklen_t len = sizeof(sin);
            if (getsockname(socket_fd, (struct sockaddr*)&sin, &len) == -1)
                break;
            result = sin.sin_port;
            close(socket_fd);
            socket_fd = -1;
        }

        if (socket_fd > 0)
            close(socket_fd);
#endif
        if (result < 1024)
            return get_default_port();
        return static_cast<uint16_t>(result);
    }

} // namespace tests
} // namespace server_lib
