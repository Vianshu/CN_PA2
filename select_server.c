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

#define PORT 8005
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

typedef struct {
    int pid;
    char name[256];
    long user_time;
    long kernel_time;
    long total_time;
} process_info;

void get_top_cpu_processes(char *output, size_t output_size) {
    DIR *proc_dir;
    struct dirent *entry;
    process_info top[2] = {0};

    proc_dir = opendir("/proc");
    if (proc_dir == NULL) {
        perror("opendir failed");
        return;
    }

    while ((entry = readdir(proc_dir)) != NULL) {
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            char stat_file_path[256];
            snprintf(stat_file_path, sizeof(stat_file_path), "/proc/%s/stat", entry->d_name);
            FILE *stat_file = fopen(stat_file_path, "r");
            if (stat_file) {
                process_info proc = {0};
                long utime, stime;

                fscanf(stat_file, "%d %s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %lu %lu",
                       &proc.pid, proc.name, &utime, &stime);
                fclose(stat_file);

                proc.user_time = utime;
                proc.kernel_time = stime;
                proc.total_time = proc.user_time + proc.kernel_time;

                for (int i = 0; i < 2; i++) {
                    if (proc.total_time > top[i].total_time) {
                        if (i == 0 && top[1].total_time < proc.total_time) {
                            top[1] = top[0];
                        }
                        top[i] = proc;
                        break;
                    }
                }
            }
        }
    }
    closedir(proc_dir);

    // Format the output string with the top processes
    snprintf(output, output_size,
             "Top 2 CPU-consuming processes:\n"
             "PID: %d, Name: %s, User Time: %ld, Kernel Time: %ld, Total Time: %ld\n"
             "PID: %d, Name: %s, User Time: %ld, Kernel Time: %ld, Total Time: %ld\n",
             top[0].pid, top[0].name, top[0].user_time, top[0].kernel_time, top[0].total_time,
             top[1].pid, top[1].name, top[1].user_time, top[1].kernel_time, top[1].total_time);
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
                    // Send top 2 processes to the client
                    char process_info[512] = {0}; // Buffer to hold the CPU process info
                    get_top_cpu_processes(process_info, sizeof(process_info));

                    send(sd, process_info, strlen(process_info), 0);
                    printf("Top processes info sent to client\n");

                    // Remove the client socket after sending the info
                    close(sd);
                    client_sockets[i] = 0; // Remove the socket from the list
                }
            }
        }
    }

    return 0;
}
