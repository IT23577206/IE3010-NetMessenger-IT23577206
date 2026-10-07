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
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <time.h>
#include <stdarg.h>

#define PORT 13206
#define BACKLOG 10
#define MAX_CLIENTS 100
#define MAX_USERNAME 32
#define MAX_ROOM_NAME 32
#define MAX_ROOMS 32
#define MAX_FILENAME 256
#define MAX_LINE 1024
/*
 * Optional extension: lightweight token authentication.
 * Clients must supply this token during registration.
 */
#define AUTH_TOKEN "NETMSG5772"
/*
 * The assignment defines FILE_TOO_LARGE but does not
 * prescribe a numerical limit. This implementation
 * uses a documented limit of 10 MiB.
 */
#define MAX_FILE_SIZE (10ULL * 1024ULL * 1024ULL)
/*
 * Optional extension: basic flood protection.
 * A client may send at most 10 commands within a 5-second window.
 */
#define RATE_LIMIT_COMMANDS 10
#define RATE_LIMIT_WINDOW 5
#define STORAGE_ROOT "./storage/IT23577206"

typedef struct {
    int socket_fd;
    int registered;
    char username[MAX_USERNAME];
    /* Per-client flood-protection state. */
    time_t rate_window_start;
    unsigned int rate_command_count;
    /*
     * Protects writes to this client's socket.
     * Several server threads may send data to the
     * same client at the same time.
     */
    pthread_mutex_t send_mutex;
} Client;


typedef struct {
    int active;
    char name[MAX_ROOM_NAME];
    Client *members[MAX_CLIENTS];
} Room;


static Client *clients[MAX_CLIENTS];

static pthread_mutex_t clients_mutex =
    PTHREAD_MUTEX_INITIALIZER;


static Room rooms[MAX_ROOMS];

static pthread_mutex_t rooms_mutex =
    PTHREAD_MUTEX_INITIALIZER;

/*
 * Thread-safe structured logging.
 * All server threads share the same log file, so a mutex
 * prevents multiple threads writing to it simultaneously.
 */

#define LOG_FILE "netmsg_IT23577206.log"

static pthread_mutex_t log_mutex =
    PTHREAD_MUTEX_INITIALIZER;

static void log_event(
    const char *level,
    const char *username,
    const char *format,
    ...)
{
    pthread_mutex_lock(&log_mutex);

    FILE *log_file = fopen(LOG_FILE, "a");

    if (log_file != NULL) {

        time_t now = time(NULL);
        struct tm time_info;

        localtime_r(&now, &time_info);

        char timestamp[32];

        strftime(
            timestamp,
            sizeof(timestamp),
            "%Y-%m-%d %H:%M:%S",
            &time_info);

        fprintf(
            log_file,
            "[%s] [%s] [%s] ",
            timestamp,
            level,
            username != NULL && username[0] != '\0'
                ? username
                : "unregistered");

        va_list arguments;

        va_start(arguments, format);
        vfprintf(log_file, format, arguments);
        va_end(arguments);

        fprintf(log_file, "\n");

        fclose(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}


/*
 * Graceful server shutdown state. Ctrl+C / SIGTERM closes the
 * listening socket so accept() wakes up, then main() records
 * SERVER STOP in the log before exiting.
 */
static volatile sig_atomic_t stop_requested = 0;
static volatile sig_atomic_t listening_socket = -1;

static void handle_stop_signal(int signal_number)
{
    (void)signal_number;

    stop_requested = 1;

    if (listening_socket >= 0) {
        close((int)listening_socket);
        listening_socket = -1;
    }
}


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
 * Find a room by name.
 * Caller must already hold rooms_mutex.
 */
static int find_room_locked(const char *room_name)
{
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active &&
            strcmp(rooms[i].name, room_name) == 0) {
            return i;
        }
    }

    return -1;
}


/*
 * JOIN <room>
 *
 * Creates the room if it does not already exist
 * and adds the client as a member.
 */
