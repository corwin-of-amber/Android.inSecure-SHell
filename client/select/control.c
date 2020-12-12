
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>

#define BUFFERSIZE 64 * 1024

int send_get(int sockfd, const char *pathname);
int send_put(int sockfd, const char *pathname);
int send_exec(int sockfd, const char *command);

int send_command(int sockfd, const char *cmd, const char *arg)
{
    int rc = -1;

    send(sockfd, "<control>\n", 10, 0);

    if (strcasecmp(cmd, "GET") == 0)
        rc = send_get(sockfd, arg);
    else if (strcasecmp(cmd, "PUT") == 0)
        rc = send_put(sockfd, arg);
    else if (strcasecmp(cmd, "EXEC") == 0)
        rc = send_exec(sockfd, arg);
    else
        fprintf(stderr, "unknown command: '%s'\n", cmd);

    close(sockfd);
    return rc;
}

int send_get(int sockfd, const char *pathname)
{
    int rc = 0;

    send(sockfd, "GET ", 4, 0);
    send(sockfd, pathname, strlen(pathname), 0);
    send(sockfd, "\n", 1, 0);
    shutdown(sockfd, SHUT_WR);

    while (1) {
        char buf[BUFFERSIZE];
        int sz = recv(sockfd, buf, sizeof(buf), 0);

        if (sz < 0) { perror("recv"); rc = -1; break; }
        else if (sz == 0) break;

        int wz = write(1, buf, sz);
        if (wz < 0) { perror("write stdout"); rc = -1; break; }
        else if (wz < sz) 
        { /* ?? */ fprintf(stderr, "warning: short write\n"); }
    }

    return rc;
}

int send_put(int sockfd, const char *pathname)
{
    int rc = 0;

    send(sockfd, "PUT ", 4, 0);
    send(sockfd, pathname, strlen(pathname), 0);
    send(sockfd, "\n", 1, 0);

    while (1) {
        char buf[BUFFERSIZE];
        int sz = read(0, buf, sizeof(buf));

        if (sz < 0) { perror("read stdin"); rc = -1; break; }
        else if (sz == 0) break;

        int wz = send(sockfd, buf, sz, 0);
        if (wz < 0) { perror("send"); rc = -1; break; }
        else if (wz < sz)  /* should block */
        { /* ?? */ fprintf(stderr, "warning: short send\n"); }
    }

    shutdown(sockfd, SHUT_WR);
    while (1) {
        char ack[10];
        int sz = recv(sockfd, ack, sizeof(ack), 0);
        if (sz < 0) { perror("recv ack"); rc = -1; break; }
        else if (sz == 0) break;
    }
    
    return rc;
}

int send_exec(int sockfd, const char *command)
{
    /** @oops this is basically send_get */
    int rc = 0;

    send(sockfd, "EXEC ", 5, 0);
    send(sockfd, command, strlen(command), 0);
    send(sockfd, "\n", 1, 0);
    shutdown(sockfd, SHUT_WR);

    while (1) {
        char buf[BUFFERSIZE];
        int sz = recv(sockfd, buf, sizeof(buf), 0);

        if (sz < 0) { perror("recv"); rc = -1; break; }
        else if (sz == 0) break;

        int wz = write(1, buf, sz);
        if (wz < 0) { perror("write stdout"); rc = -1; break; }
        else if (wz < sz) 
        { /* ?? */ fprintf(stderr, "warning: short write\n"); }
    }

    return rc;    
}

