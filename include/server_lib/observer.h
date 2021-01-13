#pragma once

#include <server_lib/event_loop.h>

#include <utility>
#include <vector>
#include <mutex>

namespace server_lib {

template <class Observer>
class observable
{
    using observer_i = Observer;
    using sink_member = std::function<void(observer_i*)>;

public:
    //for observer:
    //  'sink' and 'loop' objects are required
    //  until they will be unsubscribed!
    //
    void subscribe(observer_i& sink_impl)
    {
        subscribe_impl(sink_impl, true, nullptr);
    }
    void subscribe(observer_i& sink_impl, event_loop& loop)
    {
        subscribe_impl(sink_impl, true, &loop);
    }
    void unsubscribe(observer_i& sink_impl)
    {
        subscribe_impl(sink_impl, false, nullptr);
    }

    //for observable to signal with any arguments. Exp:
    //  notify(&observer_i::on_event1)
    //  notify(&observer_i::on_event2, val1, val2)
    //
    template <typename F, typename... Arg>
    void notify(F&& f)
    {
        static_assert(std::is_member_function_pointer<F>::value,
                      "Notify function is not a member function.");

        auto memf = std::bind(f, std::placeholders::_1);
        notify_impl(memf);
    }

    template <typename F, typename... Arg>
    void notify(F&& f, const Arg&... args)
    {
        static_assert(std::is_member_function_pointer<F>::value,
                      "Notify function is not a member function.");

        auto memf = std::bind(f, std::placeholders::_1, args...); //copy arguments!
        notify_impl(memf);
    }

protected:
    void subscribe_impl(observer_i& sink, bool on, event_loop* ploop)
    {
        std::lock_guard<std::mutex> lck(_guard_for_observers);
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
        std::lock_guard<std::mutex> lck(_guard_for_observers);
        for (auto&& item : _observers)
        {
            observer_i* psink = reinterpret_cast<observer_i*>(item.first);
            if (!psink)
                continue;

            event_loop* p_el = item.second;
            if (!p_el || !p_el->is_this_loop())
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
    std::mutex _guard_for_observers;
    std::vector<std::pair<void*, event_loop*>> _observers;
};

} // namespace server_lib
