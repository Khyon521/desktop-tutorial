// pa2_task1.c
//Generative AI: CHATGPT was used to clean up code, and catch mistakes, as well as verify the work met rubric Criteria
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>

#define MESSAGE_SIZE 16
#define DEFAULT_CLIENT_THREADS 4

char *server_ip = "127.0.0.1";
int server_port = 12345;
int num_client_threads = DEFAULT_CLIENT_THREADS;
int num_requests = 100000;

typedef struct {
    int socket_fd;
    struct sockaddr_in server_addr;
    long long total_rtt;
    long total_messages;
    long tx_cnt;
    long rx_cnt;
    int thread_id;
} client_thread_data_t;

void *client_thread_func(void *arg) {
    client_thread_data_t *data = (client_thread_data_t *)arg;
    char send_buf[MESSAGE_SIZE] = "ABCDEFGHIJKMLNOP";
    char recv_buf[MESSAGE_SIZE];
    socklen_t addr_len = sizeof(data->server_addr);

    struct timeval timeout = {2, 0}; // 2 second timeout
    setsockopt(data->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct timeval start, end;

    for (int i = 0; i < num_requests; i++) {
        gettimeofday(&start, NULL);

        if (sendto(data->socket_fd, send_buf, MESSAGE_SIZE, 0,
                   (struct sockaddr *)&data->server_addr, addr_len) != MESSAGE_SIZE) {
            continue;
        }
        data->tx_cnt++;

        int recv_bytes = recvfrom(data->socket_fd, recv_buf, MESSAGE_SIZE, 0, NULL, NULL);
        if (recv_bytes != MESSAGE_SIZE) {
            continue;
        }
        data->rx_cnt++;

        gettimeofday(&end, NULL);
        long long rtt = (end.tv_sec - start.tv_sec) * 1000000LL +
                        (end.tv_usec - start.tv_usec);
        data->total_rtt += rtt;
        data->total_messages++;
    }

    pthread_exit(NULL);
}

void run_client() {
    pthread_t threads[num_client_threads];
    client_thread_data_t thread_data[num_client_threads];

    for (int i = 0; i < num_client_threads; i++) {
        int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_fd < 0) {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }

        memset(&thread_data[i], 0, sizeof(client_thread_data_t));
        thread_data[i].socket_fd = sock_fd;
        thread_data[i].thread_id = i;

        thread_data[i].server_addr.sin_family = AF_INET;
        thread_data[i].server_addr.sin_port = htons(server_port);
        if (inet_pton(AF_INET, server_ip, &thread_data[i].server_addr.sin_addr) <= 0) {
            perror("Invalid server IP");
            exit(EXIT_FAILURE);
        }

        pthread_create(&threads[i], NULL, client_thread_func, &thread_data[i]);
    }

    long long total_rtt = 0;
    long total_messages = 0, total_tx = 0, total_rx = 0;

    for (int i = 0; i < num_client_threads; i++) {
        pthread_join(threads[i], NULL);
        close(thread_data[i].socket_fd);

        printf("Client %d: Sent = %ld, Received = %ld, Lost = %ld\n",
               thread_data[i].thread_id,
               thread_data[i].tx_cnt,
               thread_data[i].rx_cnt,
               thread_data[i].tx_cnt - thread_data[i].rx_cnt);

        total_rtt += thread_data[i].total_rtt;
        total_messages += thread_data[i].total_messages;
        total_tx += thread_data[i].tx_cnt;
        total_rx += thread_data[i].rx_cnt;
    }

    long lost = total_tx - total_rx;
    float req_rate = (float)total_rx / ((float)total_rtt / 1000000.0);

    printf("Total Packets Sent: %ld\n", total_tx);
    printf("Total Packets Received: %ld\n", total_rx);
    printf("Total Packet Loss: %ld\n", lost);
}

void run_server() {
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    int rcvbuf = 1024;
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("UDP server listening on port %d...\n", server_port);

    char buffer[MESSAGE_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1) {
        int recv_bytes = recvfrom(sock_fd, buffer, MESSAGE_SIZE, 0,
                                  (struct sockaddr *)&client_addr, &addr_len);
        if (recv_bytes < 0) {
            perror("recvfrom failed");
            continue;
        }

        sendto(sock_fd, buffer, MESSAGE_SIZE, 0,
               (struct sockaddr *)&client_addr, addr_len);
    }

    close(sock_fd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <server|client> [server_ip server_port num_client_threads num_requests]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "server") == 0) {
        if (argc > 2) server_ip = argv[2];
        if (argc > 3) server_port = atoi(argv[3]);
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

