#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <pthread.h>

typedef struct {
    int pid;
    char name[256];
    long user_time;
    long kernel_time;
    long total_time;
} process_info;

void get_top_cpu_processes() {
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

    printf("Top %d CPU-consuming processes:\n", 2);
    for (int i = 0; i < 2; i++) {
        printf("PID: %d, Name: %s, User Time: %ld, Kernel Time: %ld, Total Time: %ld\n",
               top[i].pid, top[i].name, top[i].user_time, top[i].kernel_time, top[i].total_time);
    }
}

void *handle_client(void *arg) {
    int new_socket = *(int *)arg;
    char buffer[1024] = {0};
    const char *hello = "Hello from server";

    get_top_cpu_processes();
    read(new_socket, buffer, 1024);
    printf("%d -> Message received: %s\n", new_socket, buffer);
    send(new_socket, hello, strlen(hello), 0);
    printf("Hello message sent\n");
    close(new_socket);
    free(arg);  
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8005);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        int *new_socket = malloc(sizeof(int));
        if ((*new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            free(new_socket);
            exit(EXIT_FAILURE);
        }
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, new_socket);
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}