static int join_room(Client *client,
                     const char *room_name)
{
    pthread_mutex_lock(&rooms_mutex);

    int room_index =
        find_room_locked(room_name);

    if (room_index < 0) {

        for (int i = 0; i < MAX_ROOMS; i++) {

            if (!rooms[i].active) {

                rooms[i].active = 1;

                strncpy(rooms[i].name,
                        room_name,
                        MAX_ROOM_NAME - 1);

                rooms[i].name[MAX_ROOM_NAME - 1] =
                    '\0';

                room_index = i;

                break;
            }
        }
    }


    if (room_index < 0) {

        pthread_mutex_unlock(&rooms_mutex);

        return -1;
    }


    /*
     * If already a member, treat JOIN as successful.
     */
    for (int i = 0; i < MAX_CLIENTS; i++) {

        if (rooms[room_index].members[i] == client) {

            pthread_mutex_unlock(&rooms_mutex);

            return 0;
        }
    }


    for (int i = 0; i < MAX_CLIENTS; i++) {

        if (rooms[room_index].members[i] == NULL) {

            rooms[room_index].members[i] =
                client;

            pthread_mutex_unlock(&rooms_mutex);

            return 0;
        }
    }


    pthread_mutex_unlock(&rooms_mutex);

    return -1;
}


/*
 * LEAVE <room>
 *
 * Returns:
 *  0 = left successfully
 * -1 = room not found
 * -2 = user is not a member
 */
static int leave_room(Client *client,
                      const char *room_name)
{
    pthread_mutex_lock(&rooms_mutex);

    int room_index =
        find_room_locked(room_name);

    if (room_index < 0) {

        pthread_mutex_unlock(&rooms_mutex);

        return -1;
    }


    for (int i = 0; i < MAX_CLIENTS; i++) {

        if (rooms[room_index].members[i] == client) {

            rooms[room_index].members[i] = NULL;

            /*
             * Remove the room completely when the
             * final member leaves.
             */
            int has_members = 0;

            for (int j = 0; j < MAX_CLIENTS; j++) {

                if (rooms[room_index].members[j] != NULL) {

                    has_members = 1;

                    break;
                }
            }

            if (!has_members) {

                memset(&rooms[room_index],
                       0,
                       sizeof(Room));
            }

            pthread_mutex_unlock(&rooms_mutex);

            return 0;
        }
    }


    pthread_mutex_unlock(&rooms_mutex);

    return -2;
}


/*
 * Remove a disconnecting client from every room.
 */
