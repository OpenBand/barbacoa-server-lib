#pragma once

#include <server_lib/event_loop.h>
#include <server_lib/protected_mutex.h>

#include <utility>
#include <vector>

namespace server_lib {

/**
 * \ingroup common
 *
 * \brief Design patterns: Observer
 *
 *  For observer:
 * 'sink' and 'event_loop' objects are required
 *  until they will be unsubscribed.
 *  See 'subscribe' and 'notify'
 */
template <class Observer>
class observable
{
    using observer_i = Observer;
    using sink_member = std::function<void(observer_i*)>;
    using mutex_type = protected_mutex<std::mutex>;

public:
    /**
     * @brief Subscribe
     *
     * @param sink
     */
    void subscribe(observer_i& sink)
    {
        subscribe_impl(sink, true, nullptr);
    }

    /**
     * @brief Subscribe and it will post events to event_loop
     *
     * @param sink
     */
    void subscribe(observer_i& sink, event_loop& loop)
    {
        subscribe_impl(sink, true, &loop);
    }
    void unsubscribe(observer_i& sink)
    {
        subscribe_impl(sink, false, nullptr);
    }

    /**
     * @brief Notify
     *
     * For observable to signal without arguments:
     * \code{.c}
     * o.notify(&observer_i::on_foo_event);
     * \endcode
     */
    template <typename F>
    void notify(F&& f)
    {
        static_assert(std::is_member_function_pointer<F>::value,
                      "Notify function is not a member function.");

        auto memf = std::bind(f, std::placeholders::_1);
        notify_impl(memf);
    }

    /**
     * @brief Notify
     *
     * For observable to signal with any arguments:
     * \code{.c}
     * o.notify(&observer_i::on_foo_event, val1, val2);
     * \endcode
     */
    template <typename F, typename... Arg>
    void notify(F&& f, const Arg&... args)
    {
        static_assert(std::is_member_function_pointer<F>::value,
                      "Notify function is not a member function.");

        auto memf = std::bind(f, std::placeholders::_1, args...);
        notify_impl(memf);
    }

protected:
    void subscribe_impl(observer_i& sink, bool on, event_loop* ploop)
    {
        std::lock_guard<mutex_type> lck(_guard_for_observers);
        void* psink = (void*)(&sink);
        size_t i = 0;
        for (auto&& item : _observers)
        {
            if (item.first == psink)
            {
                if (!on)
                {
                    auto empty_p = std::make_pair<void*, event_loop*>(nullptr, nullptr);
                    _observers[i] = std::move(empty_p);
                }
                break;
            }
            ++i;
        }
        if (on && i == _observers.size())
        {
            _observers.emplace_back(psink, ploop);
        }
    }

    void notify_impl(const sink_member& memf)
    {
        std::lock_guard<mutex_type> lck(_guard_for_observers);
        for (auto&& item : _observers)
        {
            observer_i* psink = reinterpret_cast<observer_i*>(item.first);
            if (!psink)
                continue;

            event_loop* p_el = item.second;
            if (p_el && !p_el->is_this_loop())
            {
                auto pending = [memf, psink]() {
                    memf(psink);
                };
                p_el->post(pending);
            }
            else
                memf(psink);
        }
    }

private:
    mutex_type _guard_for_observers;
    std::vector<std::pair<void*, event_loop*>> _observers;
};

} // namespace server_lib
