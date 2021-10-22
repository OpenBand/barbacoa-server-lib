#include <server_lib/network/transport/client_impl_i.h>
#include <server_lib/network/client_config.h>

#include <server_lib/event_loop.h>

#include <memory>

namespace server_lib {
namespace network {
    namespace transport_layer {

        class tcp_client_impl : public client_impl_i
        {
        public:
            tcp_client_impl() = default;
            ~tcp_client_impl();

            void config(const tcp_client_config&);

            bool connect(const connect_callback_type& connect_callback,
                         const fail_callback_type& fail_callback) override;

        private:
            void stop_worker();

            event_loop _worker;

            std::unique_ptr<tcp_client_config> _config;
        };

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
