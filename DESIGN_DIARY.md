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

## Entry 4 - User Registration and Presence List

**Date:** 05 October 2026

The NetMessenger protocol was extended with username registration, active-user tracking, the LIST command and clean QUIT handling.

The server now stores connected clients in a shared client table containing the socket descriptor, registration state and username. Access to this shared table is protected using a mutex.

REGISTER is enforced as the first command on a new connection. Usernames must be unique, and duplicate username attempts return `ERR 001 USERNAME_TAKEN NID:5772`.

The LIST command returns the currently registered users as a comma-separated list followed by the personalised Node ID tag.

Testing confirmed successful registration of multiple users, rejection of duplicate usernames, rejection of LIST before registration, and correct removal of a user from LIST after QUIT.

The next stage is broadcast and private messaging.

## Entry 5 - Broadcast and Private Messaging

**Date:** 05 October 2026

Broadcast and private messaging were added to the NetMessenger protocol.

The BCAST command forwards a message to every registered client except the sender using the required format `MSG BCAST <sender> <message>`. The sender receives `OK SENT NID:5772`.

The PMSG command sends a message only to the specified registered user using the required format `MSG PRIV <sender> <message>`. If the target username does not exist, the server returns `ERR 002 USER_NOT_FOUND NID:5772`.

The client was upgraded with a dedicated receiver thread so that incoming messages can be displayed asynchronously while the main thread continues accepting keyboard input.

Each server-side client structure also contains a send mutex so concurrent server threads cannot interleave messages written to the same client socket.

Testing confirmed broadcast delivery to multiple clients, private messaging in both directions, correct unknown-user handling, and preservation of the existing LIST functionality.

The next stage is room creation, room membership and room messaging.

## Entry 6 - Room Management and Room Messaging

**Date:** 05 October 2026

Room functionality was added to the NetMessenger server.

The server now supports `JOIN <room>`, `LEAVE <room>`, `ROOMS` and `RMSG <room> <message>`.

Rooms are stored in a shared room table containing the room name and a list of member client pointers. Access to room data is protected using a mutex because multiple client threads may join, leave or send room messages concurrently.

The JOIN command creates a room automatically when it does not already exist and adds the requesting user as a member. LEAVE removes the user from the room, and empty rooms are automatically deleted.

The ROOMS command lists all currently active rooms. RMSG forwards a room message only to other members of the specified room.

Testing confirmed successful room creation, room listing, room-only message delivery, rejection of messages from non-members, rejection of unknown rooms, successful leave and rejoin behaviour, and automatic removal of empty rooms.

Existing LIST and private messaging functionality were also retested successfully after the room implementation.

The next stage is TCP file sharing.

## Entry 7 - TCP File Sharing

**Date:** 05 October 2026

TCP file sharing was added to NetMessenger using the `SENDFILE <target> <filename> <filesize>` protocol.

After the textual SENDFILE header, the client transmits exactly the declared number of raw file bytes. The server reads the file in a loop because TCP may divide the data across multiple recv() operations.

The server stores each successfully received file under the personalised path:

`./storage/IT23577206/<sender_username>/<filename>`

Files can be sent either to an individual registered user or to a room. For room transfers, the file is forwarded to the other members of that room.

The receiving client reads exactly the advertised filesize and stores the received file locally under `received_files/<username>/`.

A maximum file size of 10 MiB was used as an implementation assumption because the assignment defines the FILE_TOO_LARGE error but does not specify a numerical limit.

Testing confirmed successful user-to-user transfer, room transfer to multiple recipients, byte-for-byte integrity using SHA-256 and cmp, unknown-target handling, oversized-file rejection, and deletion of incomplete files following an interrupted transfer.

The next development stage is structured logging, robustness and final integration testing.
