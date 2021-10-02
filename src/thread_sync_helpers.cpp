#include <server_lib/thread_sync_helpers.h>

#include <moonlight/web/utility.hpp>

namespace server_lib {

void spin_loop_pause()
{
    SimpleWeb::spin_loop_pause();
}

} // namespace server_lib
