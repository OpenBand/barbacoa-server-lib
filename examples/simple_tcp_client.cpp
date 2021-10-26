#include <server_lib/application.h>
#include <server_lib/network/client.h>
#include <server_lib/network/protocols.h>
#include <server_lib/solo_timers.h>

#include <ssl_helpers/encoding.h>

//#define LOG_ON
#if defined(LOG_ON)
#include <server_lib/logger.h>
#endif
#include <iostream>

int main(void)
{
#if defined(LOG_ON)
    server_lib::logger::instance().init_debug_log();
#endif

    using namespace std::chrono_literals;
    using connection = server_lib::network::connection;
    using unit = server_lib::network::unit;

    using protocol = server_lib::network::msg_protocol;
    const std::string proto_cmd = "PING";

    server_lib::solo_periodical_timer ping_timer;

    server_lib::network::client client;
    auto&& app = server_lib::application::init();
    return app.on_start([&]() {
                  auto port = 19999;
                  client.on_connect([&](connection& conn) {
                            std::cout << "Connected to " << port << ". Press ^C to stop"
                                      << "\n"
                                      << std::endl;
                            conn.on_receive([&](connection& conn, unit& unit) {
                                auto data = unit.as_string();
                                std::cout << "Received " << data.size() << " bytes: "
                                          << ssl_helpers::to_hex(unit.as_string())
                                          << " from " << conn.remote_endpoint() << std::endl;
                            });
                            conn.on_disconnect([&]() {
                                ping_timer.stop();
                            });
                            ping_timer.start(2s, [&]() {
                                //'post' just to make all business logic in the same thread
                                client.post([&]() {
                                    conn.send(conn.protocol().create(proto_cmd));
                                });
                            });
                        })
                      .on_fail([&](const std::string& e) {
                          std::cerr << "Can't connect TCP client: " << e << std::endl;
                          app.stop(1);
                      })
                      .connect(client.configurate_tcp()
                                   .set_protocol(protocol {})
                                   .set_address(port));
              })
        .run();
}
