/*
 * IE3010 Network Programming
 * NetMessenger Client
 *
 * Registration Number: IT23577206
 * Server Port: 13206
 *
 * Stage 3:
 * Keeps the client connected so that multiple
 * simultaneous connections can be tested.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT 13206

int main(void)
{
    int client_socket;

    struct sockaddr_in server_address;


    /*
     * Create TCP socket.
     */
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
    printf(" Connected to NetMessenger Server\n");
    printf(" Registration Number : IT23577206\n");
    printf(" Server Port         : %d\n", PORT);
    printf("============================================\n");

    printf("Connection is active.\n");
    printf("Press ENTER to disconnect...\n");


    /*
     * Keep this client connected until the user
     * presses Enter.
     */
    getchar();


    close(client_socket);

    printf("Disconnected from server.\n");

    return EXIT_SUCCESS;
}
