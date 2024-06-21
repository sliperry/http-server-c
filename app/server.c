#include "server.h"

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    printf("Logs from your program will appear here!\n");

    int server_fd, client_fd, client_addr_len;
    struct sockaddr_in client_addr;

    RequestBuffer *buffer = malloc(sizeof(RequestBuffer));
    buffer->read_bytes = 0;

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

    printf("Waiting for a client to connect...\n");
    client_addr_len = sizeof(client_addr);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            printf("Accept failed: %s\n", strerror(errno));
            continue;
        }

        printf("Client connected\n");

        Request *request = malloc(sizeof(Request));
        if (request == NULL) {
            fprintf(stderr, "Memory allocation failed for request\n");
            close(client_fd);
            exit(EXIT_FAILURE);
        }

        Response *response = malloc(sizeof(Response));
        if (response == NULL) {
            fprintf(stderr, "Memory allocation failed for response\n");
            free(request);
            close(client_fd);
            exit(EXIT_FAILURE);
        }

        char *content 

        switch (recv(client_fd, buffer->content, BUFFER_SIZE, 0)) {
            case -1:
                response->code = HTTP_CODE_INTERNAL_SERVER_ERROR;
                strcpy(response->message, "Internal Server Error");
                send_response(client_fd, response);
                free(response);
                break;
            default:
                
                 // Duplicate the content from the request buffer
                content = strdup(buffer->content);
                if (content == NULL) {
                    fprintf(stderr, "Memory allocation failed for content\n");
                    free(request); // Free allocated memory for request before returning
                    exit(EXIT_FAILURE);
                }

                // Extract and set the HTTP method
                if (strncmp(content, "GET", 3) == 0) {
                    request->method = GET;
                } else if (strncmp(content, "POST", 4) == 0) {
                    request->method = POST;
                } else {
                    fprintf(stderr, "Unsupported HTTP method\n");
                    free(content); // Free allocated memory for content before returning
                    free(request); // Free allocated memory for request before returning
                    exit(EXIT_FAILURE);
                }

                // Calculate the length of the path
                int path_bytes = 0;
                const char *s = content + 4;
                while (*s != ' ' && *s != '\0') { // Ensure not to read beyond the string
                    path_bytes++;
                    s++;
                }

                if (path_bytes < 0 || path_bytes >= BUFFER_SIZE) {
                    fprintf(stderr, "Invalid path length\n");
                    free(content); // Free allocated memory for content before returning
                    free(request); // Free allocated memory for request before returning
                    exit(EXIT_FAILURE);
                }

                // Copy the path and null-terminate it
                strncpy(request->path, content + 4, path_bytes);
                request->path[path_bytes] = '\0';

                // Extract User-Agent from headers
                char *start, *end;
                if ((start = strstr(content, "User-Agent:")) != NULL) {
                    start += strlen("User-Agent:"); // Move the pointer to the end of "User-Agent:"
                    while (*start == ' ') start++;  // Skip any leading spaces

                    // Find the end of the User-Agent string
                    end = strpbrk(start, "\r\n");
                    if (end) {
                        *end = '\0'; // Null-terminate the User-Agent string at the first newline or carriage return
                    }

                    strncpy(request->user_agent, start, sizeof(request->user_agent) - 1);
                    request->user_agent[sizeof(request->user_agent) - 1] = '\0'; // Ensure null-termination

                    printf("Extracted User-Agent: '%s' with length %zu\n", request->user_agent, strlen(request->user_agent));
                } else {
                    strcpy(request->user_agent, "Unknown");
                }
                // Free allocated memory for content since it's no longer needed
                free(content);
                
                if (strcmp(request->path, "/user-agent") == 0) {
                    response->code = HTTP_CODE_OK;
                    strcpy(response->message, request->user_agent);
                } else if (strlen(request->path) == 0 || (strcmp(request->path, "/") == 0 && strlen(request->path) == 1)) {
                    response->code = HTTP_CODE_OK;
                    strcpy(response->message, "OK");
                }else if (strncmp(request->path, "/echo/", 6) == 0) {
                    response->code = HTTP_CODE_OK;
                    strcpy(response->message, request->path + 6);  // Extract the string after "/echo/"
                } else {
                    response->code = HTTP_CODE_NOT_FOUND;
                    strcpy(response->message, "Not Found");
                }

                send_response(client_fd, response);
                free(request);
                free(response);
                break;
        }

        buffer->read_bytes = 0;
        printf("Client disconnected\n");
        close(client_fd);
    }

    close(server_fd);
    free(buffer);

    return 0;
}

Request *serialize_request(RequestBuffer *buffer) {
    // Allocate memory for the request struct
    Request *request = malloc(sizeof(Request));
    if (request == NULL) {
        fprintf(stderr, "Memory allocation failed for request\n");
        return NULL;
    }

    // Duplicate the content from the request buffer
    char *content = strdup(buffer->content);
    if (content == NULL) {
        fprintf(stderr, "Memory allocation failed for content\n");
        free(request); // Free allocated memory for request before returning
        return NULL;
    }

    // Extract and set the HTTP method
    if (strncmp(content, "GET", 3) == 0) {
        request->method = GET;
    } else if (strncmp(content, "POST", 4) == 0) {
        request->method = POST;
    } else {
        fprintf(stderr, "Unsupported HTTP method\n");
        free(content); // Free allocated memory for content before returning
        free(request); // Free allocated memory for request before returning
        return NULL;
    }

    // Calculate the length of the path
    int path_bytes = 0;
    const char *s = content + 4;
    while (*s != ' ' && *s != '\0') { // Ensure not to read beyond the string
        path_bytes++;
        s++;
    }

    if (path_bytes < 0 || path_bytes >= BUFFER_SIZE) {
        fprintf(stderr, "Invalid path length\n");
        free(content); // Free allocated memory for content before returning
        free(request); // Free allocated memory for request before returning
        return NULL;
    }

    // Copy the path and null-terminate it
    strncpy(request->path, content + 4, path_bytes);
    request->path[path_bytes] = '\0';

    // Extract User-Agent from headers
    char *start, *end;
    if ((start = strstr(content, "User-Agent:")) != NULL) {
        start += strlen("User-Agent:"); // Move the pointer to the end of "User-Agent:"
        while (*start == ' ') start++;  // Skip any leading spaces

        // Find the end of the User-Agent string
        end = strpbrk(start, "\r\n");
        if (end) {
            *end = '\0'; // Null-terminate the User-Agent string at the first newline or carriage return
        }

        strncpy(request->user_agent, start, sizeof(request->user_agent) - 1);
        request->user_agent[sizeof(request->user_agent) - 1] = '\0'; // Ensure null-termination

        printf("Extracted User-Agent: '%s' with length %zu\n", request->user_agent, strlen(request->user_agent));
    } else {
        strcpy(request->user_agent, "Unknown");
    }
    // Free allocated memory for content since it's no longer needed
    free(content);

    // Return the populated request struct
    return request;
}

void send_response(int client_fd, Response *response) {
    char headers[BUFFER_SIZE];
    snprintf(headers, BUFFER_SIZE,
             "HTTP/1.1 %d %s\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n",
             response->code, (response->code == HTTP_CODE_OK) ? "OK" : "Not Found", strlen(response->message));
    
    int bytes_sent = send(client_fd, headers, strlen(headers), 0);
    bytes_sent = send(client_fd, response->message, strlen(response->message), 0);

    if (bytes_sent == -1) {
        printf("Response not sent due to error\n");
    }
}