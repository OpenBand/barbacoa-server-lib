#include <server_lib/application.h>
#include <server_lib/event_loop.h>
#include <server_lib/mt_event_loop.h>

#include <server_lib/logging_helper.h>

int main(void)
{
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

            // All entities will be destroyed here.
            task1.stop();
            task2.stop();
        })
        .run();
}
