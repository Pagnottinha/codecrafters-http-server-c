#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define RESPONSE_OK "HTTP/1.1 200 OK\r\n\r\n"
#define RESPONSE_NOTFOUND "HTTP/1.1 404 Not Found\r\n\r\n"

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);

	int server_fd, client_fd, client_addr_len;
	struct sockaddr_in client_addr;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}
	
	// Since the tester restarts your program quite often, setting REUSE_PORT
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEPORT failed: %s \n", strerror(errno));
		return 1;
	}
	
	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
									 .sin_port = htons(4221),
									 .sin_addr = { htonl(INADDR_ANY) },
									};
	
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}
	
	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}
	
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);
	
	client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
	
	if (client_fd == -1) {
		printf("Client Failed to connect.\n");
		return 1;
	}
	
	char buffer[BUFFER_SIZE];

	ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));

	if (bytes_read < 0) {
		printf("Client Failed to read.\n");
		return 1;
	}

	printf("%s \n", buffer);

	char* request_line = strtok(buffer, "\r\n\r\n");

	char* http_method = strtok(request_line, " ");
	char* path = strtok(NULL, " ");
	char* http_version = strtok(NULL, " ");

	char* response = NULL;

	if (strcmp(http_method, "GET") == 0 && strcmp(path, "/") == 0) {
		response = RESPONSE_OK;
	}
	else {
		response = RESPONSE_NOTFOUND;
	}

	ssize_t bytes_send = send(client_fd, response, strlen(response), 0);
	close(client_fd);

	if (bytes_send == -1) {
		printf("Send response failed: %s \n", strerror(errno));
		return 1;
	}

	close(server_fd);

	return 0;
}
