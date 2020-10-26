#pragma once

#include <thread>
#include <memory>
#include <map>
#include <mutex>
#include <atomic>

namespace server_lib {

//emulation for UNIX __thread local storage
class thread_local_storage
{
    using buffers_content_type = unsigned char;
    using buffers_by_thread_type = std::map<std::thread::id, std::shared_ptr<buffers_content_type>>;

public:
    thread_local_storage() = delete;
    thread_local_storage(const size_t sz);

    size_t size() const
    {
        return _sz.load();
    }

    buffers_content_type* obtain();

    void increase(const size_t to_add);
    void clear();

private:
    std::mutex _mutex;
    buffers_by_thread_type _buffers;
    std::atomic<size_t> _sz;
};
} // namespace server_lib
