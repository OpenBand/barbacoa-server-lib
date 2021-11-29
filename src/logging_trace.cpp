#include <server_lib/platform_config.h>
#include <server_lib/asserts.h>
#include <server_clib/macro.h>

namespace server_lib {

namespace {
    // There is no pretty formatting functions (xprintf) that are async-signal-safe
    void __print_trace_s(char* buff, size_t sz, const char* text, int out_fd)
    {
        memset(buff, 0, sz);
        int ln = SRV_C_MIN(sz - 1, strnlen(text, sz));
        strncpy(buff, text, ln);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
        write(out_fd, buff, strnlen(buff, sz));
#pragma GCC diagnostic pop
    }
} // namespace

void print_trace_s(const char* text, int out_fd)
{
    char buff[1024];

    __print_trace_s(buff, sizeof(buff), text, out_fd);
}

} // namespace server_lib
