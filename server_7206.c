/*
 * IE3010 Network Programming
 * NetMessenger Server
 *
 * Registration Number: IT23577206
 * Personalised Port: 13206
 * Node ID: NID:5772
 *
 * Stage 5:
 * REGISTER, LIST, BCAST, PMSG and QUIT.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>

#define PORT 13206
#define BACKLOG 10
#define MAX_CLIENTS 100
#define MAX_USERNAME 32
#define MAX_LINE 1024

typedef struct {
    int socket_fd;
    int registered;
    char username[MAX_USERNAME];

    /*
     * Protects writes to this client's socket.
     * Several server threads may send data to the
     * same client at the same time.
     */
    pthread_mutex_t send_mutex;
} Client;


static Client *clients[MAX_CLIENTS];

static pthread_mutex_t clients_mutex =
    PTHREAD_MUTEX_INITIALIZER;


/*
 * Reliably send an entire buffer.
 */
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


/*
 * Serialise messages sent to a particular client.
 */
static int send_to_client(Client *client,
                          const char *message)
{
    int result;

    pthread_mutex_lock(&client->send_mutex);

    result =
        send_all(client->socket_fd,
                 message);

    pthread_mutex_unlock(&client->send_mutex);

    return result;
}


/*
 * Read one newline-terminated protocol line.
 */
