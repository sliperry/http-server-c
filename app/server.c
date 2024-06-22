#include "server.h"

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    printf("Logs from your program will appear here!\n");

    int server_fd, client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEADDR failed: %s\n", strerror(errno));
        exit(errno);
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr = { htonl(INADDR_ANY) },
    };

    if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s \n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("Waiting for clients to connect...\n");

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            printf("Accept failed: %s\n", strerror(errno));
            continue;
        }

        pthread_t thread_id;
        int *client_socket = malloc(sizeof(int));
        *client_socket = client_fd;

        if (pthread_create(&thread_id, NULL, handle_client, (void *)client_socket) != 0) {
            printf("Failed to create thread: %s\n", strerror(errno));
            free(client_socket);
            close(client_fd);
        } else {
            pthread_detach(thread_id);
        }
    }

    close(server_fd);

    return 0;
}

void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    free(arg);

    RequestBuffer buffer = { .read_bytes = 0 };
    Request *request = malloc(sizeof(Request));
    Response *response = malloc(sizeof(Response));

    if (request == NULL || response == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        close(client_fd);
        pthread_exit(NULL);
    }

    switch (recv(client_fd, buffer.content, BUFFER_SIZE, 0)) {
        case -1:
            response->code = HTTP_CODE_INTERNAL_SERVER_ERROR;
            strcpy(response->message, "Internal Server Error");
            send_response(client_fd, response);
            break;
        default:
            printf("%s", buffer.content);
            char *content = strdup(buffer.content);
            if (content == NULL) {
                fprintf(stderr, "Memory allocation failed for content\n");
                free(request);
                free(response);
                close(client_fd);
                pthread_exit(NULL);
            }

            if (strncmp(content, "GET", 3) == 0) {
                request->method = GET;
            } else if (strncmp(content, "POST", 4) == 0) {
                request->method = POST;
            } else {
                fprintf(stderr, "Unsupported HTTP method\n");
                free(content);
                free(request);
                free(response);
                close(client_fd);
                pthread_exit(NULL);
            }

            int path_bytes = 0;
            const char *s = content + 4;
            while (*s != ' ' && *s != '\0' && path_bytes < BUFFER_SIZE - 1) {
                request->path[path_bytes++] = *s++;
            }
            request->path[path_bytes] = '\0';

            free(content);

            if (strcmp(request->path, "/user-agent") == 0) {
                response->code = HTTP_CODE_OK;
                strcpy(response->message, "User-Agent");
            } else if (strcmp(request->path, "/") == 0) {
                response->code = HTTP_CODE_OK;
                strcpy(response->message, "OK");
            } else if (strncmp(request->path, "/echo/", 6) == 0) {
                response->code = HTTP_CODE_OK;
                strcpy(response->message, request->path + 6);
            } else {
                response->code = HTTP_CODE_NOT_FOUND;
                strcpy(response->message, "Not Found");
            }

            send_response(client_fd, response);
            break;
    }

    free(request);
    free(response);
    close(client_fd);
    pthread_exit(NULL);
}

void send_response(int client_fd, Response *response) {
    char headers[BUFFER_SIZE];
    snprintf(headers, BUFFER_SIZE,
             "HTTP/1.1 %d %s\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n",
             response->code, (response->code == HTTP_CODE_OK) ? "OK" : "Not Found", strlen(response->message));

    send(client_fd, headers, strlen(headers), 0);
    send(client_fd, response->message, strlen(response->message), 0);
}
