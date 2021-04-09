#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct s_client {
    int fd, id;
    struct s_client * next;
} t_client;

fd_set g_readfds;
fd_set readfds;
fd_set writefds;
int max_fd;
int max_id;
t_client * clients;
int sockfd;
char server_msg[50];
char client_msg[200000];
char client_buf[200000];
char to_send[200000];

void exit_fatal() {
    write(2, "Fatal error\n", strlen("Fatal error\n"));
    if (sockfd)
        close(sockfd);
    exit(1);
}

t_client * add_client(int connfd) {
    t_client * new_client = calloc(1, sizeof(t_client));
    if (new_client == NULL)
        exit_fatal();

    new_client->fd = connfd;
    new_client->id = max_id++;
    if (clients == NULL) {
        clients = new_client;
    } else {
        t_client * temp = clients;
        while (temp->next)
            temp = temp->next;
        temp->next = new_client;
    }

    if (max_fd < connfd)
        max_fd = connfd;
    FD_SET(connfd, &g_readfds);
    return new_client;
}

void send_msg(int socket, char * messsage) {
    t_client * temp = clients;

    while (temp) {
        if (temp->fd != socket && FD_ISSET(temp->fd, &writefds))
            if (send(temp->fd, messsage, strlen(messsage), 0) < 0)
                exit_fatal();
        temp = temp->next;
    }
}

void remove_client(socket) {
    t_client * temp = clients;
    t_client * prev = clients;

    if (clients->fd == socket)
        clients = clients->next;
    else {
        while (temp->fd != socket) {
            prev = temp;
            temp = temp->next;
        }
        prev->next = prev->next->next;
    }

    free(temp);
    FD_CLR(socket, &g_readfds);
}

int get_client_id(int socket) {
    t_client * temp = clients;

    while (temp) {
        if (temp->fd == socket)
            return temp->id;
        temp = temp->next;
    }
    return -1;
}

void resend_messages(int socket) {
    int i = 0;
    int j = 0;
    while (client_msg[i] != '\0') {
        client_buf[j] = client_msg[i];
        if (client_msg[i] == '\n') {
            sprintf(to_send, "client %d: %s", get_client_id(socket), client_buf);
            send_msg(socket, to_send);
            bzero(client_buf, strlen(client_buf));
            bzero(to_send, strlen(to_send));
            j = -1;
        }
        i++;
        j++;
    }
    bzero(client_msg, strlen(client_msg));
    bzero(client_buf, strlen(client_buf));
}

int main(int argc, char ** argv) {
    if (argc != 2) {
        write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
        exit(1);
    }

    int connfd;
    struct sockaddr_in servaddr, cli;
    int port = atoi(argv[1]);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = 0x100007f; //127.0.0.1
    servaddr.sin_port = port >> 8 | port << 8;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        exit_fatal();
    if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
        exit_fatal();
    if (listen(sockfd, 0) != 0)
        exit_fatal();

    max_fd = sockfd;
    FD_SET(sockfd, &g_readfds);
    while (1) {
        readfds = writefds = g_readfds;
        if (select(max_fd + 1, &readfds, &writefds, NULL, NULL) < 0)
            continue;

        for (int socket = 0; socket <= max_fd; socket++) {
            if (FD_ISSET(socket, &readfds)) {
                if (socket == sockfd) {
                    socklen_t len = sizeof(cli);
                    connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
                    if (connfd < 0)
                        exit_fatal();
                    t_client * new_client = add_client(connfd);
                    sprintf(server_msg, "server: client %d just arrived\n", new_client->id);
                    send_msg(socket, server_msg);
                    bzero(server_msg, sizeof(server_msg));
                    break;
                } else {
                    if (recv(socket, client_msg, sizeof(client_msg), 0) <= 0) {
                        sprintf(server_msg, "server: client %d just left\n", get_client_id(socket));
                        remove_client(socket);
                        send_msg(socket, server_msg);
                        bzero(server_msg, sizeof(server_msg));
                        break;
                    } else {
                        resend_messages(socket);
                    }
                }
            }
        }
    }
    return 0;
}