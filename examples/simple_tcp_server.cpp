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

    using server_lib::network::connection;
    using server_lib::network::pconnection;
    using server_lib::network::unit;

    using my_protocol = server_lib::network::msg_protocol;
    const std::string protocol_message = "PING";
    const std::string protocol_message_ack = "PONG";

    server_lib::network::server server;
    auto&& app = server_lib::application::init();
    return app.on_start([&]() {
                  auto port = 19999;
                  server.on_start([port]() {
                            std::cout << "Online at " << port << ". "
                                      << "Example to test: "
                                      << "echo -n $'\\x02\\x48\\x69'|nc localhost 19999 -N"
                                      << "\n\t or run simple_tcp_client."
                                      << "\nPress ^C to stop"
                                      << "\n"
                                      << std::endl;
                        })
                      .on_new_connection([&](const pconnection& pconn) {
                          pconn->on_receive([&](const pconnection& pconn, unit& unit) {
                                   auto data = unit.as_string();
                                   std::cout << "#" << pconn->id() << " "
                                             << "(" << pconn->remote_endpoint() << ") - "
                                             << "received " << data.size() << " bytes: "
                                             << ssl_helpers::to_printable(unit.as_string())
                                             << std::endl;
                                   if (protocol_message == data)
                                       pconn->send(pconn->protocol().create(protocol_message_ack));
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
                                 .set_protocol<my_protocol>()
                                 .set_address(port));
              })
        .run();
}
