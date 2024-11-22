#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define BUFFER_SIZE 1024

struct thread_args {
    char ip[INET_ADDRSTRLEN];
    int port;
    int count;
};

void *tcping_thread(void *arg) {
    struct thread_args *args = (struct thread_args *)arg;

    struct sockaddr_in server_addr;
    int sock;
    struct timespec start, end;

    // 初始化目标地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(args->port);
    inet_pton(AF_INET, args->ip, &server_addr.sin_addr);

    for (int i = 0; i < args->count; i++) {
        // 创建 socket
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("Socket creation failed");
            pthread_exit(NULL);
        }

        // 开始计时
        clock_gettime(CLOCK_MONOTONIC, &start);

        // 尝试连接
        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connection failed");
            close(sock);
            pthread_exit(NULL);
        }

        // 结束计时
        clock_gettime(CLOCK_MONOTONIC, &end);
        close(sock);

        // 计算 RTT
        double rtt = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
        printf("Thread %ld - RTT to %s:%d = %.2f ms\n", pthread_self(), args->ip, args->port, rtt);

        sleep(1); // 延时，避免过于频繁
    }

    free(args); // 释放线程参数内存
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <IP> <Port> <NumThreads> [PingCount]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int num_threads = atoi(argv[3]);
    int ping_count = (argc > 4) ? atoi(argv[4]) : 4; // 默认 4 次 ping

    if (num_threads <= 0 || port <= 0 || ping_count <= 0) {
        fprintf(stderr, "Invalid arguments. Ensure positive numbers for port, threads, and count.\n");
        return EXIT_FAILURE;
    }

    pthread_t threads[num_threads];

    for (int i = 0; i < num_threads; i++) {
        struct thread_args *args = malloc(sizeof(struct thread_args));
        if (args == NULL) {
            perror("Memory allocation failed");
            return EXIT_FAILURE;
        }

        // 初始化线程参数
        strncpy(args->ip, ip, INET_ADDRSTRLEN);
        args->port = port;
        args->count = ping_count;

        // 创建线程
        if (pthread_create(&threads[i], NULL, tcping_thread, (void *)args) != 0) {
            perror("Thread creation failed");
            free(args);
            return EXIT_FAILURE;
        }
    }

    // 等待所有线程结束
    for (int i = 0; i < num_threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("Thread join failed");
        }
    }

    printf("All threads finished.\n");
    return EXIT_SUCCESS;
}
