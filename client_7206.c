/*
 * IE3010 Network Programming
 * NetMessenger Client
 *
 * Registration Number: IT23577206
 * Server Port: 13206
 *
 * Stage 5:
 * Interactive client with asynchronous receiver thread.
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

#define SERVER_IP "127.0.0.1"
#define PORT 13206
#define MAX_LINE 1024
#define MAX_FILENAME 256
#define MAX_USERNAME 32
#define MAX_FILE_SIZE (10ULL * 1024ULL * 1024ULL)


static volatile sig_atomic_t running = 1;

/*
 * Set after OK REGISTERED is received.
 * Used only for organising files received by this
 * particular client process.
 */
static char own_username[MAX_USERNAME] =
    "unregistered";


/*
 * Send an entire protocol line.
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
 * Read one newline-terminated server line.
 */
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


/*
 * Send raw binary bytes.
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
 * Receive a server-forwarded SENDFILE frame.
 *
 * Files are stored locally as:
 *
 * received_files/<recipient_username>/<filename>
 */
static int receive_forwarded_file(int socket_fd,
                                  const char *header)
{
    char target[MAX_USERNAME];
    char filename[MAX_FILENAME];

    unsigned long long filesize;


    if (sscanf(header,
               "SENDFILE %31s %255s %llu",
               target,
               filename,
               &filesize) != 3) {

        return -1;
    }


    if (filesize > MAX_FILE_SIZE ||
        strchr(filename, '/') != NULL ||
        strchr(filename, '\\') != NULL) {

        return -1;
    }


    if (ensure_directory(
            "received_files") < 0) {

        return -1;
    }


    char user_directory[512];


    snprintf(user_directory,
             sizeof(user_directory),
             "received_files/%s",
             own_username);


    if (ensure_directory(
            user_directory) < 0) {

        return -1;
    }


    char path[1024];


    snprintf(path,
             sizeof(path),
             "%s/%s",
             user_directory,
             filename);


    FILE *output =
        fopen(path, "wb");


    if (output == NULL) {

        return -1;
    }


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

            fclose(output);

            remove(path);

            return -1;
        }


        if (fwrite(buffer,
                   1,
                   (size_t)received,
                   output) !=
            (size_t)received) {

            fclose(output);

            remove(path);

            return -1;
        }


        remaining -=
            (unsigned long long)received;
    }


    fclose(output);


    printf("\nFILE RECEIVED\n");
    printf("Target : %s\n", target);
    printf("File   : %s\n", filename);
    printf("Size   : %llu bytes\n", filesize);
    printf("Saved  : %s\n", path);

    fflush(stdout);


    return 0;
}


/*
 * Process a SENDFILE command entered by the user.
 *
 * The user types the exact assignment syntax:
 *
 * SENDFILE <target> <filename> <filesize>
 */
static int send_local_file(int socket_fd,
                           const char *command_line)
{
    char target[MAX_USERNAME];
    char filename[MAX_FILENAME];
    char extra[2];

    unsigned long long declared_size;


    int count =
        sscanf(command_line,
               "SENDFILE %31s %255s %llu %1s",
               target,
               filename,
               &declared_size,
               extra);


    if (count != 3) {

        printf(
            "Usage: SENDFILE <target> <filename> <filesize>\n");

        return -1;
    }


    struct stat information;


    if (stat(filename,
             &information) < 0) {

        perror("stat");

        return -1;
    }


    if (!S_ISREG(information.st_mode)) {

        printf(
            "The specified path is not a regular file.\n");

        return -1;
    }


    unsigned long long actual_size =
        (unsigned long long)
            information.st_size;


    if (actual_size != declared_size) {

        printf(
            "File size mismatch. Actual size is %llu bytes.\n",
            actual_size);

        return -1;
    }


    FILE *input =
        fopen(filename, "rb");


    if (input == NULL) {

        perror("fopen");

        return -1;
    }


    /*
     * Send the line exactly as entered, including
     * its terminating newline.
     */
    if (send_all(socket_fd,
                 command_line) < 0) {

        fclose(input);

        return -2;
    }


    unsigned char buffer[8192];

    size_t read_count;


    while ((read_count =
                fread(buffer,
                      1,
                      sizeof(buffer),
                      input)) > 0) {

        if (send_bytes(socket_fd,
                       buffer,
                       read_count) < 0) {

            fclose(input);

            return -2;
        }
    }


    if (ferror(input)) {

        fclose(input);

        return -1;
    }


    fclose(input);


    printf(
        "Sent %llu raw file bytes.\n",
        actual_size);


    return 0;
}


