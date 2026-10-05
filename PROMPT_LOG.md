# AI Prompt Log

## Interaction 1

**Date:** 04 October 2026  
**Tool:** ChatGPT

**Prompt / Purpose:**  
Asked ChatGPT to explain the IE3010 NetMessenger assignment in simple terms and provide a step-by-step implementation approach.

**How the output was used:**  
The explanation was used to understand the assignment requirements and divide the implementation into manageable stages: project setup, basic TCP connection, multi-client handling, registration, messaging, rooms, file transfer, logging, error handling, testing and documentation.

**Evaluation / Changes Made:**  
The guidance was checked against the official assignment specification. Personalised values were calculated using registration number IT23577206, and a thread-per-client architecture was selected as the planned concurrency approach. Implementation code will be tested and understood before inclusion in the final submission.

## Interaction 2

**Date:** 04 October 2026  
**Tool:** ChatGPT

**Prompt / Purpose:**  
Requested step-by-step guidance to implement and test the first TCP server and client using C and the personalised port 13206.

**How the output was used:**  
Implemented the basic server in `server_7206.c`, the basic client in `client_7206.c`, and GCC build rules in `Makefile_7206`.

**Evaluation / Changes Made:**  
The code was compiled and tested in CentOS. The client successfully connected to the server at `127.0.0.1:13206`. The listening port was independently verified using `ss -tlnp`.

## Interaction 3

**Date:** 05 October 2026
**Tool:** ChatGPT

**Prompt / Purpose:**
Requested step-by-step guidance for extending the NetMessenger TCP server to support at least five clients simultaneously.

**How the output was used:**
The server was updated to use POSIX threads so that each connected client is handled independently. The Makefile was updated with the `-pthread` compiler option, and the client was temporarily configured to remain connected until Enter was pressed.

**Evaluation / Changes Made:**
The implementation was compiled and tested in CentOS. Five clients connected simultaneously to port 13206. The connections were verified using `ss -tnp`, and all clients were disconnected individually to confirm correct thread and socket cleanup.

## Interaction 4

**Date:** 05 October 2026
**Tool:** ChatGPT

**Prompt / Purpose:**
Requested guidance for implementing REGISTER, unique username checking, LIST, registration-first enforcement and QUIT handling.

**How the output was used:**
The server was extended with a shared client table containing connection and username information. REGISTER and LIST were implemented according to the assignment protocol, with personalised NID responses.

**Evaluation / Changes Made:**
The implementation was compiled and tested using multiple clients. Duplicate usernames correctly returned `ERR 001 USERNAME_TAKEN NID:5772`, LIST before registration returned `ERR 005 REGISTER_REQUIRED NID:5772`, and QUIT removed the user from the active-user list.

## Interaction 5

**Date:** 05 October 2026
**Tool:** ChatGPT

**Prompt / Purpose:**
Requested guidance for implementing broadcast messaging, private messaging and asynchronous message reception.

**How the output was used:**
The server was extended with BCAST and PMSG handling. The client was upgraded with a receiver thread so that forwarded messages can arrive while the user is typing commands.

**Evaluation / Changes Made:**
The implementation was compiled and tested with multiple registered clients. Broadcast messages were delivered to all other clients, private messages were delivered only to the intended user, and unknown targets correctly returned `ERR 002 USER_NOT_FOUND NID:5772`. Existing REGISTER and LIST behaviour was also retested successfully.

## Interaction 6

**Date:** 05 October 2026
**Tool:** ChatGPT

**Prompt / Purpose:**
Requested step-by-step guidance for implementing room creation, membership tracking, room listing, room messaging and empty-room cleanup.

**How the output was used:**
The server was extended with room data structures and handlers for JOIN, LEAVE, ROOMS and RMSG. The client command menu was also updated with the new room commands.

**Evaluation / Changes Made:**
The implementation was compiled and tested with multiple clients. Users successfully joined different rooms, messages were delivered only to room members, non-members were rejected, unknown rooms returned an error, users could leave and rejoin rooms, and empty rooms were automatically removed. Existing LIST and PMSG functionality was retested successfully.

## Interaction 7

**Date:** 05 October 2026
**Tool:** ChatGPT

**Prompt / Purpose:**
Requested continued step-by-step guidance for implementing TCP file sharing to individual users and rooms, including raw-byte transfer, storage, validation and robustness testing.

**How the output was used:**
The server and client were extended to support SENDFILE. The implementation receives the exact declared number of bytes, stores a personalised server-side copy, forwards files to users or room members, and stores received files on recipient clients.

**Evaluation / Changes Made:**
Testing covered user-to-user transfer, room transfer, SHA-256 integrity, unknown targets, oversized files and interrupted transfers. The original file, server copy and recipient copies were verified as identical.
## Interaction 8

**Date:** 05 October 2026
**Tool:** ChatGPT

**Prompt / Purpose:**
Requested step-by-step guidance for implementing structured server-side logging, robustness testing, and final multi-client integration testing for the NetMessenger application.

**How the output was used:**
The server was extended with a thread-safe logging mechanism that writes timestamped protocol events to `netmsg_IT23577206.log`. Logging was incorporated for server start and stop, connections, registration, LIST, broadcast messages, private messages, room operations, file transfers, QUIT, disconnects, and protocol errors.

The implementation was tested using invalid users, unknown rooms, non-member room messaging, duplicate usernames, and commands before registration. The generated log was inspected to verify that errors and normal protocol activity were recorded correctly.

A final integration test was also carried out with five simultaneous clients: nathasha, fernando, anton, amal and rehan. Broadcast messaging, private messaging, room isolation, SENDFile transfer, client removal after QUIT, and the active-user LIST were verified.

**Evaluation / Changes Made:**
The logging functionality worked without affecting the previously implemented networking features. The server and client compiled successfully, the five-client test completed successfully, and the final log contained timestamped evidence of both normal operations and error conditions.
