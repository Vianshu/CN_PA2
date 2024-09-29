#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <dirent.h>
#include <ctype.h>

#define BUFFER_SIZE 1024

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

    close(sock);
    return NULL;
}


int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    const char *hello = "Hello from server";

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        printf("Socket failed\n");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8000);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("Bind failedn\n");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        printf("Listen failedn\n");
        exit(EXIT_FAILURE);
    }

    while (1){
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            printf("Accept failedn\n");
            exit(EXIT_FAILURE);
        }
        read(new_socket, buffer, 1024);
        printf("Message Recieved\n");
        handle_client(new_socket);
        printf("Message sent\n");
    }
    //close(new_socket);
    close(server_fd);
    return 0;
}
