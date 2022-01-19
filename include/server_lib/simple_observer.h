#pragma once

#include <server_lib/event_loop.h>
#include <server_lib/protected_mutex.h>
#include <server_lib/asserts.h>

#include <utility>
#include <vector>

namespace server_lib {

/**
 * \ingroup common
 *
 * \brief Design patterns: Simplified Observer
 *
 * Only one sink method supported per object.
 * Unsubscribe is not supported.
 *
 */
template <typename Callback>
class simple_observable
{
    using callback = Callback;
    using mutex_type = protected_mutex<std::mutex>;

public:
    simple_observable() = default;

    simple_observable(const simple_observable& other)
    {
        std::lock_guard<mutex_type> lck(other._guard_for_observers);
        _observers = other._observers;
    }

    void subscribe(callback&& sink)
    {
        subscribe_impl(std::forward<callback>(sink), nullptr);
    }

    void subscribe(callback&& sink, event_loop& loop)
    {
        subscribe_impl(std::forward<callback>(sink), &loop);
    }

    void notify()
    {
        auto call_ = [this](callback sink) {
            this->call_void(sink);
        };
        notify_impl(call_);
    }

    template <typename... Arg>
    void notify(const Arg&... args)
    {
        auto call_ = [this, args...](callback sink) {
            this->call_with_args(sink, args...);
        };
        notify_impl(call_);
    }

    template <typename... Arg>
    void notify(Arg&&... args)
    {
        auto call_ = [&](callback sink) {
            this->call_with_args(sink, std::forward<decltype(args)>(args)...);
        };
        // Check if notification in the same thread
        notify_impl(call_, false);
    }

    bool is_any_subscriber() const
    {
        std::lock_guard<mutex_type> lck(_guard_for_observers);
        return !_observers.empty();
    }

protected:
    void subscribe_impl(callback&& sink, event_loop* ploop)
    {
        std::lock_guard<mutex_type> lck(_guard_for_observers);
        _observers.emplace_back(std::move(sink), ploop);
    }

    template <typename Callback_>
    void call_void(Callback_ sink)
    {
        sink();
    }

    template <typename Callback_, typename... Arg>
    void call_with_args(Callback_ sink, const Arg&... args)
    {
        sink(args...);
    }

    template <typename Callback_, typename... Arg>
    void call_with_args(Callback_ sink, Arg&&... args)
    {
        sink(std::forward<decltype(args)>(args)...);
    }

    template <typename Call>
    void notify_impl(Call call, bool async_allowed = true)
    {
        std::lock_guard<mutex_type> lck(_guard_for_observers);
        for (auto&& item : _observers)
        {
            callback& sink = item.first;

            event_loop* p_el = item.second;
            if (p_el && !p_el->is_this_loop())
            {
                SRV_ASSERT(async_allowed, "Async mode is not allowed");

                auto pending = [call, sink]() {
                    call(sink);
                };
                p_el->post(pending);
            }
            else
            {
                call(sink);
            }
        }
    }

private:
    mutable mutex_type _guard_for_observers;
    std::vector<std::pair<callback, event_loop*>> _observers;
};

} // namespace server_lib
