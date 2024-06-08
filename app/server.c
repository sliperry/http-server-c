#include "server.h"

// Main function
int main() {
    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    // Print debug message
    printf("Logs from your program will appear here!\n");

    // Initialize server variables
    int server_fd, client_addr_len;
    struct sockaddr_in client_addr;
    RequestBuffer *buffer = create_request_buffer();
    Request *request;
    Response *response;

    // Create and configure the server socket
    server_fd = create_socket();
    configure_socket(server_fd);  
    
    // Define the server address
    struct sockaddr_in serv_addr = {  .sin_family = AF_INET ,
                                      .sin_port = htons(PORT),
                                      .sin_addr = { htonl(INADDR_ANY) },
                                    };
    
    // Bind the server socket
    if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s \n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Listen for incoming connections
    if (listen(server_fd, 5) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    printf("Waiting for a client to connect...\n");
    client_addr_len = sizeof(client_addr);

    // Infinite loop to accept and handle client connections
    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            printf("Accept failed: %s\n", strerror(errno));
            continue; // If accept fails, continue to the next iteration
        }

        printf("Client connected\n");

        // Read the client's request into the buffer
        REQUEST_BUFFER_RESULT buffer_result = read_into_request_buffer(buffer, client_fd);
		printf(buffer->content);

        switch (buffer_result) {
            case REQUEST_BUFFER_ERROR:
                // If there was an error reading the request, send an internal server error response
                response = build_internal_server_error_response();
                send_response(client_fd, response);
                free(response);
                break;
            case REQUEST_BUFFER_OK:
                // If the request was read successfully, process it
                request = serialize_request(buffer);
                response = handle_request(request);
				printf(response->message);
                send_response(client_fd, response);
                free(request);
                free(response);
                break;
        }

        // Reset the buffer for the next request
        buffer->read_bytes = 0;
        printf("Client disconnected\n");
        // Close the client connection
        close(client_fd);
    }
    
    // Close the server socket (not reachable in this infinite loop)
    close(server_fd);

    // Exit the program successfully
    exit(EXIT_SUCCESS);
}

// Function to create a request buffer
RequestBuffer *create_request_buffer() {
    RequestBuffer *buffer = malloc(sizeof(RequestBuffer));
    buffer->read_bytes = 0;
    return buffer;
}

// Function to read data into the request buffer
REQUEST_BUFFER_RESULT read_into_request_buffer(RequestBuffer *buffer, int client_fd) {
    buffer->read_bytes = recv(client_fd, buffer->content, BUFFER_SIZE, 0);
	if (buffer->read_bytes == -1) {
        return REQUEST_BUFFER_ERROR;
    }
    buffer->content[buffer->read_bytes] = '\0';
    return REQUEST_BUFFER_OK;
}

// Function to build an internal server error response
Response *build_internal_server_error_response() {
    Response *response = malloc(sizeof(Response));
    response->code = HTTP_CODE_INTERNAL_SERVER_ERROR;
    strcpy(response->message, "Internal Server Error");
    return response;
}

// Function to calculate the number of bytes until a specific character
int calc_bytes_till_char(const char *sequence, char c) {
    int count = 0;
    const char *s = sequence;
    while (*s != c) {
        count++;
        s++;
    }
    return count;
}

// Function to serialize the request
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

// Function to handle the request and generate a response
Response *handle_request(Request *request) {
    Response *response = malloc(sizeof(Response));
    if (strlen(request->path) == 0 || (strcmp(request->path, "/") == 0 && strlen(request->path) == 1)) {
        response->code = HTTP_CODE_OK;
        strcpy(response->message, "OK");
    } else {
        response->code = HTTP_CODE_NOT_FOUND;
        strcpy(response->message, "Not Found");
    }
    return response;
}

// Function to create a server socket
int create_socket() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return server_fd;
}

// Function to configure the server socket
void configure_socket(int server_fd) {
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEADDR failed: %s\n", strerror(errno));
        exit(errno);
    }
}

// Function to send a response to the client
void send_response(int client_fd, Response *response) {
    size_t resp_size = 17 + strlen(response->message);
    char *formatted_response = malloc(resp_size);
    sprintf(formatted_response, "HTTP/1.1 %d %s\r\n\r\n", response->code, response->message);
    ssize_t bytes_sent = send(client_fd, formatted_response, resp_size, 0);
    free(formatted_response);
    if (bytes_sent == -1) {
        printf("Response not sent due to error\n");
    }
}

// Function to handle errors
void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
