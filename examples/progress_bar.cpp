#include <server_lib/application.h>
#include <server_lib/network/server.h>
#include <server_lib/network/protocols.h>

#include <ssl_helpers/encoding.h>

#include <server_lib/solo_timers.h>

//#define LOG_ON
#if defined(LOG_ON)
#include <server_lib/logger.h>
#endif
#include <iostream>

#include <map>

int main(void)
{
#if defined(LOG_ON)
    server_lib::logger::instance().init_debug_log();
#endif

    using namespace std::chrono_literals;
    using connection = server_lib::network::connection;
    using unit = server_lib::network::unit;

    using my_protocol = server_lib::network::msg_protocol;

    auto socket_file = server_lib::network::unix_local_server_config::preserve_socket_file();

    server_lib::solo_periodical_timer progress_timer;
    std::atomic<int> progress_left(100);

    server_lib::network::server server;
    std::mutex clients_guard;
    std::map<uint64_t, std::shared_ptr<connection>> clients;

    auto&& app = server_lib::application::init();
    return app.on_start([&]() {
                  auto notify = [&](const std::shared_ptr<connection>& client) {
                      client->send(client->protocol().create(std::to_string(100 - progress_left.load()) + "%\n"));
                  };
                  server.on_start([&, notify]() {
                            std::cout << "Online at " << socket_file << ". "
                                      << "Example to test: nc -N -U " << socket_file
                                      << ". Press ^C to stop"
                                      << "\n"
                                      << std::endl;

                            progress_timer.start(1s, [&]() {
                                // do smth., change progress
                                std::atomic_fetch_sub<int>(&progress_left, 1);

                                // notify current connections
                                std::lock_guard<std::mutex> lock(clients_guard);
                                for (auto&& item : clients)
                                {
                                    notify(item.second);
                                }

                                //stop application when 100% reached
                                if (progress_left.load() < 1)
                                    app.stop();
                            });
                        })
                      .on_new_connection([&, notify](const std::shared_ptr<connection>& conn) {
                          conn->on_disconnect([&](uint64_t conn_id) {
                              std::lock_guard<std::mutex> lock(clients_guard);
                              clients.erase(conn_id);
                          });
                          {
                              std::lock_guard<std::mutex> lock(clients_guard);
                              clients.emplace(conn->id(), conn);
                          }
                          // notify new connection to refresh it progress
                          notify(conn);
                      })
                      .on_fail([&](const std::string& e) {
                          std::cerr << "Can't start server: " << e << std::endl;
                          app.stop(1);
                      })
                      .start(server.configurate_unix_local()
                                 .set_protocol<my_protocol>()
                                 .set_socket_file(socket_file));
              })
        .on_exit([&](const int) {
            //to prevent EOF errors (for pure logs)
            server.stop(true);
        })
        .run();
}
