#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

#define MESSAGE_SIZE 16
#define DEFAULT_CLIENT_THREADS 4
#define MAX_RETRIES 5
#define TIMEOUT_USEC 100000  // 100 ms

char *server_ip = "127.0.0.1";
int server_port = 12345;
int num_client_threads = DEFAULT_CLIENT_THREADS;
int num_requests = 1000;

// Define packet structure with sequence number and client ID
typedef struct {
    int client_id;
    int seq_num;
    char payload[MESSAGE_SIZE];
} packet_t;

typedef struct {
    int socket_fd;
    struct sockaddr_in server_addr;
    socklen_t addr_len;
    int thread_id;
    int tx_cnt;
    int rx_cnt;
} client_thread_data_t;

void *client_thread_func(void *arg) {
    client_thread_data_t *data = (client_thread_data_t *)arg;
    packet_t pkt, recv_pkt;

    struct timeval timeout = {0, TIMEOUT_USEC}; // 100 ms timeout
    setsockopt(data->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    for (int i = 0; i < num_requests; i++) {
        pkt.client_id = data->thread_id;
        pkt.seq_num = i;
        memcpy(pkt.payload, "ABCDEFGHIJKMLNOP", MESSAGE_SIZE);

        int retries = 0;
        while (retries < MAX_RETRIES) {
            sendto(data->socket_fd, &pkt, sizeof(pkt), 0,
                   (struct sockaddr *)&data->server_addr, data->addr_len);
            data->tx_cnt++;

            ssize_t bytes = recvfrom(data->socket_fd, &recv_pkt, sizeof(recv_pkt), 0, NULL, NULL);
            if (bytes > 0 && recv_pkt.seq_num == pkt.seq_num && recv_pkt.client_id == pkt.client_id) {
                data->rx_cnt++;
                break;
            } else {
                retries++;
            }
        }
    }

    close(data->socket_fd);
    return NULL;
}

void run_client() {
    pthread_t threads[num_client_threads];
    client_thread_data_t thread_data[num_client_threads];
    struct sockaddr_in server_addr;

    for (int i = 0; i < num_client_threads; i++) {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == -1) {
            perror("socket");
            exit(EXIT_FAILURE);
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

        thread_data[i].socket_fd = sock;
        thread_data[i].server_addr = server_addr;
        thread_data[i].addr_len = sizeof(server_addr);
        thread_data[i].thread_id = i;
        thread_data[i].tx_cnt = 0;
        thread_data[i].rx_cnt = 0;

        pthread_create(&threads[i], NULL, client_thread_func, &thread_data[i]);
    }

    int total_tx = 0, total_rx = 0;
    for (int i = 0; i < num_client_threads; i++) {
        pthread_join(threads[i], NULL);
        printf("Client %d: Sent = %d, Received = %d, Lost = %d\n",
               i, thread_data[i].tx_cnt, thread_data[i].rx_cnt,
               thread_data[i].tx_cnt - thread_data[i].rx_cnt);
        total_tx += thread_data[i].tx_cnt;
        total_rx += thread_data[i].rx_cnt;
    }

    printf("Total Packets Sent: %d\n", total_tx);
    printf("Total Packets Received: %d\n", total_rx);
    printf("Total Packet Loss: %d\n", total_tx - total_rx);
}

void run_server() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    packet_t recv_pkt;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on UDP port %d\n", server_port);

    while (1) {
        ssize_t bytes = recvfrom(sock, &recv_pkt, sizeof(recv_pkt), 0,
                                 (struct sockaddr *)&client_addr, &client_len);
        if (bytes > 0) {
            sendto(sock, &recv_pkt, sizeof(recv_pkt), 0,
                   (struct sockaddr *)&client_addr, client_len);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <server|client> [server_ip server_port num_client_threads num_requests]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "server") == 0) {
        run_server();
    } else if (strcmp(argv[1], "client") == 0) {
        if (argc != 6) {
            printf("Usage: %s client <server_ip> <server_port> <num_client_threads> <num_requests>\n", argv[0]);
            return 1;
        }

        server_ip = argv[2];
        server_port = atoi(argv[3]);
        num_client_threads = atoi(argv[4]);
        num_requests = atoi(argv[5]);
        run_client();
    } else {
        printf("Invalid mode. Use 'server' or 'client'.\n");
        return 1;
    }

    return 0;
}
