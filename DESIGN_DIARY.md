# NetMessenger Design Diary

## Entry 1 - Initial Project Planning

**Date:** 04 October 2026

The NetMessenger assignment requirements were reviewed and the project was divided into incremental development stages.

A GitHub repository named `IE3010-NetMessenger-IT23577206` was created and cloned into the CentOS Linux virtual machine.

The implementation will use C and the BSD sockets API as required by the assignment.

A thread-per-client concurrency model is planned for the server because it provides a clear way to handle multiple clients simultaneously while keeping each client connection independent.

The personalised values calculated from registration number IT23577206 are:

- Port: 13206
- Node ID: NID:5772
- Server file: server_7206.c
- Client file: client_7206.c
- Makefile: Makefile_7206
- Log file: netmsg_IT23577206.log
- Storage base path: ./storage/IT23577206/

The next step is to implement and test a basic TCP connection between one server and one client.

## Entry 2 - Basic TCP Client/Server Connection

**Date:** 04 October 2026

A working TCP client/server connection was implemented and tested.

The server creates an IPv4 TCP socket using `socket()`, binds it to the personalised port 13206, places the socket into listening mode using `listen()`, and accepts a client connection using `accept()`.

The client creates a TCP socket and connects to the server through the loopback address `127.0.0.1` on port 13206.

Both programs compiled successfully using GCC. The server was also verified using `ss -tlnp`, which confirmed that `server_7206` was listening on the correct personalised port 13206.

This version handles one client only. The next stage will introduce multi-client concurrency using POSIX threads.
