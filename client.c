#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/file.h>
#include <time.h>

#define CONFIG_FILE "config"
#define BUFFER_SIZE 2048
#define DELAY_SUM_FILE "delay_sum.log"

int main(int argc, char *argv[]) {
    int client_fd;
    struct sockaddr_un server_addr;
    char buffer[BUFFER_SIZE];
    FILE *config_file;
    char socket_path[BUFFER_SIZE];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <delay_ms>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int delay_ms = atoi(argv[1]);
    if (delay_ms < 0) {
        fprintf(stderr, "Delay must be not negative integer\n");
        exit(EXIT_FAILURE);
    }

    config_file = fopen(CONFIG_FILE, "r");
    if (config_file == NULL) {
        perror("Failed to open config file");
        exit(EXIT_FAILURE);
    }
    if (fgets(socket_path, BUFFER_SIZE, config_file) == NULL) {
        perror("Failed to read socket path from config file");
        exit(EXIT_FAILURE);
    }
    fclose(config_file);
    socket_path[strcspn(socket_path, "\n")] = 0;

    client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path) - 1);

    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect to server");
        exit(EXIT_FAILURE);
    }

    int total_delay = - delay_ms; // не учитываем задержку после последней отправки.
    ssize_t num_read;

    srand(time(NULL) ^ getpid());

    while ((num_read = read(STDIN_FILENO, buffer, rand() % 255 + 1)) > 0) {
        if (write(client_fd, buffer, num_read) != num_read) {
            perror("Failed to write to server");
            exit(EXIT_FAILURE);
        }

        if (delay_ms > 0) {
            usleep(delay_ms * 1000);
            total_delay += delay_ms;
        }
    }

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Failed to set timeout on socket");
        exit(EXIT_FAILURE);
    }

    while ((num_read = read(client_fd, buffer, BUFFER_SIZE)) > 0) {
        write(STDOUT_FILENO, buffer, num_read);
    }

    close(client_fd);

    int delay_file_fd = open(DELAY_SUM_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (delay_file_fd < 0) {
        perror("Failed to open delay sum file");
        exit(EXIT_FAILURE);
    }

    if (flock(delay_file_fd, LOCK_EX) == -1) {
        perror("Failed to lock delay sum file");
        exit(EXIT_FAILURE);
    }

    dprintf(delay_file_fd, "%d\n", total_delay);

    flock(delay_file_fd, LOCK_UN);
    close(delay_file_fd);

    return 0;
}