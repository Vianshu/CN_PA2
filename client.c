#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

void* client_task(void* arg) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    const char *hello = "Hello from client";
    char buffer[1024] = {0};

    pthread_t this_id = pthread_self();

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Thread %lu: Socket creation error\n", this_id);
        return NULL;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8005);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Thread %lu: Invalid address/Address not supported\n", this_id);
        return NULL;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Thread %lu: Connection Failed\n", this_id);
        return NULL;
    }

    send(sock, hello, strlen(hello), 0);
    printf("Thread %lu: Hello message sent\n", this_id);
    read(sock, buffer, 1024);
    printf("Thread %lu: Message received: %s\n", this_id, buffer);

    close(sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Number of clients not defined! Try Again.\n");
        return 1;
    }

    int n = atoi(argv[1]);

    while (n-- > 0) {
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, client_task, NULL);
        pthread_join(thread_id, NULL);
    }

    return 0;
}