/*
 * Continuously receives responses and forwarded
 * messages from the server.
 */
static void *receiver_thread(void *arg)
{
    int socket_fd =
        *((int *)arg);

    char response[MAX_LINE];


    while (running) {

        ssize_t length =
            recv_line(
                socket_fd,
                response,
                sizeof(response));


        if (length <= 0) {

            running = 0;

            break;
        }


        /*
         * Remember this client's registered username.
         */
        if (strncmp(response,
                    "OK REGISTERED ",
                    14) == 0) {

            char username[MAX_USERNAME];

            if (sscanf(response,
                       "OK REGISTERED %31s",
                       username) == 1) {

                strncpy(own_username,
                        username,
                        MAX_USERNAME - 1);

                own_username[MAX_USERNAME - 1] =
                    '\0';
            }
        }


        /*
         * A SENDFILE line from the server is followed
         * immediately by raw bytes, so consume the
         * complete file before reading another line.
         */
        if (strncmp(response,
                    "SENDFILE ",
                    9) == 0) {

            if (receive_forwarded_file(
                    socket_fd,
                    response) < 0) {

                printf(
                    "\nFile reception failed.\n");

                running = 0;

                break;
            }

            continue;
        }


        printf("\n%s\n", response);

        fflush(stdout);


        if (strncmp(
                response,
                "OK BYE ",
                7) == 0) {

            running = 0;

            break;
        }
    }


    return NULL;
}


int main(void)
{
    int client_socket;

    struct sockaddr_in
        server_address;

    char input[MAX_LINE];


    signal(SIGPIPE,
           SIG_IGN);


    client_socket =
        socket(
            AF_INET,
            SOCK_STREAM,
            0);


    if (client_socket < 0) {

        perror("socket");

        return EXIT_FAILURE;
    }


    memset(
        &server_address,
        0,
        sizeof(server_address));


    server_address.sin_family =
        AF_INET;


    server_address.sin_port =
        htons(PORT);


    if (inet_pton(
            AF_INET,
            SERVER_IP,
            &server_address.sin_addr) <= 0) {

        fprintf(
            stderr,
            "Invalid server address.\n");

        close(client_socket);

        return EXIT_FAILURE;
    }


    printf(
        "Connecting to %s:%d...\n",
        SERVER_IP,
        PORT);


    if (connect(
            client_socket,
            (struct sockaddr *)
                &server_address,
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

    printf(
        "Connected successfully.\n\n");


    printf(
        "First command must be:\n");

    printf(
        "REGISTER <username>\n\n");


    printf("Available commands:\n");
    printf("REGISTER <username>\n");
    printf("LIST\n");
    printf("BCAST <message>\n");
    printf("PMSG <username> <message>\n");
    printf("JOIN <room>\n");
    printf("LEAVE <room>\n");
    printf("ROOMS\n");
    printf("RMSG <room> <message>\n");
    printf("SENDFILE <target> <filename> <filesize>\n");
    printf("QUIT\n");


    pthread_t receiver;


    if (pthread_create(
            &receiver,
            NULL,
            receiver_thread,
            &client_socket) != 0) {

        perror("pthread_create");

        close(client_socket);

        return EXIT_FAILURE;
    }


    while (running) {

        printf("\n> ");

        fflush(stdout);


        if (fgets(
                input,
                sizeof(input),
                stdin) == NULL) {

            running = 0;

            shutdown(
                client_socket,
                SHUT_RDWR);

            break;
        }


        if (!running) {
            break;
        }


        /*
         * SENDFILE is special because its text header
         * must be followed immediately by raw bytes.
         */
        if (strncmp(input,
                    "SENDFILE ",
                    9) == 0) {

            int file_result =
                send_local_file(
                    client_socket,
                    input);

            if (file_result == -2) {

                printf(
                    "Connection lost during file transfer.\n");

                running = 0;

                break;
            }

            continue;
        }


        if (send_all(
                client_socket,
                input) < 0) {

            printf(
                "Connection lost.\n");

            running = 0;

            break;
        }


        /*
         * After QUIT the receiver thread waits for
         * OK BYE and the server closes the socket.
         */
        if (strcmp(
                input,
                "QUIT\n") == 0) {

            break;
        }
    }


    pthread_join(
        receiver,
        NULL);


    close(client_socket);


    printf(
        "Client closed.\n");


    return EXIT_SUCCESS;
}
