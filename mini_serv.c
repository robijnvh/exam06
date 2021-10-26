#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>

// STRUCT
typedef struct s_client {
    int id;
    int fd;
    struct s_client *next;
} t_client;

// GLOBAL
t_client *clients = NULL;
int sockfd, client_counter, bufsize;
fd_set writeFD, readFD, allFD;
struct sockaddr_in servaddr;
char message[1000000];

// FUNCTIONS
void    parseError(const char *error)
{
    write(STDERR_FILENO, error, strlen(error));
    exit(1);
}

void    checkInput(int argc) {
    if (argc != 2)
        parseError("Wrong number of arguments\n");
}

void    setVariables(int port) {
    client_counter = 0;
    bufsize = 1000000;
    sockfd = 0;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        parseError("Fatal error\n");
    
    bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port);
}

int maxfd() {
    int ret = sockfd;
    t_client *tmp = clients;
    while (tmp) {
        if (tmp->fd > ret)
            ret = tmp->fd;
        tmp = tmp->next;
    }
    return ret;
}

void    cleanError() {
    t_client *tmp = clients;
    while (tmp) {
        t_client *current = tmp;
        tmp = tmp->next;
        close(current->fd);
        free(current);
    }
    close(sockfd);
    parseError("Fatal error\n");
}

void    addClient(int fd) {
    t_client *new = malloc(sizeof(t_client));
    if (!new)
        cleanError();
    new->fd = fd;
    new->id = client_counter;
    client_counter++;
    new->next = NULL;
    FD_SET(new->fd, &allFD);
    if (!clients)
        clients = new;
    else {
        t_client *tmp = clients;
        while (tmp->next)
            tmp = tmp->next;
        tmp->next = new;
    }
}

void    sendMessage(const char *message, int id) {
    t_client *tmp = clients;
    while (tmp) {
        if (FD_ISSET(tmp->fd, &writeFD) && tmp->id != id)
            send(tmp->fd, message, strlen(message), 0);
        tmp = tmp->next;
    }
}

void    createClient() {
    int newfd;
    struct sockaddr_in client;
    bzero(&client, sizeof(client));
    socklen_t len = sizeof(client);
    if ((newfd = accept(sockfd, (struct sockaddr *)&client, &len)) < 0)
        cleanError();
    addClient(newfd);
    bzero(&message, sizeof(message));
    sprintf(message, "server: client %d just arrived\n", client_counter - 1);
    sendMessage(message, -1);
}

void    deleteClient(t_client *del) {
    FD_CLR(del->fd, &allFD);
    close(del->fd);
    t_client *tmp = clients;
    if (tmp->fd == del->fd) {
        clients = tmp->next;
        free(tmp);
    }
    else {
        while (tmp) {
            if (tmp->next->fd == del->fd) {
                free(del);
                tmp->next = del->next;
                break;
            }
            tmp = tmp->next;
        }
    }
    sprintf(message, "server: client %d just left\n", tmp->id);
    sendMessage(message, -1);
}

void    createMessage(int id) {
    char clientMessage[50];
    bzero(&clientMessage, sizeof(clientMessage));
    sprintf(clientMessage, "client %d: ", id);

    char ret[bufsize];
    bzero(&ret, sizeof(ret));

    char buf[bufsize];

    size_t i = 0;
    while (message[i]) {
        bzero(&buf, sizeof(buf));
        strcat(ret, clientMessage);
        size_t j = 0;
        while (message[i] && message[i] != '\n') {
            buf[j] = message[i];
            ++i;
            ++j;
        }
        if (message[i] == '\n') {
            buf[j] = message[i];
            ++i;
        }
        strcat(ret, buf);
    }
    sendMessage(ret, id);
}

void    client(t_client *tmp) {
    bzero(&message, sizeof(message));
    if (recv(tmp->fd, message, bufsize, 0) <= 0)
        deleteClient(tmp);
    else
        createMessage(tmp->id);
}

int main(int argc, char **argv) {
    checkInput(argc);
    setVariables(atoi(argv[1]));

    int option = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
        parseError("Fatal error\n");
    if (listen(sockfd, 255) != 0)
        parseError("Fatal error\n");
    
    FD_ZERO(&allFD);
    FD_SET(sockfd, &allFD);
    while (1) {
        readFD = allFD;
        writeFD = allFD;
        if (select(maxfd() + 1, &readFD, &writeFD, NULL, NULL) == -1)
            cleanError();
        
        if (FD_ISSET(sockfd, &readFD)) {
            createClient();
            continue;
        }
        else {
            t_client *tmp = clients;
            while (tmp) {
                t_client *add = tmp->next;
                if (FD_ISSET(tmp->fd, &readFD)) {
                    client(tmp);
                    break;
                }
                tmp = add;
            }
        }
    }
    return 0;
}