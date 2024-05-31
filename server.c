#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048
#define LOG_BUF_SIZE 256

int server_fd;
FILE *log_file;

void handle_sigint(int sig) {
    if (log_file) {
        fflush(log_file);
        fclose(log_file);
    }
    if (server_fd >= 0) {
        close(server_fd);
    }
    unlink("/tmp/brownian_bot_socket");
    exit(0);
}

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        exit(EXIT_FAILURE);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        exit(EXIT_FAILURE);
    }
}

long get_current_time_millis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

int main() {
    int client_fds[MAX_CLIENTS], max_fd, i, valread;
    struct sockaddr_un address;
    char buffer[BUFFER_SIZE];
    char response[32];
    char client_buffers[MAX_CLIENTS][BUFFER_SIZE];
    fd_set readfds, writefds;
    int state = 0;

    signal(SIGINT, handle_sigint);

    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    set_nonblocking(server_fd);

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, "/tmp/brownian_bot_socket");
    unlink(address.sun_path);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
        client_fds[i] = 0;
        memset(client_buffers[i], 0, BUFFER_SIZE);
    }

    log_file = fopen("server.log", "w");
    if (log_file == NULL) {
        perror("failed to open log file");
        exit(EXIT_FAILURE);
    }

    while (1) {
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(server_fd, &readfds);
        max_fd = server_fd;

        for (i = 0; i < MAX_CLIENTS; i++) {
            if (client_fds[i] > 0) {
                FD_SET(client_fds[i], &readfds);
                FD_SET(client_fds[i], &writefds);
            }
            if (client_fds[i] > max_fd) {
                max_fd = client_fds[i];
            }
        }

        if (select(max_fd + 1, &readfds, &writefds, NULL, NULL) < 0) {
            perror("select failed");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(server_fd, &readfds)) {
            int new_socket;
            if ((new_socket = accept(server_fd, NULL, NULL)) < 0) {
                perror("accept failed");
                exit(EXIT_FAILURE);
            }

            set_nonblocking(new_socket);

            void *heap_end = sbrk(0);
            fprintf(log_file, "New client connected, FD: %d, Heap end: %p\n", new_socket, heap_end);

            for (i = 0; i < MAX_CLIENTS; i++) {
                if (client_fds[i] == 0) {
                    client_fds[i] = new_socket;
                    break;
                }
            }
        }

        for (i = 0; i < MAX_CLIENTS; i++) {
            if (FD_ISSET(client_fds[i], &readfds)) {
                memset(buffer, 0, BUFFER_SIZE);
                valread = read(client_fds[i], buffer, 256);

                if (valread == 0) {
                    close(client_fds[i]);
                    client_fds[i] = 0;
                } else if (valread > 0) {
                    if (strlen(client_buffers[i]) + valread < BUFFER_SIZE) {
                        strncat(client_buffers[i], buffer, valread);
                        long timestamp = get_current_time_millis();
                        fprintf(log_file, "Client received time in ms: %ld\n", timestamp);

                        char *newline_pos;
                        while ((newline_pos = strchr(client_buffers[i], '\n')) != NULL) {
                            int num = atoi(client_buffers[i]);
                            state += num;

                            snprintf(response, 32, "%d\n", state);

                            fprintf(log_file, "Received: %d\n", num);

                            if (FD_ISSET(client_fds[i], &writefds)) {
                                int sent_bytes = send(client_fds[i], response, strlen(response), 0);
                                if (sent_bytes == -1) {
                                    perror("send failed");
                                    fprintf(log_file, "Send failed, errno: %d\n", errno);
                                    fflush(log_file);
                                } else {
                                    fprintf(log_file, "Sent: %s", response);
                                }
                            }

                            memmove(client_buffers[i], newline_pos + 1, strlen(newline_pos + 1) + 1);
                        }
                    } else {
                        memset(client_buffers[i], 0, BUFFER_SIZE);
                        fprintf(log_file, "Buffer overflow, resetting client buffer\n");
                        fflush(log_file);
                    }
                } else if (valread < 0 && errno != EAGAIN) {
                    perror("read failed");
                    fprintf(log_file, "Read failed, errno: %d\n", errno);
                    fflush(log_file);
                }
            }
        }
    }

    return 0;
}
