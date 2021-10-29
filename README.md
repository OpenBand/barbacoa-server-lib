# Barbacoa Server Lib

This library based on the practice experience of writing  production applications. 
The main goal of this project is to make the process of writing server applications in C ++ **more productive**.

Usually when people say "server", they mean a network server serving incoming connections on some protocol from the TCP/IP stack. 
But the server is also a lot of stuff that you have to deal with from solution to solution. 
These are settings, logging, threads, start and stop control, and some other things that I would not want to do every time from server to server. 
Let's free a developer for specific business tasks.

# Examples

* Web server:

```cpp
    namespace web = server_lib::network::web;

    using request_type = web::web_request_i;
    using response_type = web::web_server_response_i;

    web::web_server server;

    auto&& app = server_lib::application::init();
    return app.on_start([&]() {
                  auto port = 8282;
                  server.on_start([port]() {
                            std::cout << "Online at " << port << ". "
                                      << "Example to test: curl 'localhost:8282/my'"
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
                      .on_fail([&](std::shared_ptr<request_type> request, const std::string& e) {
                          if (!request)
                          {
                              std::cerr << "Can't start Web server: " << e << std::endl;
                              app.stop(1);
                          }
                      })
                      .start(server.configurate().set_address(port));
              })
        .run();
```

* TCP server:

```cpp
    using connection = server_lib::network::connection;
    using unit = server_lib::network::unit;

    using my_protocol = server_lib::network::msg_protocol;
    const std::string protocol_message = "PING";
    const std::string protocol_message_ack = "PONG";

    server_lib::network::server server;
    auto&& app = server_lib::application::init();
    return app.on_start([&]() {
                  auto port = 19999;
                  server.on_start([port]() {
                            std::cout << "Online at " << port << ". "
                                      << "Example to test: echo -n $'\\x02\\x48\\x69'|nc localhost 19999 -N"
                                      << "\n\t or run simple_tcp_client."
                                      << "\nPress ^C to stop"
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
                                  if (protocol_message == data)
                                      conn.send(conn.protocol().create(protocol_message_ack));
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
```

* TCP client:

```cpp
    using namespace std::chrono_literals;

    using connection = server_lib::network::connection;
    using unit = server_lib::network::unit;

    using my_protocol = server_lib::network::msg_protocol;
    const std::string protocol_message = "PING";

    server_lib::solo_periodical_timer ping_timer;

    server_lib::network::client client;
    auto&& app = server_lib::application::init();
    return app.on_start([&]() {
                  auto port = 19999;
                  client.on_connect([&, port](connection& conn) {
                            std::cout << "Connected to " << port << ". Press ^C to stop"
                                      << "\n"
                                      << std::endl;
                            conn.on_receive([&](connection& conn, unit& unit) {
                                auto data = unit.as_string();
                                std::cout << "Received " << data.size() << " bytes: "
                                          << ssl_helpers::to_printable(unit.as_string())
                                          << " from " << conn.remote_endpoint() << std::endl;
                            });
                            conn.on_disconnect([&]() {
                                ping_timer.stop();
                                app.stop(0);
                            });
                            ping_timer.start(2s, [&]() {
                                //'post' just to make all business logic in the same thread
                                client.post([&]() {
                                    conn.send(conn.protocol().create(protocol_message));
                                });
                            });
                        })
                      .on_fail([&](const std::string& e) {
                          std::cerr << "Can't connect TCP client: " << e << std::endl;
                          app.stop(1);
                      })
                      .connect(client.configurate_tcp()
                                   .set_protocol<my_protocol>()
                                   .set_address(port));
              })
        .run();
```

* Threads for business and logs:

```cpp
    server_lib::logger::instance().init_debug_log();

    using namespace std::chrono_literals;

    server_lib::event_loop task1;
    server_lib::mt_event_loop task2(3);

    task1.change_thread_name("task1");
    task2.change_thread_name("task2");

    auto&& app = server_lib::application::init();
    return app
        .on_start([&]() {
            LOG_INFO("Application has started");

            task1.start()
                .post([]() {
                    LOG_DEBUG("Task (one short)");
                })
                .repeat(1s,
                        []() {
                            LOG_DEBUG("Periodical task");
                        });

            task1.post(2s, []() {
                LOG_DEBUG("Another task (one short)");
            });

            task2
                .repeat(1s, []() {
                    LOG_DEBUG("Periodical task for pull");
                })
                .repeat(500ms, []() {
                    LOG_DEBUG("Another periodical task for pull");
                })
                .post(10s, [&]() {
                    LOG_DEBUG("I'm stopping it now!");

                    app.stop();
                })
                .start();
        })
        .on_exit([&](const int) {
            LOG_INFO("Application has stopped");

            //All entities will be destroyed here
            task1.stop();
            task2.stop();
        })
        .run();
```

* Service with progress bar API (Linux only):

```cpp
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
```

# Requirements

C++14, Boost from version 1.58. 
Now it was tested on 1.74.

# Platforms

This lib tested on Ubuntu 16.04-20.04, Windows 10. But there is no any platform-specific features at this library. And one could be compiling and launching library on any platform where Boost is working good enough

# Building

Use CMake

# Features

* Thread safe signals handling
* Logging with disk space control
* Configuration from INI or command line
* Event loop
* Timers
* Client/server connections
* Crash handling (stacktrace, corefile)

# Notes

* [How to prepare binaries for corefile investigation](docs.md/how_to_prepare_binaries_for_corefile_investigation.md)

# Dependences

* Boost, OpenSSL
* [ssl-helpers](https://github.com/romualdo-bar/barbacoa-ssl-helpers.git) (OpenSSL C++ extension)
* [server-clib](https://github.com/romualdo-bar/barbacoa-server-clib.git) (Extension for POSIX based platforms only)




