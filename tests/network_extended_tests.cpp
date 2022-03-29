#include "network_common.h"

#include <server_lib/event_loop.h>
#include <server_lib/logging_helper.h>

#include <server_lib/network/server.h>
#include <server_lib/network/client.h>
#include <server_lib/network/protocols.h>

#include <ssl_helpers/encoding.h>

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <queue>

namespace server_lib {
namespace tests {

    using namespace server_lib::network;
    using namespace std::chrono_literals;

    BOOST_FIXTURE_TEST_SUITE(network_tests, basic_network_fixture)

    BOOST_AUTO_TEST_CASE(tcp_proxy_check)
    {
        print_current_test_name();

        const std::string ping_cmd = "Hellow server!";
        const std::string pong_cmd = "Me too";

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        auto print_unit = [](const unit& unit) -> std::string {
            return ssl_helpers::to_printable(unit.to_printable_string());
        };

        // Setup App server
        //

        server app_server;

        std::string host = get_default_address();
        auto port = get_free_port();
        decltype(port) proxy_port;

        msg_protocol app_protocol;
        pconnection server_connection;

        auto server_recieve_callback = [&](pconnection pconn, unit unit) {
            LOG_TRACE("********* server_on_receive");

            LOG_TRACE("***** recieve unit: " << print_unit(unit));

            BOOST_REQUIRE_EQUAL(unit.as_string(), ping_cmd);

            BOOST_REQUIRE_NO_THROW(server_connection->send(pong_cmd));
        };

        app_server.on_start([&]() {
                      LOG_TRACE("********* server started at port = " << port);

                      proxy_port = get_free_port();
                      if (proxy_port == port)
                          proxy_port++;

                      // Move to the next test step
                      std::unique_lock<std::mutex> lck(done_test_cond_guard);
                      done_test = true;
                      done_test_cond.notify_one();
                  })
            .on_new_connection([&](pconnection pconn) {
                BOOST_REQUIRE(pconn);

                LOG_TRACE("********* server_new_connection_callback: " << pconn->id());

                server_connection = pconn;

                server_connection->on_receive(server_recieve_callback)
                    .on_disconnect([&]() {
                        LOG_TRACE("********* server_connection_on_disconnect");
                    });
            })
            .start(app_server.configurate_tcp()
                       .set_worker_name("!S-T")
                       .set_address(host, port)
                       .set_protocol(app_protocol));

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));

        done_test = false;

        // Setup proxy
        //

        event_loop proxy_th;

        proxy_th.change_thread_name("!P");

        client proxy_client;
        server proxy_server;

        raw_protocol proxy_protocol;
        pconnection proxy_in_connection, proxy_out_connection;
        std::queue<unit> proxy_in_buffer;
        std::mutex proxy_in_buffer_guard;

        auto release_buffer = [&]() {
            BOOST_REQUIRE(proxy_out_connection);

            std::lock_guard<std::mutex> lck(proxy_in_buffer_guard);
            while (!proxy_in_buffer.empty())
            {
                LOG_TRACE("*** proxy release_buffer = " << proxy_in_buffer.size());

                auto unit_to_send_next = proxy_in_buffer.front();

                LOG_TRACE("***** proxy unit: " << print_unit(unit_to_send_next));

                proxy_out_connection->send(unit_to_send_next);
                proxy_in_buffer.pop();
            }
        };

