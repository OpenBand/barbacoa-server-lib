#include <server_lib/application.h>
#include <server_lib/network/web/web_server.h>

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

    namespace web = server_lib::network::web;

    using request_type = web::web_request_i;
    using response_type = web::web_server_response_i;

    web::web_server server;

    auto&& app = server_lib::application::init();
    return app.on_start([&]() {
                  auto port = 8282;
                  server.on_start([port]() {
                            std::cout << "Online at " << port << ". "
                                      << "Test example: curl 'localhost:8282/my'"
                                      << ". Press ^C to stop"
                                      << "\n"
                                      << std::endl;
                        })
                      .on_request("/my", "GET", [&](std::shared_ptr<request_type> request, std::shared_ptr<response_type> response) {
                          std::string message("You are #");
                          message += std::to_string(request->id());
                          message += ". ";
                          message += "Service has tested\n";

                          response->post(web::http_status_code::success_ok,
                                         message,
                                         { { "Content-Type", "text/plain" } });
                          response->close_connection_after_response();
                      })
                      .on_fail([&](std::shared_ptr<request_type>, const std::string& e) {
                          std::cerr << "Can't start Web server: " << e << std::endl;
                          app.stop(1);
                      })
                      .start(server.configurate().set_address(port));
              })
        .run();
}
