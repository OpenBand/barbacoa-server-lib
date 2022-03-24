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
template <typename Callback, typename MutexType = protected_mutex<std::mutex>>
class simple_observable
{
protected:
    using callback = Callback;
    using mutex_type = MutexType;
    using callback_storage_type = std::vector<std::pair<callback, event_loop*>>;

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
        notify_impl([this](callback sink) {
            this->call_void(sink);
        },
                    true);
    }

    template <typename... Arg>
    void notify(const Arg&... args)
    {
        notify_impl([this, args...](callback sink) {
            this->call_with_args(sink, args...);
        },
                    true);
    }

    template <typename... Arg>
    void notify_ref(Arg&... args)
    {
        // Check if notification in the same thread
        notify_impl([&](callback sink) {
            this->call_with_args(sink, args...);
        },
                    false);
    }

    // TODO: Use shared_lock for C++17 here
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
    void call_with_args(Callback_ sink, Arg&... args)
    {
        sink(args...);
    }

    template <typename Call>
    void notify_impl(Call call, bool async_allowed)
    {
        std::lock_guard<mutex_type> lck(_guard_for_observers);
        for (auto&& item : _observers)
        {
            notify_item_impl(call, async_allowed, item.first, item.second);
        }
    }

    template <typename Call>
    void notify_item_impl(Call call, bool async_allowed, callback& sink, event_loop* p_el)
    {
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

protected:
    mutable mutex_type _guard_for_observers;
    callback_storage_type _observers;
};

// Use if observable object could be destroyed in callback and
// notify is invoked rare
template <typename Callback, typename MutexType = std::mutex>
class protected_observable : public simple_observable<Callback, MutexType>
{
    using base_type = simple_observable<Callback, MutexType>;
    using callback = typename base_type::callback;
    using mutex_type = typename base_type::mutex_type;
    using callback_storage_type = typename base_type::callback_storage_type;

public:
    using base_type::base_type;

    void notify()
    {
        protected_notify_impl([this](callback sink) {
            this->call_void(sink);
        },
                              true);
    }

    template <typename... Arg>
    void notify(const Arg&... args)
    {
        protected_notify_impl([this, args...](callback sink) {
            this->call_with_args(sink, args...);
        },
                              true);
    }

    template <typename... Arg>
    void notify(Arg&... args)
    {
        // Check if notification in the same thread
        protected_notify_impl([&](callback sink) {
            this->call_with_args(sink, args...);
        },
                              false);
    }

protected:
    template <typename Call>
    void protected_notify_impl(Call call, bool async_allowed)
    {
        callback_storage_type observers;
        std::unique_lock<mutex_type> lck(this->_guard_for_observers);
        observers = this->_observers;
        lck.unlock();
        for (auto&& item : observers)
        {
            this->notify_item_impl(call, async_allowed, item.first, item.second);
        }
    }
};

} // namespace server_lib
