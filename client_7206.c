/*
 * IE3010 Network Programming
 * NetMessenger Client
 *
 * Registration Number: IT23577206
 * Server Port: 13206
 *
 * Stage 4:
 * Interactive REGISTER, LIST and QUIT testing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT 13206
#define MAX_LINE 1024


static int send_all(int socket_fd,
                    const char *message)
{
    size_t total = 0;
    size_t length = strlen(message);

    while (total < length) {

        ssize_t sent =
            send(socket_fd,
                 message + total,
                 length - total,
                 0);

        if (sent <= 0) {
            return -1;
        }

        total += (size_t)sent;
    }

    return 0;
}


static ssize_t recv_line(int socket_fd,
                         char *buffer,
                         size_t size)
{
    size_t index = 0;

    while (index < size - 1) {

        char ch;

        ssize_t received =
            recv(socket_fd,
                 &ch,
                 1,
                 0);

        if (received == 0) {
            return 0;
        }

        if (received < 0) {
            return -1;
        }

        if (ch == '\n') {

            buffer[index] = '\0';

            return (ssize_t)index;
        }

        if (ch != '\r') {

            buffer[index++] = ch;
        }
    }

    buffer[index] = '\0';

    return (ssize_t)index;
}


int main(void)
{
    int client_socket;

    struct sockaddr_in server_address;

    char input[MAX_LINE];
    char response[MAX_LINE];


    client_socket =
        socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket < 0) {

        perror("socket");

        return EXIT_FAILURE;
    }


    memset(&server_address,
           0,
           sizeof(server_address));

    server_address.sin_family =
        AF_INET;

    server_address.sin_port =
        htons(PORT);


    if (inet_pton(AF_INET,
                  SERVER_IP,
                  &server_address.sin_addr) <= 0) {

        fprintf(stderr,
                "Invalid server address.\n");

        close(client_socket);

        return EXIT_FAILURE;
    }


    printf("Connecting to %s:%d...\n",
           SERVER_IP,
           PORT);


    if (connect(client_socket,
                (struct sockaddr *)&server_address,
                sizeof(server_address)) < 0) {

        perror("connect");

        close(client_socket);

        return EXIT_FAILURE;
    }


    printf("============================================\n");
    printf(" NetMessenger Client\n");
    printf(" Registration Number : IT23577206\n");
    printf(" Server Port         : %d\n", PORT);
    printf("============================================\n");

    printf("Connected successfully.\n");
    printf("\n");

    printf("First command must be:\n");
    printf("REGISTER <username>\n");

    printf("\nAvailable Stage 4 commands:\n");
    printf("REGISTER <username>\n");
    printf("LIST\n");
    printf("QUIT\n");


    while (1) {

        printf("\n> ");

        fflush(stdout);


        if (fgets(input,
                  sizeof(input),
                  stdin) == NULL) {

            break;
        }


        /*
         * fgets() already includes the newline
         * required by the protocol.
         */
        if (send_all(client_socket,
                     input) < 0) {

            printf("Connection lost.\n");

            break;
        }


        ssize_t length =
            recv_line(client_socket,
                      response,
                      sizeof(response));


        if (length <= 0) {

            printf("Server closed the connection.\n");

            break;
        }


        printf("%s\n", response);


        if (strncmp(response,
                    "OK BYE ",
                    7) == 0) {

            break;
        }
    }


    close(client_socket);

    printf("Client closed.\n");

    return EXIT_SUCCESS;
}
