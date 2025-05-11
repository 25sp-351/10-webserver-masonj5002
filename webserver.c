#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_PORT 8080
int verbose = 0;  // sorry, trying to figure out how to pass to handleConnection

void* handleConnection(void* arg) {
    int client_fd = *(int*)arg;
    free(arg);

    char buffer[1024];
    ssize_t bytes_read;

    bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        close(client_fd);
        return NULL;
    }

    buffer[bytes_read] = '\0';

    if (verbose)
        printf("%s", buffer);

    if (strncmp(buffer, "GET /hello", 10) == 0) {
        const char* response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 12\r\n"
            "\r\n"
            "Hello world!";
        write(client_fd, response, strlen(response));
    } else if (strncmp(buffer, "GET /static/images/", 19) ==
               0) {  // Only works with a file name for 19 characters
        char file_path[256] = "static/images/";
        char* start         = buffer + 19;
        char* space         = strchr(start, ' ');
        if (space &&
            (space - start) < (int)(sizeof(file_path) - strlen(file_path))) {
            strncat(file_path, start, space - start);

            if (verbose)
                printf("Looking for file: %s\n", file_path);

            FILE* fp = fopen(file_path, "rb");
            if (fp) {
                fseek(fp, 0, SEEK_END);
                long file_size = ftell(fp);
                fseek(fp, 0, SEEK_SET);

                char header[256];
                snprintf(header, sizeof(header),
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: image/jpeg\r\n"
                         "Content-Length: %ld\r\n"
                         "Connection: close\r\n"
                         "\r\n",
                         file_size);

                write(client_fd, header, strlen(header));

                char img_buf[1024];
                size_t n;
                while ((n = fread(img_buf, 1, sizeof(img_buf), fp)) > 0)
                    write(client_fd, img_buf, n);
                fclose(fp);
            } else {
                const char* not_found =
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n";
                write(client_fd, not_found, strlen(not_found));
            }
        }
    } else {
        const char* not_found =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "\r\n";
        write(client_fd, not_found, strlen(not_found));
    }

    close(client_fd);
    return NULL;
}

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;

    for (int ix = 1; ix < argc; ++ix) {
        if (strcmp(argv[ix], "-p") == 0) {
            port = atoi(argv[ix + 1]);
            ix++;
        } else if (strcmp(argv[ix], "-v") == 0) {
            verbose = 1;
        }
    }

    if (verbose)
        printf("Verbose mode enabled\n");

    fprintf(stderr, "Listening on port %d\n", port);

    int socket_fd = socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in socket_address;
    memset(&socket_address, 0, sizeof(socket_address));
    socket_address.sin_family      = AF_INET;
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
    socket_address.sin_port        = htons(port);

    if (bind(socket_fd, (struct sockaddr*)&socket_address,
             sizeof(socket_address)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(socket_fd, 5) < 0) {
        perror("listen");
        return 1;
    }

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);

        int* client_fd_buf           = malloc(sizeof(int));
        if (!client_fd_buf) {
            perror("malloc");
            continue;
        }

        *client_fd_buf = accept(socket_fd, (struct sockaddr*)&client_address,
                                &client_address_len);
        if (*client_fd_buf < 0) {
            perror("accept");
            free(client_fd_buf);
            continue;
        }

        pthread_t thread;
        pthread_create(&thread, NULL, handleConnection, client_fd_buf);
        pthread_detach(thread);  // Automatically reclaim thread resources
    }

    close(socket_fd);
    return 0;
}
