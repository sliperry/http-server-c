#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

// Define buffer sizes and port
#define BUFFER_SIZE 2048
#define PORT 4221

// Define HTTP methods and response codes
typedef enum {
    GET,
    POST
} HTTP_METHOD;

typedef enum {
    HTTP_CODE_OK = 200,
    HTTP_CODE_NOT_FOUND = 404,
    HTTP_CODE_INTERNAL_SERVER_ERROR = 500
} HTTP_CODE;

typedef enum { 
    REQUEST_BUFFER_ERROR, 
    REQUEST_BUFFER_OK 
} REQUEST_BUFFER_RESULT;

// Struct for request buffer
typedef struct {
    char content[BUFFER_SIZE];
    int read_bytes;
} RequestBuffer;

// Struct for HTTP request
typedef struct {
    HTTP_METHOD method;
    char path[BUFFER_SIZE];
    char user_agent[BUFFER_SIZE]; // New field to store User-Agent
} Request;

// Struct for HTTP response
typedef struct {
    HTTP_CODE code;
    char message[BUFFER_SIZE];
} Response;

// Function declarations
REQUEST_BUFFER_RESULT read_into_request_buffer(RequestBuffer *buffer, int client_fd);
int calc_bytes_till_char(const char *sequence, char c);
Request *serialize_request(RequestBuffer *buffer);
Response *handle_request(Request *request);
void send_response(int client_fd, Response *response);

#endif // SERVER_H