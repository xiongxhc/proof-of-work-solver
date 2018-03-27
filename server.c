/*************************************
 * -----Computer Systems Project-----
 * By: HangChen Xiong 
 *************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#include "uint256.h"
#include "sha256.h"

#define LOG_FILE "log.txt"

#define TIME_BUFFER 100
#define MAX_BUF 256
#define MAX_ARGC 10
#define SPACES " \r\n"

typedef struct {
    int fd; // the client file description
    char ip[16]; // the client IP address
    char req[MAX_BUF]; // request received from client
    char res[MAX_BUF]; // response send to client
    int index;

    int abort; // indicate if should abort
    int *disconnect;

    char tmp[MAX_BUF]; // used to strtok
    char *argv[MAX_ARGC]; // split the request to argv
    int argc;

    BYTE target[32];
    char *seed;
    unsigned long solution;
} thread_data;

// use for the work thread
typedef struct {
    thread_data *data;
    unsigned long solution; // the solution
    int step; // step
} thread_data1;

// use for logging time
pthread_mutex_t lock;

FILE *fp;

pthread_t client_threads[100];
int client_count = 0;

// handle the new client connection
void new_client(int fd, struct sockaddr_in *addr, int index);

// the thread func
void *thread_func(void *p);
void *thread_func1(void *p);

// execute the client command
void execute_command(thread_data *data);

// record the request received from client
void logging_req(thread_data *data);

// record the response send to client
void logging_res(thread_data *data);

// transfer difficulty to target
void transfer_difficulty(char *d, BYTE *uint256);

// check if it is a solution
int check_solution(BYTE *target, char *s, unsigned long l);

int main(int argc, char *argv[]) {
    // check the command line argument
    if (argc < 2) {
        fprintf(stderr, "prompt: ./server [port number]\n");
        exit(1);
    }

    int port = atoi(argv[1]);

    // create and set the server address
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // open the server socket
    int sockfd = socket(AF_INET,SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    // bind address to the socket
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    // listen
    listen(sockfd, 5);

    // open the log.txt
    fp = fopen(LOG_FILE, "wb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }

    // the server handler
    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);

        int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len);
        if (newsockfd < 0) {
            perror("accpet");
            exit(1);
        }

        if (client_count >= 100) {
            close(newsockfd);
            continue;
        }

        int index = 0;
        for (int i = 0; i < 100; i++) {
            if (client_threads[i] == 0) {
                index = 1;
                break;
            }
        }

        new_client(newsockfd, &cli_addr, index);
    }

    return 0;
}

void new_client(int fd, struct sockaddr_in *addr, int index) {
    // create a thread_data and create a new thread
    thread_data *data = calloc(1, sizeof(thread_data));
    data->fd = fd;
    data->index = index;
    data->disconnect = (int*)malloc(sizeof(int));
    *(data->disconnect) = 0;
    strcpy(data->ip, inet_ntoa(addr->sin_addr));

    client_count++;

    if (pthread_create(&(client_threads[index]), NULL, thread_func, (void *)data) < 0) {
        perror("pthread_create");
        exit(1);
    }
}

void *thread_func(void *p) {
    // resume the thread_data
    thread_data *data = (thread_data *)p;

    char buffer[256];

    while (1) {
        if (*(data->disconnect)) {
            break;
        }

        bzero(buffer, 256);
        // read a request from socket
        int n = 0;
        n = read(data->fd, buffer, sizeof(data->req));
        if (n < 0) {
            // read failed
            perror("read");
            break;
        }

        if (buffer[0] == '\0') {
            break;
        }

        memcpy(data->req, buffer, 256);

        // add a '\0' after request
        // data->req[n] = '\0';

        // split request into argv, copy the request to strtok
        memcpy(data->tmp, data->req, 256);
        data->argc = 0;
        char *t = strtok(data->tmp, SPACES);
        while (data->argc < MAX_ARGC && t) {
            data->argv[data->argc++] = t;
            t = strtok(NULL, SPACES);
        }

        // save log
        logging_req(data);

        // execute the command
        execute_command(data);
    }

    close(data->fd);

    pthread_mutex_lock(&lock);
    client_threads[data->index] = 0;
    client_count--;
    pthread_mutex_unlock(&lock);

    pthread_exit(NULL);

}

void logging_req(thread_data *data) {
    // critical section 
    pthread_mutex_lock(&lock); 
    // logging time
    char time_buff[TIME_BUFFER];
    time_t curtime = time (0);
    strftime (time_buff, TIME_BUFFER, "%d/%m/%Y %H:%M:%S]", localtime (&curtime));
    // create logging detail [client to server]
    fprintf(fp, "[Time:%s [server:0.0.0.0] [client:%s] [socketID:%d] [client=>server] %s\n",
            time_buff, data->ip, data->fd, data->req);
    fflush(fp);
    pthread_mutex_unlock(&lock);
}

void logging_res(thread_data *data) {
    // critical section 
    pthread_mutex_lock(&lock); 
    // logging time
    char time_buff[TIME_BUFFER];
    time_t curtime = time (0);
    strftime (time_buff, TIME_BUFFER, "%d/%m/%Y %H:%M:%S]", localtime (&curtime));
    // create logging detail [server to client]
    fprintf(fp, "[Time:%s [server:0.0.0.0] [client:%s] [socketID:%d] [server=>client] %s\n",
            time_buff, data->ip, data->fd, data->res);
    fflush(fp);
    pthread_mutex_unlock(&lock);
}

void execute_command(thread_data *data) {
    int n = 0;
    if (data->argc == 1 && strcmp(data->argv[0], "PING") == 0) {
        // received PING, response PONG
        sprintf(data->res, "PONG\r\n");
        n = write(data->fd, data->res, 6);
        logging_res(data);
    } else if (data->argc == 4 && strcmp(data->argv[0], "SOLN") == 0) {
        if ((strlen(data->argv[1]) != 8) ||
            (strlen(data->argv[2]) != 64) ||
            (strlen(data->argv[3]) != 16)) {
            // write error
            sprintf(data->res, "ERRO unknown command or invalid params\r\n");
            n = write(data->fd, data->res, 40);
            logging_res(data);
        } else {
        // received SOLN, check if 
        transfer_difficulty(data->argv[1], data->target);
        data->seed = data->argv[2];
        sscanf(data->argv[3], "%lx", &data->solution);
        if (check_solution(data->target, data->seed, data->solution)) {
            sprintf(data->res, "OKAY\r\n");
            n = write(data->fd, data->res, 6);
        } else {
            sprintf(data->res, "ERRO                  invalid solution\r\n");
            n = write(data->fd, data->res, 40);
        }
        logging_res(data);
        }
    } else if (data->argc == 5 && strcmp(data->argv[0], "WORK") == 0) {
        if ((strlen(data->argv[1]) != 8) ||
            (strlen(data->argv[2]) != 64) ||
            (strlen(data->argv[3]) != 16) ||
            (strlen(data->argv[4]) != 2)) {
            // write error
            sprintf(data->res, "ERRO unknown command or invalid params\r\n");
            n = write(data->fd, data->res, 40);
            logging_res(data);
        } else {
            // received WORK, do it
            data->abort = 0;
            transfer_difficulty(data->argv[1], data->target);
            data->seed = data->argv[2];
            sscanf(data->argv[3], "%lx", &data->solution);

            // new pthreads to do the work
            pthread_t pid;
            thread_data1 *data1 = calloc(1, sizeof(thread_data));
            data1->data = data;
            data1->solution = data->solution;
            data1->step = 1;
            if (pthread_create(&pid, NULL, thread_func1, data1) < 0) {
                perror("pthread_create");
                exit(1);
            }
        }

         
    } else if (data->argc == 1 && strcmp(data->argv[0], "ABRT") == 0) {
        // set the abort flag and response OK
        data->abort = 1;
        sprintf(data->res, "OKAY\r\n");
        n = write(data->fd, data->res, 6);
        logging_res(data);
    } else {
        sprintf(data->res, "ERRO unknown command or invalid params\r\n");
        n = write(data->fd, data->res, 40);
        logging_res(data);
    }

    if (n < 0) {
        *(data->disconnect) = 1;
    }
}

// transfer difficulty to target
void transfer_difficulty(char *d, BYTE *uint256) {
    // the first 2 bytes of d is a
    char s[16];
    strcpy(s, d);
    int a = 0, b = 0;
    sscanf(s + 2, "%x", &b);
    s[2] = '\0';
    sscanf(s, "%x", &a);

    // uint256 = b << (8 * (a - 3));
    BYTE bb[32];
    uint256_init(uint256);
    uint256_init(bb);
    bb[31] = b & 0xff;
    bb[30] = (b >> 8) & 0xff;
    bb[29] = (b >> 16) & 0xff;
    uint256_sl(uint256, bb, 8 * (a - 3));
}

// check if it is a solution
int check_solution(BYTE *target, char *s, unsigned long l) {
    char ss[81];
    strcpy(ss, s);
    sprintf(ss + 64, "%016lx", l);

    // transfer string to BYTE[]
    BYTE b[40];

    int i = 0;
    for (i = 78; i >= 0; i -= 2) {
        unsigned t;
        sscanf(ss + i, "%x", &t);
        b[i/2] = t & 0xff;
        ss[i] = '\0';
    }

    // sha256(sha256(b))
    BYTE c[32];
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, b, 40);
    sha256_final(&ctx, c);

    BYTE d[32];
    SHA256_CTX ctx1;
    sha256_init(&ctx1);
    sha256_update(&ctx1, c, 32);
    sha256_final(&ctx1, d);

    // compare target with d
    for (i = 0; i < 32; i++) {
        if (target[i] == d[i]) {
            continue;
        } else if (target[i] > d[i]) {
            return 1;
        } else {
            return 0;
        }
    }
    return 0;
}

void *thread_func1(void *p) {
    thread_data1 *data1 = (thread_data1 *)p;
    // if not abort, try doing
    while (data1->data->abort == 0) {
        if (check_solution(data1->data->target, data1->data->seed, data1->solution)) {
            // find a solution
            if (data1->data->abort == 0) {
                data1->data->abort = 1;
                sprintf(data1->data->res, "SOLN %s %s %016lx\r\n", data1->data->argv[1], data1->data->argv[2], data1->solution);
                int n = write(data1->data->fd, data1->data->res, 97);
                if (n < 0) {
                    *(data1->data->disconnect) = 1;
                }
                logging_res(data1->data);
            }
            break;
        } else {
            data1->solution += data1->step;
        }
    }
    return NULL;
}
