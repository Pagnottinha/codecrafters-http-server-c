#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define RESPONSE_OK "HTTP/1.1 200 OK\r\n"
#define RESPONSE_NOTFOUND "HTTP/1.1 404 Not Found\r\n"
#define RESPONSE_NOTALLOWED "HTTP/1.1 405 Method Not Allowed\r\n"
#define RESPONSE_SERVERERROR "HTTP/1.1 500 Internal Server Error\r\n"

// remember to free!!!
char* get_header_attribute(char* header, char* attribute);
int connection(int client_fd);

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
	
	while(1) {
		printf("Waiting for a client to connect...\n");
		client_addr_len = sizeof(client_addr);
		
		client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
		
		if (client_fd == -1) {
			printf("Client Failed to connect.\n");
			return 1;
		}
		
		if (fork() == 0) {
			if(connection(client_fd) == 1) {
				printf("Connection error\n");
				return 1;
			}
			break;
		}
		close(client_fd);		
	}

	close(client_fd);
	close(server_fd);

	return 0;
}

int connection(int client_fd) {
	char buffer[BUFFER_SIZE];

	ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));

	if (bytes_read < 0) {
		printf("Client Failed to read.\n");
		return 1;
	}

	char* header = strchr(buffer, '\n') + 1;
	char* request_line = strtok(buffer, "\r\n");

	char* http_method = strtok(request_line, " ");
	char* path = strtok(NULL, " ");
	char* http_version = strtok(NULL, " ");

	printf("HTTP Method: %s \n", http_method);
	printf("Path: %s \n", path);
	printf("Header:\n%s", header);

	char response[BUFFER_SIZE];

	if (strcmp(http_method, "GET") == 0 && strcmp(path, "/") == 0) {
		snprintf(response, BUFFER_SIZE, "%s\r\n", RESPONSE_OK);
	}
	else if (strncmp(path, "/echo/", 6) == 0) {
		if (strcmp(http_method, "GET") == 0) {
			// + 1 to skip '/'
			char* msg = strchr(path + 1, '/') + 1;
			snprintf(response, BUFFER_SIZE, "%sContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s", RESPONSE_OK, strlen(msg), msg);
		}
		else {
			snprintf(response, BUFFER_SIZE, "%s\r\n", RESPONSE_NOTALLOWED);
		}
	}
	else if (strcmp(path, "/user-agent") == 0) {
		if (strcmp(http_method, "GET") == 0) {
			char* agent = get_header_attribute(header, "User-Agent");
			printf("%s\n", agent);
			if (agent != NULL) {
				snprintf(response, BUFFER_SIZE, "%sContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s", RESPONSE_OK, strlen(agent), agent);
				free(agent);
			}
			else {
				snprintf(response, BUFFER_SIZE, "%s\r\n", RESPONSE_SERVERERROR);
			}

		}
		else {
			snprintf(response, BUFFER_SIZE, "%s\r\n", RESPONSE_NOTALLOWED);
		}
	}
	else {
		snprintf(response, BUFFER_SIZE, "%s\r\n", RESPONSE_NOTFOUND);
	}

	ssize_t bytes_send = send(client_fd, response, strlen(response), 0);

	if (bytes_send == -1) {
		printf("Send response failed: %s \n", strerror(errno));
		return 1;
	}

	return 0;
}

char* get_header_attribute(char* header, char* attribute) {
	char new_attribute[strlen(attribute) + 3];
	new_attribute[0] = '\0';
	strcat(new_attribute, attribute);
	strcat(new_attribute, ": ");
	char* start = strstr(header, new_attribute);

	if (start == NULL)
		return NULL;

	char* end = strstr(start, "\r\n");

	size_t content_len = end - start - strlen(new_attribute);
	char* content = (char*) malloc(sizeof(char) * content_len);

	if (content == NULL) {
		printf("Malloc Failed.");
		return NULL;
	}

	strncpy(content, start + strlen(new_attribute), content_len);
	content[content_len] = '\0';

	return content;
}