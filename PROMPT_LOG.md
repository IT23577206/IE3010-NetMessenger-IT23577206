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
