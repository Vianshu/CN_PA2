#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <ctype.h>
#include <dirent.h>
#include<errno.h>

#define PORT 8000
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024


typedef struct  {
    int socket;
    struct sockaddr_in address;
}client_t ;



void *handle_client(int sock) {
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE * 2]; 
    memset(response, 0, sizeof(response)); 
    snprintf(response, sizeof(response), "Top two CPU-consuming processes:\n");

    FILE *fp = popen("ps -eo pid,comm,%cpu --sort=-%cpu | head -n 3", "r");
    if (fp == NULL) {
        perror("Failed to run command");
        close(sock);
        return NULL;
    }

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strncat(response, buffer, sizeof(response) - strlen(response) - 1);
    }
    pclose(fp);
    send(sock, response, strlen(response), 0);

    //close(sock);
    return NULL;
}

int main() {
    int server_fd, new_socket, client_sockets[MAX_CLIENTS], max_sd, sd;
    struct sockaddr_in address;
    fd_set readfds;
    char buffer[BUFFER_SIZE];

    // Initialize client sockets
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = 0;
    }

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the address and port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d\n", PORT);

    int addrlen = sizeof(address);

    while (1) {
        // Clear the socket set
        FD_ZERO(&readfds);

        // Add server socket to set
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        // Add client sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        // Wait for activity on any of the sockets
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            perror("Select error");
        }

        // If something happened on the server socket, it's an incoming connection
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }

            printf("New connection, socket fd is %d\n", new_socket);

            // Add new socket to the client_sockets array
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    printf("Adding to list of sockets as %d\n", i);
                    break;
                }
            }
        }

        // Check all client sockets
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (FD_ISSET(sd, &readfds)) {
                // Check if it was for closing, and also read the incoming message
                int valread = read(sd, buffer, BUFFER_SIZE);
                if (valread == 0) {
                    // Client disconnected
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("Host disconnected, IP %s, port %d\n",
                           inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    close(sd);
                    client_sockets[i] = 0; // Remove the socket from the list
                } else {
                    handle_client(client_sockets[i]);
                    client_sockets[i] = 0; // Remove the socket from the list
                }
            }
        }
    }

    return 0;
}
