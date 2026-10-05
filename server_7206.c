/*
 * IE3010 Network Programming
 * NetMessenger Server
 *
 * Registration Number: IT23577206
 * Personalised Port: 13206
 * Node ID: NID:5772
 *
 * Stage 3:
 * Multi-client server using POSIX threads.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>

#define PORT 13206
#define BACKLOG 10
#define BUFFER_SIZE 1024

/*
 * Number of clients currently connected.
 * Protected by active_clients_mutex because several
 * client threads may update it at the same time.
 */
static int active_clients = 0;

static pthread_mutex_t active_clients_mutex =
    PTHREAD_MUTEX_INITIALIZER;


/*
 * Handles one connected client.
 *
 * Every client receives its own thread so that the
 * main server can continue accepting new clients.
 */
static void *handle_client(void *arg)
{
    int client_socket = *((int *)arg);

    char buffer[BUFFER_SIZE];

    free(arg);

    pthread_mutex_lock(&active_clients_mutex);

    active_clients++;

    printf("[+] Client connected. Active clients: %d\n",
           active_clients);

    pthread_mutex_unlock(&active_clients_mutex);

    /*
     * For this development stage the thread waits
     * until the client disconnects.
     *
     * The full NetMessenger command protocol will
     * be implemented in later stages.
     */
    while (1) {

        ssize_t bytes_received =
            recv(client_socket,
                 buffer,
                 sizeof(buffer) - 1,
                 0);

        if (bytes_received > 0) {

            buffer[bytes_received] = '\0';

            printf("[CLIENT DATA] %s\n", buffer);
        }
        else if (bytes_received == 0) {

            /*
             * recv() returning zero means the client
             * closed its connection normally.
             */
            break;
        }
        else {

            perror("recv");
            break;
        }
    }

    close(client_socket);

    pthread_mutex_lock(&active_clients_mutex);

    active_clients--;

    printf("[-] Client disconnected. Active clients: %d\n",
           active_clients);

    pthread_mutex_unlock(&active_clients_mutex);

    return NULL;
}


int main(void)
{
    int server_socket;

    struct sockaddr_in server_address;

    /*
     * Create an IPv4 TCP socket.
     */
    server_socket =
        socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0) {

        perror("socket");

        return EXIT_FAILURE;
    }


    /*
     * Allow the server to restart without waiting
     * for the previous socket address to expire.
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


    memset(&server_address,
           0,
           sizeof(server_address));

    server_address.sin_family = AF_INET;

    server_address.sin_addr.s_addr =
        htonl(INADDR_ANY);

    server_address.sin_port =
        htons(PORT);


    /*
     * Bind to personalised port 13206.
     */
    if (bind(server_socket,
             (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0) {

        perror("bind");

        close(server_socket);

        return EXIT_FAILURE;
    }


    /*
     * Put the server into listening mode.
     */
    if (listen(server_socket, BACKLOG) < 0) {

        perror("listen");

        close(server_socket);

        return EXIT_FAILURE;
    }


    printf("============================================\n");
    printf(" NetMessenger Multi-Client Server\n");
    printf(" Registration Number : IT23577206\n");
    printf(" Port                : %d\n", PORT);
    printf(" Node ID             : NID:5772\n");
    printf(" Concurrency         : POSIX Threads\n");
    printf("============================================\n");

    printf("Server is waiting for clients...\n");


    /*
     * Keep accepting clients continuously.
     */
    while (1) {

        struct sockaddr_in client_address;

        socklen_t client_length =
            sizeof(client_address);

        int client_socket =
            accept(server_socket,
                   (struct sockaddr *)&client_address,
                   &client_length);

        if (client_socket < 0) {

            perror("accept");

            continue;
        }


        printf("Connection from %s:%d\n",
               inet_ntoa(client_address.sin_addr),
               ntohs(client_address.sin_port));


        /*
         * Allocate the socket descriptor on the heap.
         * Each thread receives its own copy.
         */
        int *client_socket_ptr =
            malloc(sizeof(int));

        if (client_socket_ptr == NULL) {

            perror("malloc");

            close(client_socket);

            continue;
        }


        *client_socket_ptr =
            client_socket;


        pthread_t thread_id;

        /*
         * Start a new thread for this client.
         */
        if (pthread_create(&thread_id,
                           NULL,
                           handle_client,
                           client_socket_ptr) != 0) {

            perror("pthread_create");

            close(client_socket);

            free(client_socket_ptr);

            continue;
        }


        /*
         * Detached threads clean up automatically
         * when they finish.
         */
        pthread_detach(thread_id);
    }


    close(server_socket);

    return EXIT_SUCCESS;
}
