#include "server.h"

// Main function
int main() {
    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    // Print statements for debugging
    printf("Logs from your program will appear here!\n");

    int server_fd, client_addr_len;
    struct sockaddr_in client_addr;

    RequestBuffer *buffer = malloc(sizeof(RequestBuffer));
    buffer->read_bytes = 0;

    Request *request;
    Response *response;

    // Create and configure the server socket
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
        // Accept client connections in a loop
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            printf("Accept failed: %s\n", strerror(errno));
            continue;
        }

        printf("Client connected\n");

        // Read data from the client
        REQUEST_BUFFER_RESULT buffer_result = read_into_request_buffer(buffer, client_fd);
        switch (buffer_result) {
            case REQUEST_BUFFER_ERROR:
                response = build_internal_server_error_response();
                send_response(client_fd, response);
                free(response);
                break;
            case REQUEST_BUFFER_OK:
                request = serialize_request(buffer);
                response = handle_request(request);
                send_response(client_fd, response);
                free(request);
                free(response);
                break;
        }

        printf("Here is the text: %c",response->message);

        buffer->read_bytes = 0;
        printf("Client disconnected\n");
        close(client_fd);
    }  

    close(server_fd);
    free(buffer);

    exit(EXIT_SUCCESS);
}



REQUEST_BUFFER_RESULT read_into_request_buffer(RequestBuffer *buffer, int client_fd) {
    buffer->read_bytes = recv(client_fd, buffer->content, BUFFER_SIZE, 0);
    if (buffer->read_bytes == -1) {
        return REQUEST_BUFFER_ERROR;
    }
    buffer->content[buffer->read_bytes] = '\0';
    printf("Received content from client:\n%s\n", buffer->content); // Improved logging
    return REQUEST_BUFFER_OK;
}

Response *build_internal_server_error_response() {
    Response *response = malloc(sizeof(Response));
    response->code = HTTP_CODE_INTERNAL_SERVER_ERROR;
    strcpy(response->message, "Internal Server Error");
    return response;
}

int calc_bytes_till_char(const char *sequence, char c) {
    int count = 0;
    const char *s = sequence;
    while (*s != c && *s != '\0') { // Ensure not to read beyond the string
        count++;
        s++;
    }
    return count;
}

Request *serialize_request(RequestBuffer *buffer) {
    Request *request = malloc(sizeof(Request));
    char *content = strdup(buffer->content);

    if (strncmp(content, "GET", 3) == 0) {
        request->method = GET;
    } else if (strncmp(content, "POST", 4) == 0) {
        request->method = POST;
    }

    int path_bytes = calc_bytes_till_char(content + 4, ' ');
    strncpy(request->path, content + 4, path_bytes);
    request->path[path_bytes] = '\0';

    free(content);
    return request;
}

Response *handle_request(Request *request) {
    Response *response = malloc(sizeof(Response));

    if (strlen(request->path) == 0 || (strcmp(request->path, "/") == 0 && strlen(request->path) == 1)) {
        response->code = HTTP_CODE_OK;
        strcpy(response->message, "OK");
    } else if (strncmp(request->path, "/echo/", 6) == 0) {
        response->code = HTTP_CODE_OK;
        strcpy(response->message, request->path + 6);  // Extract the string after "/echo/"
    } else {
        response->code = HTTP_CODE_NOT_FOUND;
        strcpy(response->message, "Not Found");
    }

    return response;
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