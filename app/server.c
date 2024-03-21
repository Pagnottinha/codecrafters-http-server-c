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
#define RESPONSE_CREATED "HTTP/1.1 201 Created\r\n"
#define RESPONSE_NOTFOUND "HTTP/1.1 404 Not Found\r\n"
#define RESPONSE_NOTALLOWED "HTTP/1.1 405 Method Not Allowed\r\n"
#define RESPONSE_SERVERERROR "HTTP/1.1 500 Internal Server Error\r\n"

// remember to free!!!
char* get_header_attribute(char* header, char* attribute);
int connection(int client_fd, char* directory_path);
char* get_directory(int argc, char** argv);


int main(int argc, char **argv) {
	// Disable output buffering
	setbuf(stdout, NULL);
	char* directory_path = get_directory(argc, argv);

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
			if(connection(client_fd, directory_path) == 1) {
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

char* get_directory(int argc, char** argv) {
	
	for(int i = 1; i + 1 < argc; i++) {
		if (strcmp(argv[i], "--directory") == 0) {
			return argv[i + 1];
		}
	}

	return NULL;
}

int connection(int client_fd, char* directory_path) {
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
	else if (strncmp(path, "/files/", 6) == 0) {
		char* file_path = NULL;

		if (directory_path != NULL) {
			char* file_name = strchr(path + 1, '/') + 1;
			size_t file_path_len = strlen(directory_path) + strlen(file_name) + 1;
			file_path = (char*) malloc(file_path_len);

			if (file_path == NULL) {
				printf("file_path malloc failed.");
			}
			else {
				snprintf(file_path, file_path_len, "%s%s", directory_path, file_name);
				printf("%s\n", file_path);
			}
		}

		if (strcmp(http_method, "GET") == 0) {
			char* file_content = NULL;

			if (file_path != NULL) {
				FILE* filep = fopen(file_path, "r");
				if (filep != NULL) {
					fseek(filep, 0L, SEEK_END);
					long size = ftell(filep);
					fseek(filep, 0L, SEEK_SET);
					file_content = (char*) malloc(size + 1);
					fread(file_content, size, 1, filep);
					fclose(filep);
				}

				free(file_path);
			}

			if (file_content != NULL) {
				snprintf(response, BUFFER_SIZE, "%sContent-Type: application/octet-stream\r\nContent-Length: %ld\r\n\r\n%s", RESPONSE_OK, strlen(file_content), file_content);
			}
			else if (file_content == NULL) {
				snprintf(response, BUFFER_SIZE, "%s\r\n", RESPONSE_NOTFOUND);
			}
			else {
				snprintf(response, BUFFER_SIZE, "%s\r\n", RESPONSE_SERVERERROR);
			}

		} else if (strcmp(http_method, "POST") == 0) {
			int flag = -1;

			if (file_path != NULL) {
				char* content = strrchr(header, '\n') + 1;

				FILE* filep = fopen(file_path, "w");

				if (filep != NULL) {
					fprintf(filep, "%s", content);
					flag = 0;
					fclose(filep);
				}

				free(file_path);
			}
			
			if (flag != -1) {
				snprintf(response, BUFFER_SIZE, "%s\r\n", RESPONSE_CREATED);
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
	char* content = (char*) malloc(content_len);

	if (content == NULL) {
		printf("Malloc Failed.");
		return NULL;
	}

	strncpy(content, start + strlen(new_attribute), content_len);
	content[content_len] = '\0';

	return content;
}