/*
 * IE3010 Network Programming
 * NetMessenger Server
 * Registration Number: IT23577206
 * Port: 13206
 * Node ID: NID:5772
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 13206
#define BACKLOG 5

int main(void)
{
    int server_socket;
    int client_socket;

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    socklen_t client_length = sizeof(client_address);

    /*
     * Step 1: Create a TCP socket.
     * AF_INET     = IPv4
     * SOCK_STREAM = TCP
     */
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    /*
     * Allows the server to restart without waiting
     * for the old socket address to be released.
     */
    int option = 1;

    if (setsockopt(server_socket,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &option,
                   sizeof(option)) < 0) {
        perror("setsockopt");
        close(server_socket);
        return EXIT_FAILURE;
    }

    /*
     * Configure the server address.
     */
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT);

    /*
     * Step 2: Bind the socket to personalised port 13206.
     */
    if (bind(server_socket,
             (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0) {
        perror("bind");
        close(server_socket);
        return EXIT_FAILURE;
    }

    /*
     * Step 3: Listen for incoming client connections.
     */
    if (listen(server_socket, BACKLOG) < 0) {
        perror("listen");
        close(server_socket);
        return EXIT_FAILURE;
    }

    printf("============================================\n");
    printf(" NetMessenger Server\n");
    printf(" Registration Number : IT23577206\n");
    printf(" Port                : %d\n", PORT);
    printf(" Node ID             : NID:5772\n");
    printf("============================================\n");
    printf("Server is listening for a client...\n");

    /*
     * Step 4: Accept one client connection.
     * Multi-client support will be added in the next stage.
     */
    client_socket = accept(server_socket,
                           (struct sockaddr *)&client_address,
                           &client_length);

    if (client_socket < 0) {
        perror("accept");
        close(server_socket);
        return EXIT_FAILURE;
    }

    printf("Client connected from %s:%d\n",
           inet_ntoa(client_address.sin_addr),
           ntohs(client_address.sin_port));

    /*
     * This first version only proves that the TCP
     * connection works successfully.
     */
    close(client_socket);
    close(server_socket);

    printf("Basic TCP connection test completed.\n");

    return EXIT_SUCCESS;
}
