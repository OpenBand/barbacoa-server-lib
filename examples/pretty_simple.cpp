#include <server_lib/application.h>
#include <server_lib/network/server.h>
#include <server_lib/network/protocols.h>
#include <ssl_helpers/encoding.h>

#include <iostream>

int main(void)
{
    using connection = server_lib::network::connection;
    using unit = server_lib::network::unit;
    using raw_protocol = server_lib::network::raw_protocol;
    server_lib::network::server server;
    auto&& app = server_lib::application::init();
    return app.on_start([&]() {
                  auto port = 19999;
                  server.on_new_connection([](const std::shared_ptr<connection>& conn) {
                            conn->on_receive([](connection& conn, unit& unit) {
                                auto data = unit.as_string();
                                std::cout << "Received " << data.size() << " bytes: "
                                          << ssl_helpers::to_hex(unit.as_string())
                                          << " from " << conn.remote_endpoint() << std::endl;
                            });
                        })
                      .on_fail([&](const std::string& e) {
                          std::cerr << "Can't start TCP server: " << e << std::endl;
                          app.stop(1);
                      })
                      .start(server.configurate_tcp()
                                 .set_protocol(raw_protocol {})
                                 .set_address(port));
                  std::cout << "Online at " << port << ". Press ^C to stop" << std::endl;
              })
        .run();
}
