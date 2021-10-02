#include <server_lib/application.h>

#include <iostream>

int main(void)
{
    return server_lib::application::init().on_start([]() {
                                              std::cout << "Online. Press ^C to stop" << std::endl;
                                          })
        .run();
}
