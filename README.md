# Barbacoa Server Lib

This library based on the experience of writing the really working live applications. 
The main goal of this project is to make the process of writing server applications in C ++ **more productive**.

Usually when people say "server", they mean a network server serving incoming connections on some protocol from the TCP/IP stack. 
But the server is also a lot of stuff that you have to deal with from solution to solution. 
These are settings, logging, threads, start and stop control, and some other things that I would not want to do every time from server to server. 
Let's free a developer for specific business tasks.

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
* [moonlight-web-server](https://github.com/romualdo-bar/moonlight-web-server.git) (HTTP)
* [tacopie](https://github.com/romualdo-bar/tacopie.git) (TCP)
* [ssl-helpers](https://github.com/romualdo-bar/barbacoa-ssl-helpers.git) (OpenSSL C++ extension)
* [server-clib](https://github.com/romualdo-bar/barbacoa-server-clib.git) (POSIX C lib)




