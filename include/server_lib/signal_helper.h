#pragma once

namespace server_lib {

//External daemonizer may switch off standard output,
//then server, in turn, could receive signal SIGPIPE
//for writing attempts. You could use this locker
//instead comment all cout/cerr output
class block_pipe_signal
{
public:
    block_pipe_signal(); // auto lock
    ~block_pipe_signal(); // unlock

    static void lock();
    static void unlock();
};

} // namespace server_lib
