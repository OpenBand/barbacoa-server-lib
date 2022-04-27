#include <server_lib/thread_sync_helpers.h>

#include <boost/version.hpp>

#ifdef __SSE2__
#include <emmintrin.h>
namespace {
inline void __spin_loop_pause()
{
    _mm_pause();
}
} // namespace
#elif defined(_MSC_VER) && _MSC_VER >= 1800 && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
namespace {
inline void __spin_loop_pause()
{
    _mm_pause();
}
} // namespace
#else
namespace {
inline void __spin_loop_pause()
{
}
} // namespace
#endif

namespace server_lib {

void spin_loop_pause()
{
    __spin_loop_pause();
}

} // namespace server_lib
