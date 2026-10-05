# NetMessenger - IE3010 Network Programming

## Student Information

- Registration Number: IT23577206
- Module: IE3010 - Network Programming
- Assignment: NetMessenger: A Multi-Client Chat and File-Sharing Platform over TCP/IP

## Personalised Configuration

- Registration Number: IT23577206
- Numeric Part: 23577206
- Last Four Digits: 7206
- Server Port: 13206
- Node ID: NID:5772
- Server Source File: server_7206.c
- Client Source File: client_7206.c
- Makefile: Makefile_7206
- Log File: netmsg_IT23577206.log
- Storage Path: ./storage/IT23577206/<sender_username>/<filename>

## Project Overview

NetMessenger is a TCP/IP based multi-client chat and file-sharing application implemented in C using the BSD sockets API.

The completed system will support:

- Multiple simultaneous clients
- Unique username registration
- Online user listing
- Broadcast messaging
- Private messaging
- Chat rooms
- Room messaging
- File sharing
- Graceful client disconnection
- Error handling
- Server-side logging

## Build Instructions

Build instructions will be added as the implementation progresses.

## Development Status

Project setup completed. TCP server and client implementation is the next development stage.

## Implementation Progress

### Stage 1 - Project Setup
Completed.

### Stage 2 - Basic TCP Connection
Completed.

The current implementation successfully:

- Creates TCP sockets using the BSD sockets API
- Binds the server to personalised port 13206
- Listens for incoming client connections
- Accepts a TCP client connection
- Connects the client to `127.0.0.1:13206`
- Compiles using the personalised Makefile
- Verifies the listening port using `ss -tlnp`

The next stage is multi-client concurrency.

### Stage 3 - Multi-Client Concurrency
Completed.

The server now:

- Accepts new client connections continuously
- Creates one POSIX thread per connected client
- Supports at least five simultaneous clients
- Uses a mutex to protect the shared active-client counter
- Detects client disconnections
- Cleans up client sockets without terminating the server

The next development stage is username registration, presence management and the LIST command.

### Stage 4 - Registration and User Listing
Completed.

Implemented and tested:

- `REGISTER <username>`
- Unique username validation
- `LIST`
- Registration-first enforcement
- `QUIT`
- Removal of disconnected users from the active-user table
- Personalised `NID:5772` appended to all OK and ERR responses

The next stage is broadcast and private messaging.

### Stage 5 - Broadcast and Private Messaging
Completed.

Implemented and tested:

- `BCAST <message>`
- `MSG BCAST <sender> <message>` forwarding
- `PMSG <username> <message>`
- `MSG PRIV <sender> <message>` forwarding
- `ERR 002 USER_NOT_FOUND NID:5772`
- Asynchronous client receiver thread
- Per-client send mutex for safe concurrent socket writes
- Broadcast delivery to multiple connected users

The next stage is room management and room messaging.

### Stage 6 - Room Management and Room Messaging
Completed.

Implemented and tested:

- `JOIN <room>`
- Automatic room creation
- `LEAVE <room>`
- `ROOMS`
- `RMSG <room> <message>`
- `MSG ROOM <room> <sender> <message>` forwarding
- Room membership validation
- Unknown-room error handling
- Non-member message rejection
- Rejoining rooms
- Automatic removal of empty rooms
- Cleanup of room membership when a client disconnects

The next development stage is TCP file sharing.
