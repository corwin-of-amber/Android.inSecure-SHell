/**
 * Control protocol for executing simple commands on the server.
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "config.h"

struct LineBuffered {
    char read_buf[BUFFERSIZE];
    int read_pos;
    int read_sz;
    char line_buf[BUFFERSIZE];
    int line_pos;
};

int handle_control_command(int sockfd, struct LineBuffered *incoming);
int handle_get(int sockfd, const char *pathname);
int handle_get_dir(int sockfd, const char *pathname);
int handle_get_file(int sockfd, const char *pathname);
int handle_put(int sockfd, const char *pathname,
               struct LineBuffered *incoming);
int handle_exec(int sockfd, const char *command);


void handle_control(int sockfd, char *buf, int buf_sz)
{
    const char *ok_msg = "<ok>\n";
    send(sockfd, ok_msg, strlen(ok_msg), 0);

    struct LineBuffered incoming;
    incoming.line_pos = 0;
    
    incoming.read_sz = buf_sz;
    memcpy(incoming.read_buf, buf, buf_sz);

    int rc = 0;

    while (rc == 0) {
        if (incoming.read_pos >= incoming.read_sz) {
            incoming.read_sz = recv(sockfd, incoming.read_buf, BUFFERSIZE, 0);
            incoming.read_pos = 0;
        }

        if (incoming.read_sz == 0) break;

        int i = incoming.read_pos, j = incoming.line_pos;
        while (i < incoming.read_sz) {
            if (incoming.read_buf[i] == '\n') {
                incoming.read_pos = i + 1;
                incoming.line_pos = j;
                if ((rc = handle_control_command(sockfd, &incoming)) < 0)
                    break;
                j = 0; i = incoming.read_pos;
            }
            else {
                incoming.line_buf[j++] = incoming.read_buf[i++];
            }
        }
        incoming.read_pos = i;
        incoming.line_pos = j;
    }

    close(sockfd);
}

int handle_control_command(int sockfd, struct LineBuffered *incoming)
{
    incoming->line_buf[incoming->line_pos] = '\0';
    printf("[%d] line: [%s]\n", sockfd, incoming->line_buf);

    const char *cmd = incoming->line_buf;
    int rc = -1;

    if (strncasecmp(cmd, "GET ", 4) == 0)
        rc = handle_get(sockfd, incoming->line_buf + 4);
    else if (strncasecmp(cmd, "PUT ", 4) == 0)
        rc = handle_put(sockfd, incoming->line_buf + 4, incoming);
    else if (strncasecmp(cmd, "EXEC ", 5) == 0)
        rc = handle_exec(sockfd, incoming->line_buf + 5);

    if (rc < 0)
        send(sockfd, "<error>\n", 8, 0);

    return rc;
}

int handle_get(int sockfd, const char *pathname) {
    struct stat buf;

    if (stat(pathname, &buf) < 0) {
        perror("stat");
        return -1;
    }

    if ((buf.st_mode & S_IFDIR) != 0) {
        return handle_get_dir(sockfd, pathname);
    }
    else {
        return handle_get_file(sockfd, pathname);
    }
}

int handle_get_dir(int sockfd, const char *pathname) {
    DIR *d = opendir(pathname);
    if (d == NULL) {
        perror("opendir");
        return -1;
    }

    FILE *resp = fdopen(sockfd, "w");

    struct dirent *dp;
    while ((dp = readdir(d)) != NULL) {
        fprintf(resp, "  %s\n", dp->d_name);
    }
    fflush(resp);
    closedir(d);
    return 0;
}

int handle_get_file(int sockfd, const char *pathname) {
    int infd = open(pathname, O_RDONLY);

    if (infd < 0) { perror("open"); return -1; }

    int rc = 0;

    while (1) {
        char buf[BUFFERSIZE];
        int sz = read(infd, buf, sizeof(buf));

        if (sz < 0) { perror("read"); rc = -1; break; }
        else if (sz == 0) break;

        int wz = send(sockfd, buf, sz, 0);
        if (wz < 0) { perror("send"); rc = -1; break; }
        else if (wz < sz)  /* should block */
        { /* ?? */ fprintf(stderr, "warning: short send\n"); }
    }

    close(infd);
    return rc;
}

int handle_put(int sockfd, const char *pathname,
               struct LineBuffered *incoming)
{
    int outfd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC);

    if (outfd < 0) { perror("open"); return -1; }

    if (incoming->read_pos < incoming->read_sz) {
        write(outfd, incoming->read_buf + incoming->read_pos,
                     incoming->read_sz - incoming->read_pos);
        incoming->read_pos = incoming->read_sz;
    }

    int rc = 0;

    while (1) {
        char buf[BUFFERSIZE];
        int sz = recv(sockfd, buf, sizeof(buf), 0);

        if (sz < 0) { perror("recv"); rc = -1; break; }
        else if (sz == 0) break;

        int wz = write(outfd, buf, sz);
        if (wz < 0) { perror("write stdout"); rc = -1; break; }
        else if (wz < sz) 
        { /* ?? */ fprintf(stderr, "warning: short write\n"); }
    }

    close(outfd);
    return rc;
}

int handle_exec(int sockfd, const char *command)
{
    int pipefd[2];

    if (pipe(pipefd) < 0) { perror("pipe"); return -1; }

    int pid = fork(), rc = 0;


    if (pid == 0) { /* child */
        dup2(pipefd[1], 1); close(sockfd); close(pipefd[0]);
        execl(SHELL, SHELL_NAME, "-c", command, NULL);
        /* error in exec */
        perror("execl"); close(1); exit(1);
    }
    else close(pipefd[1]);
    
    while (1) {
        char buf[BUFFERSIZE];
        int sz = read(pipefd[0], buf, sizeof(buf));

        if (sz < 0) { perror("read"); rc = -1; break; }
        else if (sz == 0) break;

        int wz = send(sockfd, buf, sz, 0);
        if (wz < 0) { perror("send"); rc = -1; break; }
        else if (wz < sz)  /* should block */
        { /* ?? */ fprintf(stderr, "warning: short send\n"); }
    }
    
    close(pipefd[0]);

    if (waitpid(pid, &rc, 0) < 0) { /** @todo prev rc is lost */
        perror("waitpid"); return -1;
    }

    printf("[%d] exit (%x)\n", sockfd, rc);

    return rc ? -1 : 0;
}
