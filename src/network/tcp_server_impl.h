#include <server_lib/network/tcp_server_i.h>

#include <tacopie/tacopie>

#include <mutex>
#include <list>
#include <map>

namespace server_lib {
namespace network {

    class tcp_connection_impl;

    class tcp_server_impl : public tcp_server_i,
                            public std::enable_shared_from_this<tcp_server_impl>
    {
    public:
        tcp_server_impl() = default;

        void start(const std::string& host, uint16_t port, event_loop* callback_thread = nullptr, const on_new_connection_callback_type& callback = nullptr) override;

        void stop(bool wait_for_removal = false, bool recursive_wait_for_removal = true) override;

        bool is_running(void) const override;

        void set_nb_workers(uint8_t nb_threads) override;

    private:
        bool on_new_connection(const std::shared_ptr<tacopie::tcp_client>&);
        void on_client_disconnected(const std::shared_ptr<tacopie::tcp_client>& client);

        tacopie::tcp_server _impl;

        event_loop* _callback_thread = nullptr;
        on_new_connection_callback_type _new_connection_handler = nullptr;

        std::list<std::shared_ptr<tacopie::tcp_client>> _clients;
        std::map<std::shared_ptr<tacopie::tcp_client>, std::shared_ptr<tcp_connection_impl>> _connections;
        std::mutex _clients_mutex;
    };

} // namespace network
} // namespace server_lib
