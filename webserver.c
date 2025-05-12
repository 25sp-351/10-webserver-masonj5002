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

void handleImages(int client_fd, char* buffer) {
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
}

void handleMath(int client_fd, char* buffer) {
    char* start         = buffer + 10;
    char* operation_end = strchr(start, '/');
    if (!operation_end) {
        const char* not_found =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "\r\n";
        write(client_fd, not_found, strlen(not_found));
        close(client_fd);
        return;
    }

    *operation_end = '\0';

    char* num1_str = operation_end + 1;
    char* num2_str = strchr(num1_str, '/');
    if (!num2_str) {
        const char* not_found =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "\r\n";
        write(client_fd, not_found, strlen(not_found));
        close(client_fd);
        return;
    }

    *num2_str = '\0';

    num2_str++;

    int num1   = atoi(num1_str);
    int num2   = atoi(num2_str);
    int result = 0;
    char response[1024];
    int response_len = 0;

    // Addition
    if (strcmp(start, "add") == 0) {
        result       = num1 + num2;
        response_len = snprintf(response, sizeof(response),
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/plain\r\n"
                                "Content-Length: %d\r\n"
                                "\r\n"
                                "%d",
                                result, result);

    }
    // Multiplication
    else if (strcmp(start, "mul") == 0) {
        result       = num1 * num2;
        response_len = snprintf(response, sizeof(response),
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/plain\r\n"
                                "Content-Length: %d\r\n"
                                "\r\n"
                                "%d",
                                result, result);
    }
    // Division
    else if (strcmp(start, "div") == 0) {
        if (num2 == 0) {
            const char* error_response =
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 15\r\n"
                "\r\n"
                "Division by zero!";
            write(client_fd, error_response, strlen(error_response));
            close(client_fd);
            return;
        }
        result       = num1 / num2;
        response_len = snprintf(response, sizeof(response),
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/plain\r\n"
                                "Content-Length: %d\r\n"
                                "\r\n"
                                "%d",
                                result, result);
    } else {
        const char* not_found =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "\r\n";
        write(client_fd, not_found, strlen(not_found));
        close(client_fd);
        return;
    }

    write(client_fd, response, response_len);
}

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

    // HELLO WORLD
    if (strncmp(buffer, "GET /hello", 10) == 0) {
        const char* response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 12\r\n"
            "\r\n"
            "Hello world!";
        write(client_fd, response, strlen(response));
    }

    else if (strncmp(buffer, "GET /static/images/", 19) == 0) {
        handleImages(client_fd, buffer);
    }

    else if (strncmp(buffer, "GET /calc/", 10) == 0) {
        handleMath(client_fd, buffer);
    }

    else {
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
