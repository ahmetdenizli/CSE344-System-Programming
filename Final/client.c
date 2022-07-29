/* Ahmet Denizli 161044020 */
/*      CSE344 FINAL       */
/*       16/06/2022        */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "utilities.h"

#define MAX_REQ_SIZE 100

struct ThreadParams {
    int threadNum;
    char request[BUF_SIZE];
};

pthread_mutex_t mutex;
pthread_cond_t condition;

int totalReq = 0, startedThreadCount = 0, PORT;
char ipAdrr[20];

void *clientThreadFun(void *arg);

int main(int argc, char *argv[]) {
    int opt, i, fd, n;
    char c, str[300], reqFilePath[100], requests[MAX_REQ_SIZE][BUF_SIZE];

    while ((opt = getopt(argc, argv, ":r:q:s:")) != -1) {
        switch (opt) {
            case 'r':
                strcpy(reqFilePath, optarg);
                break;
            case 'q':
                for(i=0;i<(int)strlen(optarg);++i){
                    if(optarg[i]<48 || optarg[i]>57)
                        errExit("client.c -q port argument must be non negative number");
                }
                PORT = atoi(optarg);
                break;
            case 's':
                strcpy(ipAdrr, optarg);
                break;
            case ':':
                errno = EINVAL;
                errExit("You must write like this: \"./client -r requestFile -q PORT -s IP\"");
                break;
            case '?':
                errno = EINVAL;
                errExit("You must write like this: \"./client -r requestFile -q PORT -s IP\"");
                break;
            default:
                break;
        }
    }
    if(optind!=7){
        errno=EINVAL;
        errExit("You must write like this: \"./client -r requestFile -q PORT -s IP\"");
    }

    fd = open(reqFilePath, O_RDWR | O_SYNC);
    if (fd < 0)
    {
        errExit("client.c - Cannot open request filePath");
    }

    if(pthread_mutex_init(&mutex,NULL)!=0){
        errExit("Mutex initialize error");
    }
    if(pthread_cond_init(&condition,NULL)!=0){
        errExit("Condition initialize error");
    }
    /* set STDOUT without buffering */
    setbuf(stdout,NULL);

    i = 0;
    totalReq = 0;
    while ((n = read(fd, &c, 1)) > 0) {
        if(totalReq == MAX_REQ_SIZE)
            break;

        if(c == '\n' || c == '\r'){
            if(i != 0) {
                requests[totalReq][i] = '\0';
                i = 0;
                totalReq++;
            }
            continue;
        }
        if(i >= BUF_SIZE-1){
            close(fd);
            errno=EINVAL;
            perror("Max request length is 200. One of the request in the file is too big.");
            exit(EXIT_FAILURE);
        }

        requests[totalReq][i] = c;
        i++;
    }

    sprintf(str,"Client: I have loaded %d requests and I’m creating %d threads.\n", totalReq, totalReq);
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }

    /* Create threads */
    pthread_t tid_client[totalReq];
    struct ThreadParams params[totalReq];
    for (i = 0; i < totalReq; i++) {
        params[i].threadNum = i;
        strcpy(params[i].request, requests[i]);

        if (pthread_create (&tid_client[i], NULL, clientThreadFun, (void *) &params[i]) != 0) {
            errExit ("pthread_create error");
        }
    }

    /* join for consumer to terminate */
    for (i = 0; i < totalReq; i++){
        if (pthread_join (tid_client[i], NULL) != 0) {
            errExit("pthread_join error");
        }
    }

    sprintf(str,"Client: All threads have terminated, goodbye.\n");
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }

    if(pthread_mutex_destroy(&mutex)!=0){
        pthread_mutex_unlock(&mutex);
        if(pthread_mutex_destroy(&mutex)!=0){
            errExit("Mutex destroy error");
        }
    }
    if(pthread_cond_destroy(&condition)!=0){
        errExit("Condition destroy error");
    }
}

/* Function for threads */
void *clientThreadFun(void *arg){
    char str[600], buf[BUF_SIZE];
    struct ThreadParams *params = arg;
    int sfd;
    struct sockaddr_in servaddr;

    sprintf(str,"Client-Thread-%d: Thread-%d has been created\n", params->threadNum, params->threadNum);
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }

    pthread_mutex_lock(&mutex);
    startedThreadCount++;
    while (startedThreadCount < totalReq){
        pthread_cond_wait(&condition, &mutex);
    }
    pthread_cond_broadcast(&condition);
    pthread_mutex_unlock(&mutex);

    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
        errExit("client.c socket creation error");
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ipAdrr);
    servaddr.sin_port = htons(PORT);

    if (connect(sfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        errExit("connection with the server failed.");
    }

    sprintf(str,"Client-Thread-%d: I am requesting \"%s\"\n", params->threadNum, params->request);
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }

    write(sfd, "0", 2);
    read(sfd, buf, sizeof(buf));
    bzero(buf, sizeof(buf));

    write(sfd, params->request, strlen(params->request));
    bzero(buf, sizeof(buf));

    /* Reads the response */
    read(sfd, buf, sizeof(buf));

    sprintf(str,"Client-Thread-%d: The server’s response to \"%s\" is %s\n", params->threadNum, params->request, buf);
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }

    sprintf(str,"Client-Thread-%d: Terminating\n", params->threadNum);
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }

    close(sfd);
    return NULL;
}