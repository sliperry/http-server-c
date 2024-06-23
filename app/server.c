#include "server.h"


char *files_directory = NULL;

int main(int argc, char *argv[]) {
    parse_args(argc, argv);
    if (files_directory == NULL) {
        fprintf(stderr, "No directory specified. Use --directory flag.\n");
        exit(EXIT_FAILURE);
    }

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

            char *start = strstr(content, "User-Agent:");
            if (start != NULL) {
                start += strlen("User-Agent:");
                while (*start == ' ') start++;
                char *end = strpbrk(start, "\r\n");
                if (end) {
                    *end = '\0';
                }
                strncpy(request->user_agent, start, sizeof(request->user_agent) - 1);
                request->user_agent[sizeof(request->user_agent) - 1] = '\0';
                printf("Extracted User-Agent: '%s' with length %zu\n", request->user_agent, strlen(request->user_agent));
            } else {
                strcpy(request->user_agent, "Unknown");
            }

            free(content);

            if (strncmp(request->path, "/files/", 7) == 0) {
                char filepath[BUFFER_SIZE];
                snprintf(filepath, BUFFER_SIZE, "%s/%s", files_directory, request->path + 7);

                struct stat st;
                if (stat(filepath, &st) == 0) {
                    FILE *file = fopen(filepath, "rb");
                    if (file) {
                        fseek(file, 0, SEEK_END);
                        size_t file_size = ftell(file);
                        fseek(file, 0, SEEK_SET);

                        response->code = HTTP_CODE_OK;
                        fread(response->message, 1, file_size, file);
                        response->message[file_size] = '\0';

                        fclose(file);

                        snprintf(response->message, BUFFER_SIZE, "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %zu\r\n\r\n", file_size);
                        send(client_fd, response->message, strlen(response->message), 0);
                        send(client_fd, response->message, file_size, 0);
                    } else {
                        response->code = HTTP_CODE_INTERNAL_SERVER_ERROR;
                        strcpy(response->message, "Internal Server Error");
                        send_response(client_fd, response);
                    }
                } else {
                    response->code = HTTP_CODE_NOT_FOUND;
                    strcpy(response->message, "Not Found");
                    send_response(client_fd, response);
                }
            } else if (strcmp(request->path, "/user-agent") == 0) {
                response->code = HTTP_CODE_OK;
                strcpy(response->message, request->user_agent);
                send_response(client_fd, response);
            } else if (strcmp(request->path, "/") == 0) {
                response->code = HTTP_CODE_OK;
                strcpy(response->message, "OK");
                send_response(client_fd, response);
            } else if (strncmp(request->path, "/echo/", 6) == 0) {
                response->code = HTTP_CODE_OK;
                strcpy(response->message, request->path + 6);
                send_response(client_fd, response);
            } else {
                response->code = HTTP_CODE_NOT_FOUND;
                strcpy(response->message, "Not Found");
                send_response(client_fd, response);
            }
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

void parse_args(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--directory") == 0 && i + 1 < argc) {
            files_directory = argv[i + 1];
        }
    }
}