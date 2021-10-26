#include <server_lib/application.h>
#include <server_lib/network/server.h>
#include <server_lib/network/protocols.h>

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

    using connection = server_lib::network::connection;
    using unit = server_lib::network::unit;

    using protocol = server_lib::network::msg_protocol;
    const std::string proto_cmd = "PING";
    const std::string proto_ask = "PONG";

    server_lib::network::server server;
    auto&& app = server_lib::application::init();
    return app.on_start([&]() {
                  auto port = 19999;
                  server.on_start([port]() {
                            std::cout << "Online at " << port << ". Press ^C to stop"
                                      << "\n"
                                      << std::endl;
                        })
                      .on_new_connection([&](const std::shared_ptr<connection>& conn) {
                          conn->on_receive([&](connection& conn, unit& unit) {
                                  auto data = unit.as_string();
                                  std::cout << "#" << conn.id() << " "
                                            << "(" << conn.remote_endpoint() << ") - "
                                            << "received " << data.size() << " bytes: "
                                            << ssl_helpers::to_printable(unit.as_string())
                                            << std::endl;
                                  if (proto_cmd == data)
                                      conn.send(conn.protocol().create(proto_ask));
                              })
                              .on_disconnect([&](size_t conn_id) {
                                  std::cout << "#" << conn_id << " - "
                                            << " disconnected"
                                            << "\n"
                                            << std::endl;
                              });
                      })
                      .on_fail([&](const std::string& e) {
                          std::cerr << "Can't start TCP server: " << e << std::endl;
                          app.stop(1);
                      })
                      .start(server.configurate_tcp()
                                 .set_protocol(protocol {})
                                 .set_address(port));
              })
        .run();
}
