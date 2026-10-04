/*
 * IE3010 Network Programming
 * NetMessenger Client
 * Registration Number: IT23577206
 * Server Port: 13206
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
     * Step 1: Create a TCP socket.
     */
    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    /*
     * Configure the server address.
     */
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    if (inet_pton(AF_INET,
                  SERVER_IP,
                  &server_address.sin_addr) <= 0) {
        perror("inet_pton");
        close(client_socket);
        return EXIT_FAILURE;
    }

    printf("Connecting to NetMessenger server %s:%d...\n",
           SERVER_IP,
           PORT);

    /*
     * Step 2: Connect to the server.
     */
    if (connect(client_socket,
                (struct sockaddr *)&server_address,
                sizeof(server_address)) < 0) {
        perror("connect");
        close(client_socket);
        return EXIT_FAILURE;
    }

    printf("Connected successfully to NetMessenger server.\n");
    printf("Registration Number: IT23577206\n");
    printf("Server Port: %d\n", PORT);

    /*
     * Later stages will add REGISTER, LIST,
     * messaging, rooms and file sharing.
     */
    close(client_socket);

    printf("Basic TCP connection test completed.\n");

    return EXIT_SUCCESS;
}