        proxy_th.on_start([&]() {
                    LOG_TRACE("********* proxy_start");

                    proxy_server.on_start([&]() {
                                    LOG_TRACE("********* proxy started at port = " << proxy_port);

                                    // Move to the next test step
                                    std::unique_lock<std::mutex> lck(done_test_cond_guard);
                                    done_test = true;
                                    done_test_cond.notify_one();
                                })
                        .on_new_connection([&](pconnection pconn) {
                            BOOST_REQUIRE(pconn);

                            LOG_TRACE("********* proxy_server_new_connection_callback: " << pconn->id());

                            proxy_in_connection = pconn;

                            proxy_in_connection->on_receive([&](pconnection, unit unit) {
                                                   proxy_th.post([&, unit]() {
                                                       LOG_TRACE("********* proxy_server_on_receive");
                                                       if (proxy_out_connection)
                                                       {
                                                           release_buffer();

                                                           LOG_TRACE("*** proxy send out");

                                                           LOG_TRACE("***** proxy unit: " << print_unit(unit));

                                                           proxy_out_connection->send(unit);
                                                       }
                                                       else
                                                       {
                                                           LOG_TRACE("*** proxy push_buffer");

                                                           std::lock_guard<std::mutex> lck(proxy_in_buffer_guard);
                                                           proxy_in_buffer.push(unit);
                                                       }
                                                   });
                                               })
                                .on_disconnect([&]() {
                                    LOG_TRACE("********* proxy_server_on_disconnect");
                                });

                            BOOST_REQUIRE(proxy_client.on_connect([&](pconnection pconn) {
                                                          LOG_TRACE("********* proxy_client_on_connect");

                                                          BOOST_REQUIRE(pconn);

                                                          proxy_out_connection = pconn;

                                                          proxy_out_connection->on_receive([&](pconnection pconn, unit unit_) {
                                                                                  proxy_th.post([&, unit = unit_]() {
                                                                                      LOG_TRACE("********* proxy_client_on_receive");

                                                                                      BOOST_REQUIRE(proxy_in_connection);

                                                                                      LOG_TRACE("*** proxy send in");

                                                                                      LOG_TRACE("***** proxy unit: " << print_unit(unit));

                                                                                      proxy_in_connection->send(unit);
                                                                                  });
                                                                              })
                                                              .on_disconnect([&]() {
                                                                  LOG_TRACE("********* proxy_client_on_disconnect");
                                                              });

                                                          proxy_th.post([&]() {
                                                              LOG_TRACE("********* proxy release_buffer");

                                                              release_buffer();
                                                          });
                                                      })
                                              .connect(
                                                  proxy_client.configurate_tcp()
                                                      .set_worker_name("!C-P-T")
                                                      .set_address(host, port)
                                                      .set_protocol(proxy_protocol)));
                        })
                        .start(proxy_server.configurate_tcp()
                                   .set_worker_name("!S-P-T")
                                   .set_address(host, proxy_port)
                                   .set_protocol(proxy_protocol));
                })
            .start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));

        done_test = false;

        // Setup App client
        //

        client client;

        BOOST_REQUIRE(client.on_connect([&](pconnection pconn) {
                                LOG_TRACE("********* client_on_connect");

                                pconn->on_receive([&](pconnection pconn, unit unit) {
                                    LOG_TRACE("********* client_on_receive");

                                    LOG_TRACE("***** recieve unit: " << print_unit(unit));

                                    BOOST_REQUIRE_EQUAL(unit.as_string(), pong_cmd);

                                    // Finish test
                                    std::unique_lock<std::mutex> lck(done_test_cond_guard);
                                    done_test = true;
                                    done_test_cond.notify_one();
                                });

                                BOOST_REQUIRE_NO_THROW(pconn->send(ping_cmd));
                            })
                          .connect(
                              client.configurate_tcp()
                                  .set_worker_name("!C-T")
                                  .set_address(host, proxy_port)
                                  .set_protocol(app_protocol)));

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_CASE(tcp_multyresult_check)
    {
        print_current_test_name();

        bool done_test = false;
        std::mutex done_test_cond_guard;
        std::condition_variable done_test_cond;

        const std::string task_a_id = "A";
        const std::string task_b_id = "B";
        const std::string result_postfix = " result";

        // Setup App server
        //

        server server;

        std::string host = get_default_address();
        auto port = get_free_port();

        event_loop processor_a;
        event_loop processor_b;

        processor_a.change_thread_name(task_a_id);
        processor_b.change_thread_name(task_b_id);

        msg_protocol app_protocol;
        pconnection server_connection;

        processor_a.on_start([&]() {
                       LOG_TRACE("********* server processor A started");

                       // Move to the next test step
                       std::unique_lock<std::mutex> lck(done_test_cond_guard);
                       done_test = true;
                       done_test_cond.notify_one();
                   })
            .start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));

        done_test = false;

        processor_b.on_start([&]() {
                       LOG_TRACE("********* server processor B started");

                       // Move to the next test step
                       std::unique_lock<std::mutex> lck(done_test_cond_guard);
                       done_test = true;
                       done_test_cond.notify_one();
                   })
            .start();

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));

        done_test = false;

        server.on_start([&]() {
                  LOG_TRACE("********* server started at port = " << port);

                  // Move to the next test step
                  std::unique_lock<std::mutex> lck(done_test_cond_guard);
                  done_test = true;
                  done_test_cond.notify_one();
              })
            .on_new_connection([&](pconnection pconn) {
                BOOST_REQUIRE(pconn);

                LOG_TRACE("********* server_new_connection_callback: " << pconn->id());

                server_connection = pconn;

                server_connection->on_receive([&](pconnection pconn, unit unit) {
                                     LOG_TRACE("********* server_on_receive - for processor A");

                                     BOOST_REQUIRE(unit.is_string());

                                     processor_a.post([&]() {
                                         LOG_TRACE("***** processor A job");

                                         std::this_thread::sleep_for(120ms);

                                         server_connection->send(task_a_id + result_postfix);
                                     });
                                 })
                    .on_receive([&](pconnection pconn, unit unit) {
                        LOG_TRACE("********* server_on_receive - for processor B");

                        BOOST_REQUIRE(unit.is_string());

                        processor_b.post([&]() {
                            LOG_TRACE("***** processor B job");

                            std::this_thread::sleep_for(60ms);

                            server_connection->send(task_b_id + result_postfix);
                        });
                    });
            })
            .start(server.configurate_tcp()
                       .set_worker_name("!S-T")
                       .set_address(host, port)
                       .set_protocol(app_protocol));

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));

        done_test = false;

        client client;

        std::set<std::string> expected_results = { task_a_id + result_postfix, task_b_id + result_postfix };

        BOOST_REQUIRE(client.on_connect([&](pconnection pconn) {
                                LOG_TRACE("********* client_on_connect");

                                pconn->on_receive([&](pconnection pconn, unit unit) {
                                    LOG_TRACE("********* client_on_receive");

                                    LOG_TRACE("***** recieve unit: " << unit.to_printable_string());

                                    expected_results.erase(unit.as_string());

                                    if (expected_results.empty())
                                    {
                                        // Finish test
                                        std::unique_lock<std::mutex> lck(done_test_cond_guard);
                                        done_test = true;
                                        done_test_cond.notify_one();
                                    }
                                });

                                BOOST_REQUIRE_NO_THROW(pconn->send(std::string("some task")));
                            })
                          .connect(
                              client.configurate_tcp()
                                  .set_worker_name("!C-T")
                                  .set_address(host, port)
                                  .set_protocol(app_protocol)));

        BOOST_REQUIRE(waiting_for_asynch_test(done_test, done_test_cond, done_test_cond_guard));
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace server_lib
