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

    while ((bytes_read = read(client_fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        if (verbose)
            printf("%s", buffer);
        write(client_fd, buffer, bytes_read);  // Echo back
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