static void remove_client_from_rooms(Client *client)
{
    pthread_mutex_lock(&rooms_mutex);

    for (int r = 0; r < MAX_ROOMS; r++) {

        if (!rooms[r].active) {
            continue;
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {

            if (rooms[r].members[i] == client) {

                rooms[r].members[i] = NULL;
            }
        }

        /*
         * If disconnecting the client made this room
         * empty, remove the room.
         */
        int has_members = 0;

        for (int i = 0; i < MAX_CLIENTS; i++) {

            if (rooms[r].members[i] != NULL) {

                has_members = 1;

                break;
            }
        }

        if (!has_members) {

            memset(&rooms[r],
                   0,
                   sizeof(Room));
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
}


/*
 * ROOMS
 *
 * Sends all currently created room names.
 */
static void send_room_list(Client *client)
{
    char response[MAX_LINE];

    strcpy(response, "OK ROOMS ");

    pthread_mutex_lock(&rooms_mutex);

    int first = 1;

    for (int i = 0; i < MAX_ROOMS; i++) {

        if (rooms[i].active) {

            if (!first) {

                strncat(response,
                        ",",
                        sizeof(response) -
                        strlen(response) - 1);
            }

            strncat(response,
                    rooms[i].name,
                    sizeof(response) -
                    strlen(response) - 1);

            first = 0;
        }
    }

    pthread_mutex_unlock(&rooms_mutex);


    strncat(response,
            " NID:5772\n",
            sizeof(response) -
            strlen(response) - 1);


    send_to_client(client,
                   response);
}


/*
 * RMSG <room> <message>
 *
 * Returns:
 *  0 = delivered
 * -1 = room does not exist
 * -2 = sender is not a member
 */
static int room_message(Client *sender,
                        const char *room_name,
                        const char *message)
{
    char forwarded[MAX_LINE];


    snprintf(forwarded,
             sizeof(forwarded),
             "MSG ROOM %s %s %s\n",
             room_name,
             sender->username,
             message);


    pthread_mutex_lock(&rooms_mutex);


    int room_index =
        find_room_locked(room_name);


    if (room_index < 0) {

        pthread_mutex_unlock(&rooms_mutex);

        return -1;
    }


    int sender_is_member = 0;


    for (int i = 0; i < MAX_CLIENTS; i++) {

        if (rooms[room_index].members[i] == sender) {

            sender_is_member = 1;

            break;
        }
    }


    if (!sender_is_member) {

        pthread_mutex_unlock(&rooms_mutex);

        return -2;
    }


    /*
     * Deliver only to other members of this room.
     */
    for (int i = 0; i < MAX_CLIENTS; i++) {

        Client *member =
            rooms[room_index].members[i];


        if (member != NULL &&
            member != sender &&
            member->registered) {

            send_to_client(member,
                           forwarded);
        }
    }


    pthread_mutex_unlock(&rooms_mutex);

    return 0;
}


/*
 * Send raw binary bytes.
 *
 * Unlike send_all(), this function does not use
 * strlen(), so zero bytes inside a file are safe.
 */
static int send_bytes(int socket_fd,
                      const void *data,
                      size_t length)
{
    const unsigned char *buffer =
        (const unsigned char *)data;

    size_t total = 0;

    while (total < length) {

        ssize_t sent =
            send(socket_fd,
                 buffer + total,
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
 * Create a directory if it does not already exist.
 */
static int ensure_directory(const char *path)
{
    if (mkdir(path, 0755) == 0) {
        return 0;
    }

    if (errno == EEXIST) {
        return 0;
    }

    return -1;
}


/*
 * Filenames in the line protocol are single tokens.
 * Path separators and traversal components are
 * rejected.
 */
static int valid_filename(const char *filename)
{
    if (filename == NULL ||
        *filename == '\0' ||
        strlen(filename) >= MAX_FILENAME) {

        return 0;
    }

    if (strchr(filename, '/') != NULL ||
        strchr(filename, '\\') != NULL ||
        strcmp(filename, ".") == 0 ||
        strcmp(filename, "..") == 0) {

        return 0;
    }

    return 1;
}


/*
 * Build the personalised server storage path:
 *
 * ./storage/IT23577206/<sender_username>/<filename>
 */
static int build_storage_path(const char *sender,
                              const char *filename,
                              char *output,
                              size_t output_size)
{
    char registration_directory[256];
    char sender_directory[512];

    if (ensure_directory("./storage") < 0) {
        return -1;
    }

    snprintf(registration_directory,
             sizeof(registration_directory),
             "%s",
             STORAGE_ROOT);

    if (ensure_directory(registration_directory) < 0) {
        return -1;
    }

    snprintf(sender_directory,
             sizeof(sender_directory),
             "%s/%s",
             STORAGE_ROOT,
             sender);

    if (ensure_directory(sender_directory) < 0) {
        return -1;
    }

    int written =
        snprintf(output,
                 output_size,
                 "%s/%s/%s",
                 STORAGE_ROOT,
                 sender,
                 filename);

    if (written < 0 ||
        (size_t)written >= output_size) {

        return -1;
    }

    return 0;
}


/*
 * Receive exactly filesize raw bytes from TCP and
 * write them to a file.
 */
static int receive_exact_file(int socket_fd,
                              FILE *output,
                              unsigned long long filesize)
{
    unsigned char buffer[8192];

    unsigned long long remaining =
        filesize;

    while (remaining > 0) {

        size_t wanted =
            remaining > sizeof(buffer)
                ? sizeof(buffer)
                : (size_t)remaining;

        ssize_t received =
            recv(socket_fd,
                 buffer,
                 wanted,
                 0);

        if (received <= 0) {
            return -1;
        }

        size_t written =
            fwrite(buffer,
                   1,
                   (size_t)received,
                   output);

        if (written != (size_t)received) {
            return -1;
        }

        remaining -=
            (unsigned long long)received;
    }

    return 0;
}


/*
 * Send the stored file to one recipient.
 *
 * We reuse the assignment's SENDFILE framing:
 *
 * SENDFILE <target> <filename> <filesize>\n
 * <raw bytes>
 *
 * The send mutex remains locked for both the header
 * and bytes so another message cannot be inserted
 * in the middle of the file.
 */
static int send_file_to_client(Client *recipient,
                               const char *target,
                               const char *filename,
                               unsigned long long filesize,
                               const char *stored_path)
{
    char header[MAX_LINE];

    snprintf(header,
             sizeof(header),
             "SENDFILE %s %s %llu\n",
             target,
             filename,
             filesize);

    FILE *input =
        fopen(stored_path, "rb");

    if (input == NULL) {
        return -1;
    }

    pthread_mutex_lock(
        &recipient->send_mutex);

    int result = 0;

    if (send_all(recipient->socket_fd,
                 header) < 0) {

        result = -1;
    }
    else {

        unsigned char buffer[8192];

        size_t count;

        while ((count =
                    fread(buffer,
                          1,
                          sizeof(buffer),
                          input)) > 0) {

            if (send_bytes(
                    recipient->socket_fd,
                    buffer,
                    count) < 0) {

                result = -1;

                break;
            }
        }

        if (ferror(input)) {
            result = -1;
        }
    }

    pthread_mutex_unlock(
        &recipient->send_mutex);

    fclose(input);

    return result;
}


/*
 * Deliver a stored file.
 *
 * Resolution rule:
 * 1. A registered username is checked first.
 * 2. If no user matches, an existing room is checked.
 *
 * Returns:
 *  1 = username target found
 *  2 = room target found
 *  0 = no target found
 */
static int forward_stored_file(Client *sender,
                               const char *target,
                               const char *filename,
                               unsigned long long filesize,
                               const char *stored_path)
{
    /*
     * First try target as a username.
     */
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {

        if (clients[i] != NULL &&
            clients[i]->registered &&
            strcmp(clients[i]->username,
                   target) == 0) {

            send_file_to_client(
                clients[i],
                target,
                filename,
                filesize,
                stored_path);

            pthread_mutex_unlock(
                &clients_mutex);

            return 1;
        }
    }

    pthread_mutex_unlock(
        &clients_mutex);


    /*
     * Then try target as a room.
     */
    pthread_mutex_lock(&rooms_mutex);

    int room_index =
        find_room_locked(target);

    if (room_index >= 0) {

        for (int i = 0; i < MAX_CLIENTS; i++) {

            Client *member =
                rooms[room_index].members[i];

            if (member != NULL &&
                member != sender &&
                member->registered) {

                send_file_to_client(
                    member,
                    target,
                    filename,
                    filesize,
                    stored_path);
            }
        }

        pthread_mutex_unlock(
            &rooms_mutex);

        return 2;
    }

    pthread_mutex_unlock(
        &rooms_mutex);

    return 0;
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
    log_event("CONNECT", NULL, "client thread started");


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
         * Optional extension: per-client rate limiting.
         * Prevents one client from flooding the server with commands.
         */
        time_t now = time(NULL);

        if (client->rate_window_start == 0 ||
            difftime(now, client->rate_window_start) >= RATE_LIMIT_WINDOW) {

            client->rate_window_start = now;
            client->rate_command_count = 0;
        }

        client->rate_command_count++;

        if (client->rate_command_count > RATE_LIMIT_COMMANDS) {
            send_to_client(
                client,
                "ERR 011 RATE_LIMIT NID:5772\n");

            log_event(
                "SECURITY",
                client->registered ? client->username : NULL,
                "RATE_LIMIT exceeded commands=%u window=%d",
                client->rate_command_count,
                RATE_LIMIT_WINDOW);

            continue;
        }

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

                log_event("ERROR", NULL,
                          "ERR 005 REGISTER_REQUIRED command=%s",
                          line);

                break;
            }


           char username[MAX_USERNAME];
char token[64];

if (sscanf(line + 9, "%31s %63s", username, token) != 2) {
    send_to_client(
        client,
        "ERR 012 AUTH_REQUIRED NID:5772\n");

    log_event(
        "SECURITY",
        NULL,
        "AUTH_REQUIRED invalid REGISTER format");

    continue;
}

if (strcmp(token, AUTH_TOKEN) != 0) {
    send_to_client(
        client,
        "ERR 013 AUTH_FAILED NID:5772\n");

    log_event(
        "SECURITY",
        username,
        "AUTH_FAILED invalid token");

    continue;
}

            if (strlen(username) == 0 ||
                strlen(username) >= MAX_USERNAME ||
                strchr(username, ' ') != NULL ||
                strchr(username, '/') != NULL ||
                strchr(username, '\\') != NULL) {

                send_to_client(
                    client,
                    "ERR 006 INVALID_USERNAME NID:5772\n");

                log_event("ERROR", username,
                          "ERR 006 INVALID_USERNAME");

                continue;
            }


            if (register_username(
                    client,
                    username) < 0) {

                send_to_client(
                    client,
                    "ERR 001 USERNAME_TAKEN NID:5772\n");

                log_event("ERROR", username,
                          "ERR 001 USERNAME_TAKEN");

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

            log_event("REGISTER", client->username,
                      "registration successful");

            continue;
        }


        /*
         * LIST
         */
        if (strcmp(line,
                   "LIST") == 0) {

            send_user_list(client);
            log_event("LIST", client->username,
                      "requested active user list");

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

                log_event("ERROR", client->username,
                          "ERR 007 INVALID_COMMAND command=BCAST");

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

            log_event("BCAST", client->username,
                      "message=%s", message);

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

                log_event("ERROR", client->username,
                          "ERR 007 INVALID_COMMAND command=PMSG");

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

                log_event("ERROR", client->username,
                          "ERR 007 INVALID_COMMAND command=PMSG");

                continue;
            }


            if (private_message(
                    client,
                    target,
                    message) < 0) {

                send_to_client(
                    client,
                    "ERR 002 USER_NOT_FOUND NID:5772\n");

                log_event("ERROR", client->username,
                          "ERR 002 USER_NOT_FOUND target=%s", target);

                continue;
            }


            send_to_client(
                client,
                "OK SENT NID:5772\n");


            printf("[PMSG] %s -> %s: %s\n",
                   client->username,
                   target,
                   message);

            log_event("PMSG", client->username,
                      "target=%s message=%s", target, message);

            continue;
        }


        /*
         * SENDFILE <target> <filename> <filesize>
         *
         * The command line is immediately followed
         * by exactly <filesize> raw bytes.
         */
        if (strncmp(line,
                    "SENDFILE ",
                    9) == 0) {

            char *arguments =
                line + 9;

            char *space1 =
                strchr(arguments, ' ');

            if (space1 == NULL) {

                send_to_client(
                    client,
                    "ERR 007 INVALID_COMMAND NID:5772\n");
                log_event("ERROR", client->username,
                          "ERR 007 INVALID_COMMAND command=SENDFILE");

                break;
            }

            *space1 = '\0';

            char *target =
                arguments;

            char *filename =
                space1 + 1;

            char *space2 =
                strchr(filename, ' ');

            if (space2 == NULL) {

                send_to_client(
                    client,
                    "ERR 007 INVALID_COMMAND NID:5772\n");
                log_event("ERROR", client->username,
                          "ERR 007 INVALID_COMMAND command=SENDFILE");

                break;
            }

            *space2 = '\0';

            char *size_text =
                space2 + 1;


            if (*target == '\0' ||
                !valid_filename(filename) ||
                *size_text == '\0' ||
                strchr(size_text, ' ') != NULL) {

                send_to_client(
                    client,
                    "ERR 007 INVALID_COMMAND NID:5772\n");
                log_event("ERROR", client->username,
                          "ERR 007 INVALID_COMMAND command=SENDFILE");

                break;
            }


            errno = 0;

            char *end_pointer = NULL;

            unsigned long long filesize =
                strtoull(size_text,
                         &end_pointer,
                         10);


            if (errno != 0 ||
                end_pointer == size_text ||
                *end_pointer != '\0') {

                send_to_client(
                    client,
                    "ERR 007 INVALID_COMMAND NID:5772\n");
                log_event("ERROR", client->username,
                          "ERR 007 INVALID_COMMAND command=SENDFILE");

                break;
            }


            /*
             * When a SENDFILE header is rejected,
             * the connection is closed after the
             * response because raw bytes may already
             * be waiting in the TCP stream.
             */
            if (filesize > MAX_FILE_SIZE) {

                send_to_client(
                    client,
                    "ERR 004 FILE_TOO_LARGE NID:5772\n");
                log_event("ERROR", client->username,
                          "ERR 004 FILE_TOO_LARGE target=%s file=%s size=%llu",
                          target, filename, filesize);

                break;
            }


            char storage_path[1024];


            if (build_storage_path(
                    client->username,
                    filename,
                    storage_path,
                    sizeof(storage_path)) < 0) {

                send_to_client(
                    client,
                    "ERR 011 STORAGE_ERROR NID:5772\n");
                log_event("ERROR", client->username,
                          "ERR 011 STORAGE_ERROR file=%s", filename);

                break;
            }


            FILE *output =
                fopen(storage_path, "wb");


            if (output == NULL) {

                send_to_client(
                    client,
                    "ERR 011 STORAGE_ERROR NID:5772\n");
                log_event("ERROR", client->username,
                          "ERR 011 STORAGE_ERROR file=%s", filename);

                break;
            }


            printf("[FILE] Receiving %s from %s (%llu bytes)\n",
                   filename,
                   client->username,
                   filesize);


            if (receive_exact_file(
                    client->socket_fd,
                    output,
                    filesize) < 0) {

                fclose(output);

                remove(storage_path);

                printf("[FILE] Transfer interrupted: %s\n",
                       filename);
                log_event("ERROR", client->username,
                          "interrupted SENDFILE target=%s file=%s size=%llu",
                          target, filename, filesize);

                break;
            }


            fclose(output);


            int target_result =
                forward_stored_file(
                    client,
                    target,
                    filename,
                    filesize,
                    storage_path);


            if (target_result == 0) {

                /*
                 * The protocol does not encode whether
                 * a nonexistent target was intended as
                 * a user or room. This implementation
                 * checks users first, then rooms, and
                 * uses USER_NOT_FOUND if neither exists.
                 */
                send_to_client(
                    client,
                    "ERR 002 USER_NOT_FOUND NID:5772\n");
                log_event("ERROR", client->username,
                          "ERR 002 USER_NOT_FOUND SENDFILE target=%s", target);

                continue;
            }


            char response[MAX_LINE];


            snprintf(response,
                     sizeof(response),
                     "OK FILE_RECEIVED %s NID:5772\n",
                     filename);


            send_to_client(
                client,
                response);


            printf("[FILE] %s -> %s : %s (%llu bytes)\n",
                   client->username,
                   target,
                   filename,
                   filesize);

            log_event("SENDFILE", client->username,
                      "target=%s file=%s size=%llu",
                      target, filename, filesize);

            continue;
        }


        /*
         * JOIN <room>
         */
        if (strncmp(line,
                    "JOIN ",
                    5) == 0) {

            const char *room_name =
                line + 5;


            if (*room_name == '\0' ||
                strlen(room_name) >= MAX_ROOM_NAME ||
                strchr(room_name, ' ') != NULL) {

                send_to_client(
                    client,
                    "ERR 007 INVALID_COMMAND NID:5772\n");
                log_event("ERROR", client->username,
                          "ERR 007 INVALID_COMMAND command=JOIN");

                continue;
            }


            if (join_room(client,
                          room_name) < 0) {

                send_to_client(
                    client,
                    "ERR 009 ROOM_LIMIT_REACHED NID:5772\n");
                log_event("ERROR", client->username,
                          "ERR 009 ROOM_LIMIT_REACHED room=%s", room_name);

                continue;
            }


            char response[MAX_LINE];


            snprintf(response,
                     sizeof(response),
                     "OK JOINED %s NID:5772\n",
                     room_name);


            send_to_client(client,
                           response);


            printf("[JOIN] %s -> %s\n",
                   client->username,
                   room_name);
            log_event("JOIN", client->username,
                      "room=%s", room_name);

            continue;
        }


        /*
         * LEAVE <room>
         */
        if (strncmp(line,
                    "LEAVE ",
                    6) == 0) {

            const char *room_name =
                line + 6;


            int result =
                leave_room(client,
                           room_name);


            if (result == -1) {

                send_to_client(
                    client,
                    "ERR 003 ROOM_NOT_FOUND NID:5772\n");
                log_event("ERROR", client->username,
                          "ERR 003 ROOM_NOT_FOUND room=%s", room_name);

                continue;
            }


            if (result == -2) {

                send_to_client(
                    client,
                    "ERR 010 NOT_IN_ROOM NID:5772\n");
                log_event("ERROR", client->username,
                          "ERR 010 NOT_IN_ROOM room=%s", room_name);

                continue;
            }


            char response[MAX_LINE];


            snprintf(response,
                     sizeof(response),
                     "OK LEFT %s NID:5772\n",
                     room_name);


            send_to_client(client,
                           response);


            printf("[LEAVE] %s <- %s\n",
                   client->username,
                   room_name);
            log_event("LEAVE", client->username,
                      "room=%s", room_name);

            continue;
        }


        /*
         * ROOMS
         */
        if (strcmp(line,
                   "ROOMS") == 0) {

            send_room_list(client);
            log_event("ROOMS", client->username,
                      "requested room list");

            continue;
        }


        /*
         * RMSG <room> <message>
         */
        if (strncmp(line,
                    "RMSG ",
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
                log_event("ERROR", client->username,
                          "ERR 007 INVALID_COMMAND command=RMSG");

                continue;
            }


            *space = '\0';


            const char *room_name =
                arguments;

            const char *message =
                space + 1;


            if (*room_name == '\0' ||
                *message == '\0') {

                send_to_client(
                    client,
                    "ERR 007 INVALID_COMMAND NID:5772\n");
                log_event("ERROR", client->username,
                          "ERR 007 INVALID_COMMAND command=RMSG");

                continue;
            }


            int result =
                room_message(client,
                             room_name,
                             message);


            if (result == -1) {

                send_to_client(
                    client,
                    "ERR 003 ROOM_NOT_FOUND NID:5772\n");
                log_event("ERROR", client->username,
                          "ERR 003 ROOM_NOT_FOUND room=%s", room_name);

                continue;
            }


            if (result == -2) {

                send_to_client(
                    client,
                    "ERR 010 NOT_IN_ROOM NID:5772\n");
                log_event("ERROR", client->username,
                          "ERR 010 NOT_IN_ROOM room=%s", room_name);

                continue;
            }


            send_to_client(
                client,
                "OK SENT NID:5772\n");


            printf("[RMSG] %s -> %s: %s\n",
                   client->username,
                   room_name,
                   message);
            log_event("RMSG", client->username,
                      "room=%s message=%s", room_name, message);

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
            log_event("QUIT", client->username,
                      "client requested QUIT");

            break;
        }


        /*
         * Unknown or not-yet-implemented command.
         */
        send_to_client(
            client,
            "ERR 007 INVALID_COMMAND NID:5772\n");
        log_event("ERROR", client->username,
                  "ERR 007 INVALID_COMMAND command=%s", line);
    }


    if (client->registered) {

        printf(
            "[-] User disconnected: %s\n",
            client->username);
        log_event("DISCONNECT", client->username,
                  "client disconnected");
    }
    else {

        printf(
            "[-] Unregistered client disconnected.\n");
        log_event("DISCONNECT", NULL,
                  "unregistered client disconnected");
    }


    remove_client_from_rooms(client);

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

    struct sigaction stop_action;
    memset(&stop_action, 0, sizeof(stop_action));
    stop_action.sa_handler = handle_stop_signal;
    sigemptyset(&stop_action.sa_mask);
    stop_action.sa_flags = 0;

    sigaction(SIGINT, &stop_action, NULL);
    sigaction(SIGTERM, &stop_action, NULL);


    server_socket =
        socket(
            AF_INET,
            SOCK_STREAM,
            0);


    if (server_socket < 0) {

        perror("socket");

        return EXIT_FAILURE;
    }

    listening_socket = server_socket;


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

    log_event("SERVER", NULL,
              "START port=%d node=NID:5772", PORT);


    while (!stop_requested) {

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

            if (stop_requested) {
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            perror("accept");

            continue;
        }


        printf(
            "Connection from %s:%d\n",
            inet_ntoa(
                client_address.sin_addr),
            ntohs(
                client_address.sin_port));

        log_event("CONNECT", NULL,
                  "peer=%s:%d",
                  inet_ntoa(client_address.sin_addr),
                  ntohs(client_address.sin_port));


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
            log_event("ERROR", NULL,
                      "ERR 008 SERVER_FULL");

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
            log_event("ERROR", NULL,
                      "pthread_create failed: %s", strerror(errno));

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


    if (listening_socket >= 0) {
        close((int)listening_socket);
        listening_socket = -1;
    }

    log_event("SERVER", NULL, "STOP");

    return EXIT_SUCCESS;
}
