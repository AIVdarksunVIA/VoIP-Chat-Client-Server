#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>  
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>

#define PORT 5000
#define MAX_CLIENT 10
#define BUF_SIZE 4000

typedef struct {
    struct sockaddr_in addr;
    int active;
} Client;

int tcp_clients[MAX_CLIENT]; 
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void add_tcp_client(int fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENT; i++) {
        if (tcp_clients[i] == 0) {
            tcp_clients[i] = fd;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void* handle_tcp_client(void* arg) {
    int fd = *(int*)arg;
    free(arg);
    char msg_buffer[1024];

    while(1) {
        int n = recv(fd, msg_buffer, sizeof(msg_buffer), 0);
        if (n <= 0) {
            close(fd);
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENT; i++) {
                if (tcp_clients[i] == fd) {
                    tcp_clients[i] = 0;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            return NULL;
        }

        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENT;  i++) {
            if (tcp_clients[i] != 0 && tcp_clients[i] != fd) {
                send(tcp_clients[i], msg_buffer, n, 0);
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }
}

void* tcp_server_thread(void* arg) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(PORT) };
    
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, MAX_CLIENT);
    
    while(1) {
        int* client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, NULL, NULL);
        if (*client_fd > 0) {
            add_tcp_client(*client_fd);
            pthread_t tid;
            pthread_create(&tid, NULL, handle_tcp_client, client_fd);
            pthread_detach(tid);
        }
    }
}

int main() {
    int sock;

    struct sockaddr_in server_addr;
    Client clients[MAX_CLIENT] = {0};
    unsigned char buffer[BUF_SIZE];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Error creation socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Error post already in use");
        exit(EXIT_FAILURE);
    }
    printf("Server ready on port %d...\n", PORT);
    
    pthread_t tcp_master_tid;
    pthread_create(&tcp_master_tid, NULL, tcp_server_thread, NULL);
    printf("Chat (TCP) server started on port %d\n", PORT);

    while(1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        ssize_t n = recvfrom(sock, buffer, BUF_SIZE, 0, (struct sockaddr*)&client_addr, &addr_len);
        if (n < 0 ){
            perror("recvfrom Error");
            continue;
        }

        if (n == 0) continue;

        int exists = 0;
        int empty_slot = -1;

        for (int i = 0; i < MAX_CLIENT; i++){
            if (clients[i].active){
                if (clients[i].addr.sin_addr.s_addr == client_addr.sin_addr.s_addr &&
                    clients[i].addr.sin_port == client_addr.sin_port) {
                    exists = 1;
                    break;
                }
            } else if (empty_slot == -1) {
                empty_slot = i;
            }
        }

        if (!exists){
            if (empty_slot != -1){
                clients[empty_slot].addr = client_addr;
                clients[empty_slot].active = 1;
                printf("New client: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            } else {
                fprintf(stderr, "Server if full new client was ignored\n");
            }
        }

        for (int i = 0; i < MAX_CLIENT; i++) {
            if (clients[i].active) {
                if (clients[i].addr.sin_port != client_addr.sin_port || 
                    clients[i].addr.sin_addr.s_addr != client_addr.sin_addr.s_addr) {

                    ssize_t sent = sendto(sock, buffer, n, 0, 
                                          (struct sockaddr*)&clients[i].addr, sizeof(clients[i].addr));
                    if (sent < 0){
                       perror("Sending Error (sendto)");
                       //clients[i].active = 0;
                    }   

                }
            }
        }
    }

    close(sock);
    return 0;
}
