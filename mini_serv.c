#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <stdio.h>
#include <netinet/in.h>

typedef struct s_list {
    int fd;
    int id;
    struct s_list *next;
} t_list;

t_list * g_clients;
int g_listener;
fd_set g_readfds;
fd_set g_selected_readfds;
fd_set g_selected_writefds;
int g_max_fd;
char g_buf[200000];
char g_msg[200000];
char g_tmp[200000];
int g_max_id;

void fatal_error() {
    write(2, "Fatal error\n", strlen("Fatal error\n"));
    exit(1);
}

void send_msg(int except_fd) {
    for (t_list * client = g_clients; client != NULL; client = client->next) {
        if (client->fd != g_listener && client->fd != except_fd) {
            if (FD_ISSET(client->fd, &g_selected_writefds)) {
                send(client->fd, g_msg, strlen(g_msg), 0);
            }
        }
    }
}

t_list * add_client(int new_fd) {
    t_list *temp;
    t_list *new_client;
    new_client = (t_list *)calloc(1, sizeof(*g_clients));
    if (new_client == NULL)
        fatal_error();
    new_client->fd = new_fd;
    new_client->id = g_max_id;
    g_max_id++;
    if (g_max_fd < new_fd)
        g_max_fd = new_fd;

    if (g_clients == NULL) {
        g_clients = new_client;
    } else {
        temp = g_clients;
        while (temp->next)
            temp = temp->next;
        temp->next = new_client;
    }
    return new_client;
}

void remove_client(t_list * client) {
    if (client == g_clients) {
        g_clients = g_clients->next;
    } else {
        t_list * temp = g_clients;
        while (temp->next) {
            if (temp->next == client) {
                temp->next = temp->next->next;
                break;
            }
            temp = temp->next;
        }
    }

    free(client);
}

int get_client_id(int fd) {
    t_list * temp = g_clients;

    while(temp) {
        if (temp->fd == fd)
            return temp->id;
        temp = temp->next;
    }
    return 0;
}

int main(int argc, char ** argv) {
    if (argc != 2) {
        write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
        exit(1);
    }

    unsigned short port = atoi(argv[1]);

    struct sockaddr_in address;
    address.sin_family = 2;
    address.sin_port = (port >> 8) | (port << 8);
    address.sin_addr.s_addr = 0x100007f;

    if ((g_listener = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        fatal_error();
    if (bind(g_listener, (const struct sockaddr*)(&address), sizeof(address)) == -1)
        fatal_error();
    if (listen(g_listener, 0) == -1)
        fatal_error();

    g_max_fd = g_listener;

    FD_ZERO(&g_readfds);
    FD_SET(g_listener, &g_readfds);

    int new_fd;
    unsigned long len;
    while (1) {
        g_selected_readfds = g_readfds;
        g_selected_writefds = g_readfds;
        if (select(g_max_fd + 1, &g_selected_readfds, &g_selected_writefds, NULL, NULL) == -1)
            continue;

        if (FD_ISSET(g_listener, &g_selected_readfds)) {
            if ((new_fd = accept(g_listener, NULL, NULL)) == -1)
                continue;

            t_list * client = add_client(new_fd);
            bzero(g_msg, 50);
            sprintf(g_msg, "server: client %d just arrived\n", client->id);
//            write(2, g_msg, strlen(g_msg));
            send_msg(new_fd);
            FD_SET(new_fd, &g_readfds);

        } else {
            for (t_list * client = g_clients; client != NULL; client = client->next) {
                if (FD_ISSET(client->fd, &g_selected_readfds)) {
                    len = recv(client->fd, g_buf, sizeof(g_buf), 0);
                    if (len <= 0) {
                        bzero(g_msg, 50);
                        sprintf(g_msg, "server: client %d just left\n", client->id);
                        send_msg(client->fd);
                        FD_CLR(client->fd, &g_readfds);
                        close(client->fd);
                        remove_client(client);
                        break;
                    } else {
                        g_buf[len] = '\0';
                        unsigned long i = 0;
                        unsigned long j = 0;
                        while (i < len) {
                            g_tmp[j] = g_buf[i];
                            if (g_buf[i] == '\n') {
                                g_tmp[j] = '\0';
                                sprintf(g_msg, "client %d: %s\n", client->id, g_tmp);
                                send_msg(client->fd);
                                j = -1;
                            }
                            i++;
                            j++;
                        }
                    }
                }
            }
        }
    }
}
