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

## Entry 3 - Multi-Client Concurrency

**Date:** 05 October 2026

The server was extended to support multiple simultaneous client connections using POSIX threads.

Each accepted client connection is assigned to a separate thread using `pthread_create()`. This allows the main server thread to continue accepting new clients while existing clients remain connected.

A mutex is used to protect the shared active-client counter so that concurrent threads do not update the value unsafely.

The implementation was compiled using the `-pthread` option and tested with five simultaneous client connections. The server correctly reported the active-client count from one to five.

Client disconnection was also tested. Each client thread closed its socket and reduced the active-client count correctly until it returned to zero, while the main server remained operational.

The next stage is to implement username registration, presence management and the LIST command.
