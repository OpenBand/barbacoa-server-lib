#include <server_lib/thread_local_storage.h>
#include <server_lib/asserts.h>

namespace server_lib {

thread_local_storage::thread_local_storage(const size_t sz)
    : _sz(sz)
{
    SRV_ASSERT(sz > 0);
}

thread_local_storage::buffers_content_type* thread_local_storage::obtain()
{
    const std::lock_guard<std::mutex> lock(_mutex);

    auto id = std::this_thread::get_id();

    auto it = _buffers.find(id);
    if (_buffers.end() == it)
    {
        it = _buffers.emplace(id, nullptr).first;
        it->second.reset(new buffers_content_type[size()], [](buffers_content_type* p) { delete[] p; });
    }

    return it->second.get();
}

void thread_local_storage::increase(const size_t to_add)
{
    const std::lock_guard<std::mutex> lock(_mutex);

    auto new_sz = _sz.load() + to_add;
    _sz.store(new_sz);

    auto id = std::this_thread::get_id();

    auto it = _buffers.find(id);
    if (_buffers.end() == it)
        it = _buffers.emplace(id, nullptr).first;

    it->second.reset(new buffers_content_type[size()], [](buffers_content_type* p) { delete[] p; });
}

void thread_local_storage::clear()
{
    const std::lock_guard<std::mutex> lock(_mutex);

    _buffers.clear();
}

} // namespace server_lib
