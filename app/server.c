#include "server.h"

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	int server_fd, client_addr_len;
	struct sockaddr_in client_addr, serv_addr;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	serv_addr = { 	.sin_family = AF_INET ,
					.sin_port = htons(PORT),
					.sin_addr = { htonl(INADDR_ANY) },
				};
	
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	int connectio n_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);
	
	int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);	
	if (client_fd == -1) {
		printf("Accept failed: %s \n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	printf("Client connected\n");

	char buffer[BUFFERSIZE];
	char *message = "HTTP/1.1 200 OK\r\n\r\n";
	while( read(client_fd, buffer, BUFFERSIZE) != 0) {
		send(client_fd, message, strlen(message));
	}
	
	close(server_fd);

	exit(EXIT_SUCCESS);
}