static ssize_t recv_line(int socket_fd,
                         char *buffer,
                         size_t size)
{
    size_t index = 0;

    if (size == 0) {
        return -1;
    }

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


/*
 * Add a connected client to the shared table.
 */
static int add_client(Client *client)
{
    int result = -1;

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0;
         i < MAX_CLIENTS;
         i++) {

        if (clients[i] == NULL) {

            clients[i] = client;

            result = 0;

            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    return result;
}


/*
 * Remove a disconnected client.
 */
static void remove_client(Client *client)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0;
         i < MAX_CLIENTS;
         i++) {

        if (clients[i] == client) {

            clients[i] = NULL;

            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}


/*
 * Register a unique username.
 */
static int register_username(Client *client,
                             const char *username)
{
    int result = 0;

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0;
         i < MAX_CLIENTS;
         i++) {

        if (clients[i] != NULL &&
            clients[i]->registered &&
            strcmp(clients[i]->username,
                   username) == 0) {

            result = -1;

            break;
        }
    }

    if (result == 0) {

        strncpy(client->username,
                username,
                MAX_USERNAME - 1);

        client->username[MAX_USERNAME - 1] =
            '\0';

        client->registered = 1;
    }

    pthread_mutex_unlock(&clients_mutex);

    return result;
}


/*
 * Send the currently registered users.
 */
static void send_user_list(Client *client)
{
    char response[MAX_LINE];

    strcpy(response, "OK USERS ");

    pthread_mutex_lock(&clients_mutex);

    int first = 1;

    for (int i = 0;
         i < MAX_CLIENTS;
         i++) {

        if (clients[i] != NULL &&
            clients[i]->registered) {

            if (!first) {

                strncat(
                    response,
                    ",",
                    sizeof(response) -
                    strlen(response) - 1);
            }

            strncat(
                response,
                clients[i]->username,
                sizeof(response) -
                strlen(response) - 1);

            first = 0;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    strncat(
        response,
        " NID:5772\n",
        sizeof(response) -
        strlen(response) - 1);

    send_to_client(client,
                   response);
}


/*
 * Broadcast a message to every registered client
 * except the sender.
 *
 * Protocol:
 * MSG BCAST <sender> <message>
 */
static void broadcast_message(Client *sender,
                              const char *message)
{
    char forwarded[MAX_LINE];

    snprintf(
        forwarded,
        sizeof(forwarded),
        "MSG BCAST %s %s\n",
        sender->username,
        message);


    pthread_mutex_lock(&clients_mutex);

    for (int i = 0;
         i < MAX_CLIENTS;
         i++) {

        if (clients[i] != NULL &&
            clients[i]->registered &&
            clients[i] != sender) {

            send_to_client(
                clients[i],
                forwarded);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}


/*
 * Private message.
 *
 * Returns:
 *  0 = delivered
 * -1 = target not found
 */
static int private_message(Client *sender,
                           const char *target,
                           const char *message)
{
    char forwarded[MAX_LINE];

    int result = -1;


    snprintf(
        forwarded,
        sizeof(forwarded),
        "MSG PRIV %s %s\n",
        sender->username,
        message);


    pthread_mutex_lock(&clients_mutex);


    for (int i = 0;
         i < MAX_CLIENTS;
         i++) {

        if (clients[i] != NULL &&
            clients[i]->registered &&
            strcmp(clients[i]->username,
                   target) == 0) {

            send_to_client(
                clients[i],
                forwarded);

            result = 0;

            break;
        }
    }


    pthread_mutex_unlock(&clients_mutex);

    return result;
}


/*
 * Handle one client connection.
 */
static void *handle_client(void *arg)
{
    Client *client =
        (Client *)arg;

    char line[MAX_LINE];


    printf("[+] TCP client connected.\n");


    while (1) {

        ssize_t length =
            recv_line(
                client->socket_fd,
                line,
                sizeof(line));


        if (length == 0) {
            break;
        }


        if (length < 0) {

            perror("recv");

            break;
        }


        printf("[RECEIVED from %s] %s\n",
               client->registered
                   ? client->username
                   : "unregistered",
               line);


        /*
         * REGISTER must be the first command.
         */
        if (!client->registered) {

            if (strncmp(
                    line,
                    "REGISTER ",
                    9) != 0) {

                send_to_client(
                    client,
                    "ERR 005 REGISTER_REQUIRED NID:5772\n");

                break;
            }


            const char *username =
                line + 9;


            if (strlen(username) == 0 ||
                strlen(username) >= MAX_USERNAME ||
                strchr(username, ' ') != NULL) {

                send_to_client(
                    client,
                    "ERR 006 INVALID_USERNAME NID:5772\n");

                continue;
            }


            if (register_username(
                    client,
                    username) < 0) {

                send_to_client(
                    client,
                    "ERR 001 USERNAME_TAKEN NID:5772\n");

                continue;
            }


            char response[MAX_LINE];


            snprintf(
                response,
                sizeof(response),
                "OK REGISTERED %s NID:5772\n",
                client->username);


            send_to_client(
                client,
                response);


            printf("[REGISTERED] %s\n",
                   client->username);

            continue;
        }


        /*
         * LIST
         */
        if (strcmp(line,
                   "LIST") == 0) {

            send_user_list(client);

            continue;
        }


        /*
         * BCAST <message>
         */
        if (strncmp(
                line,
                "BCAST ",
                6) == 0) {

            const char *message =
                line + 6;


            if (*message == '\0') {

                send_to_client(
                    client,
                    "ERR 007 INVALID_COMMAND NID:5772\n");

                continue;
            }


            broadcast_message(
                client,
                message);


            send_to_client(
                client,
                "OK SENT NID:5772\n");


            printf("[BCAST] %s: %s\n",
                   client->username,
                   message);

            continue;
        }


        /*
         * PMSG <username> <message>
         */
        if (strncmp(
                line,
                "PMSG ",
                5) == 0) {

            char *arguments =
                line + 5;


            char *space =
                strchr(arguments,
                       ' ');


            if (space == NULL) {

                send_to_client(
                    client,
                    "ERR 007 INVALID_COMMAND NID:5772\n");

                continue;
            }


            *space = '\0';


            const char *target =
                arguments;

            const char *message =
                space + 1;


            if (*target == '\0' ||
                *message == '\0') {

                send_to_client(
                    client,
                    "ERR 007 INVALID_COMMAND NID:5772\n");

                continue;
            }


            if (private_message(
                    client,
                    target,
                    message) < 0) {

                send_to_client(
                    client,
                    "ERR 002 USER_NOT_FOUND NID:5772\n");

                continue;
            }


            send_to_client(
                client,
                "OK SENT NID:5772\n");


            printf("[PMSG] %s -> %s: %s\n",
                   client->username,
                   target,
                   message);

            continue;
        }


        /*
         * QUIT
         */
        if (strcmp(line,
                   "QUIT") == 0) {

            send_to_client(
                client,
                "OK BYE NID:5772\n");

            break;
        }


        /*
         * Unknown or not-yet-implemented command.
         */
        send_to_client(
            client,
            "ERR 007 INVALID_COMMAND NID:5772\n");
    }


    if (client->registered) {

        printf(
            "[-] User disconnected: %s\n",
            client->username);
    }
    else {

        printf(
            "[-] Unregistered client disconnected.\n");
    }


    remove_client(client);

    close(client->socket_fd);

    pthread_mutex_destroy(
        &client->send_mutex);

    free(client);

    return NULL;
}


int main(void)
{
    int server_socket;

    struct sockaddr_in server_address;


    signal(SIGPIPE,
           SIG_IGN);


    server_socket =
        socket(
            AF_INET,
            SOCK_STREAM,
            0);


    if (server_socket < 0) {

        perror("socket");

        return EXIT_FAILURE;
    }


    int option = 1;


    if (setsockopt(
            server_socket,
            SOL_SOCKET,
            SO_REUSEADDR,
            &option,
            sizeof(option)) < 0) {

        perror("setsockopt");

        close(server_socket);

        return EXIT_FAILURE;
    }


    memset(
        &server_address,
        0,
        sizeof(server_address));


    server_address.sin_family =
        AF_INET;


    server_address.sin_addr.s_addr =
        htonl(INADDR_ANY);


    server_address.sin_port =
        htons(PORT);


    if (bind(
            server_socket,
            (struct sockaddr *)
                &server_address,
            sizeof(server_address)) < 0) {

        perror("bind");

        close(server_socket);

        return EXIT_FAILURE;
    }


    if (listen(
            server_socket,
            BACKLOG) < 0) {

        perror("listen");

        close(server_socket);

        return EXIT_FAILURE;
    }


    printf("============================================\n");
    printf(" NetMessenger Server\n");
    printf(" Registration Number : IT23577206\n");
    printf(" Port                : %d\n", PORT);
    printf(" Node ID             : NID:5772\n");
    printf(" Concurrency         : POSIX Threads\n");
    printf("============================================\n");

    printf(
        "Server is waiting for clients...\n");


    while (1) {

        struct sockaddr_in
            client_address;


        socklen_t client_length =
            sizeof(client_address);


        int client_socket =
            accept(
                server_socket,
                (struct sockaddr *)
                    &client_address,
                &client_length);


        if (client_socket < 0) {

            perror("accept");

            continue;
        }


        printf(
            "Connection from %s:%d\n",
            inet_ntoa(
                client_address.sin_addr),
            ntohs(
                client_address.sin_port));


        Client *client =
            calloc(
                1,
                sizeof(Client));


        if (client == NULL) {

            perror("calloc");

            close(client_socket);

            continue;
        }


        client->socket_fd =
            client_socket;


        client->registered =
            0;


        pthread_mutex_init(
            &client->send_mutex,
            NULL);


        if (add_client(client) < 0) {

            send_to_client(
                client,
                "ERR 008 SERVER_FULL NID:5772\n");

            close(client_socket);

            pthread_mutex_destroy(
                &client->send_mutex);

            free(client);

            continue;
        }


        pthread_t thread_id;


        if (pthread_create(
                &thread_id,
                NULL,
                handle_client,
                client) != 0) {

            perror("pthread_create");

            remove_client(client);

            close(client_socket);

            pthread_mutex_destroy(
                &client->send_mutex);

            free(client);

            continue;
        }


        pthread_detach(
            thread_id);
    }


    close(server_socket);

    return EXIT_SUCCESS;
}
