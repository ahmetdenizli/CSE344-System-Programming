/* Ahmet Denizli 161044020 */
/*      CSE344 FINAL       */
/*       16/06/2022        */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "utilities.h"
#include "queue.h"

/*Servant data structure, contains Servants pid, port and responsible cities */
struct Servant {
    char servantPid[10];
    int servantPort;
    char city1[50];
    char city2[50];
    struct Servant* next;
};

/* Function for sigint handler. Sets sigint_flag to 1 */
void sigint_handler(int signum);
/* Function for pool threads. Gets thread num as argument.     */
/* Waits condition signal from server. When it gets signal     */
/* gets request from client, collect the response and sends it */
void *handlingThreadFun(void *arg);

/* mutexes array, there is one mutex for each pool thread*/
pthread_mutex_t* mutexes;
/* mutex for queue operations, enqueue and dequeue */
pthread_mutex_t poolThreadMtx;
/* condition variable for waiting and signaling*/
pthread_cond_t condition;
/* queue for containing connection fds */
struct Queue *reqQueue;
/* head pointer for Servant struct linked list */
struct Servant *servantHead = NULL;

/* variables for sigint and total handled requests number */
int sigint_flag = 0, total_handled_req = 0;

int main(int argc, char *argv[]) {
    int opt, PORT, ThreadNum, i, sfd, connfd;
    char str[300], timestamp[25];
    struct sockaddr_in servaddr, cli;
    socklen_t len;
    time_t current_time;
    struct tm tm;

    while ((opt = getopt(argc, argv, ":p:t:")) != -1) {
        switch (opt) {
            case 'p':
                for(i=0;i<(int)strlen(optarg);++i){
                    if(optarg[i]<48 || optarg[i]>57)
                        errExit("servant.c -p port argument must be non negative number");
                }
                PORT=atoi(optarg);
                break;
            case 't':
                ThreadNum=atoi(optarg);
                if(ThreadNum < 5)
                    errExit("server.c -t argument: numberOfThreads must be at least 5");
                break;
            case ':':
                errno = EINVAL;
                errExit("You must write like this: \"./server -p PORT -t numberOfThreads\"");
                break;
            case '?':
                errno = EINVAL;
                errExit("You must write like this: \"./server -p PORT -t numberOfThreads\"");
                break;
            default:
                break;
        }
    }
    if(optind!=5){
        errno=EINVAL;
        errExit("You must write like this: \"./server -p PORT -t numberOfThreads\"");
    }

    mutexes=(pthread_mutex_t*)calloc(ThreadNum,sizeof(pthread_mutex_t));

    for(i=0; i<ThreadNum; ++i){
        if(pthread_mutex_init(&mutexes[i],NULL)!=0){
            errExit("Mutex initialize error");
        }
    }
    if(pthread_mutex_init(&poolThreadMtx,NULL)!=0){
        errExit("Mutex initialize error");
    }
    if(pthread_cond_init(&condition,NULL)!=0){
        errExit("Condition initialize error");
    }

    struct sigaction act;
    /* Handler settings */
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = sigint_handler;
    /* sigaction for SIGINT */
    if (sigaction(SIGINT, &act, NULL) != 0){
        errExit("sigaction function error");
    }

    reqQueue = create_queue(500);

    int thread_no[ThreadNum];
    pthread_t tid_arr[ThreadNum];
    for (i = 0; i < ThreadNum; i++) {
        thread_no [i] = i;
        if (pthread_create (&tid_arr[i], NULL, handlingThreadFun, (void *) &thread_no[i]) != 0) {
            errExit ("pthread_create error");
        }
    }

    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
        errExit("server.c socket creation error");
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    if ((bind(sfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
        errExit("server.c socket bind failed...");

    if ((listen(sfd, 128)) != 0)
        errExit("server.c socket listen failed...");

    len = sizeof(cli);
    while(sigint_flag == 0){
        connfd = accept(sfd, (struct sockaddr*)&cli, &len);
        if (sigint_flag == 1){
            pthread_cond_broadcast(&condition);
            break;
        }
        if (connfd == -1)
            continue;

        pthread_mutex_lock(&poolThreadMtx);
        enqueue(reqQueue, connfd);
        pthread_mutex_unlock(&poolThreadMtx);

        pthread_cond_signal(&condition);
    }

    /* join for threads to terminate */
    for (i = 0; i < ThreadNum; i++){
        if (pthread_join (tid_arr[i], NULL) != 0) {
            errExit("pthread_join error");
        }
    }
    current_time=time(NULL);
    tm = *localtime(&current_time);
    sprintf(timestamp,"%02d/%02d/%04d %02d:%02d:%02d", tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900, tm.tm_hour, tm.tm_min, tm.tm_sec);

    sprintf(str,"[%s]SIGINT  has been received. I handled a total of %d requests. Goodbye.\n", timestamp, total_handled_req);
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }

    /* close and free resources */
    close(sfd);
    free_queue(reqQueue);
    struct Servant *current = servantHead;
    struct Servant *temp = NULL;
    while (current != NULL){
        kill(atoi(current->servantPid), SIGINT);
        temp = current->next;
        free(current);
        current = temp;
    }
    for(i=0; i<ThreadNum; ++i){
        if(pthread_mutex_destroy(&mutexes[i]) != 0){
            pthread_mutex_unlock(&mutexes[i]);
            if(pthread_mutex_destroy(&mutexes[i]) != 0){
                errExit("Mutex destroy error");
            }
        }
    }
    free(mutexes);
    if(pthread_mutex_destroy(&poolThreadMtx) != 0){
        pthread_mutex_unlock(&poolThreadMtx);
        if(pthread_mutex_destroy(&poolThreadMtx) != 0){
            errExit("Mutex destroy error");
        }
    }
    if(pthread_cond_destroy(&condition)!=0){
        errExit("Condition destroy error");
    }
    exit(EXIT_SUCCESS);
}

/* Function for pool threads. Gets thread num as argument.     */
/* Waits condition signal from server. When it gets signal     */
/* gets request from client, collect the response and sends it */
void *handlingThreadFun(void *arg){
    char str[300], buf[BUF_SIZE], buf2[BUF_SIZE], timestamp[25], tr_req_type[50], tr_type[50], tr_city[50], tr_date1[50], tr_date2[50];
    int threadNum = *((int*)arg);
    int servant_fd, client_fd, response, ssret;
    time_t current_time;
    struct tm tm;
    struct sockaddr_in servant_addr;

    pthread_mutex_lock(&mutexes[threadNum]);
    while (sigint_flag==0){
        pthread_cond_wait(&condition, &mutexes[threadNum]);
        if (sigint_flag==1){
            pthread_mutex_unlock(&mutexes[threadNum]);
            return NULL;
        }

        pthread_mutex_lock(&poolThreadMtx);
        client_fd = dequeue(reqQueue);
        pthread_mutex_unlock(&poolThreadMtx);
        while(client_fd != -1 && sigint_flag==0){
            bzero(buf, sizeof(buf));
            read(client_fd, buf, sizeof(buf));

            if (strcmp(buf, "0") == 0){
                write(client_fd, "-", 2);

                read(client_fd, buf, sizeof(buf));
            } else if (strcmp(buf, "1") == 0){
                write(client_fd, "-", 2);

                struct Servant *srvnt=(struct Servant*)malloc(sizeof(struct Servant));

                read(client_fd, buf, sizeof(buf));
                strcpy(srvnt->servantPid, buf);
                write(client_fd, "-", 2);

                read(client_fd, buf, sizeof(buf));
                srvnt->servantPort = atoi(buf);
                write(client_fd, "-", 2);

                read(client_fd, buf, sizeof(buf));
                strcpy(srvnt->city1, buf);
                write(client_fd, "-", 2);

                read(client_fd, buf, sizeof(buf));
                strcpy(srvnt->city2, buf);
                write(client_fd, "-", 2);

                current_time=time(NULL);
                tm = *localtime(&current_time);
                sprintf(timestamp,"%02d/%02d/%04d %02d:%02d:%02d", tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900, tm.tm_hour, tm.tm_min, tm.tm_sec);

                sprintf(str,"[%s]Servant %s present at port %d handling cities %s-%s\n", timestamp, srvnt->servantPid, srvnt->servantPort, srvnt->city1, srvnt->city2);
                if(write(STDOUT_FILENO,str,strlen(str))<0){
                    errExit("write function error");
                }

                close(client_fd);

                pthread_mutex_lock(&poolThreadMtx);
                srvnt->next = servantHead;
                servantHead = srvnt;

                client_fd = dequeue(reqQueue);
                pthread_mutex_unlock(&poolThreadMtx);
                continue;
            } else {
                close(client_fd);
                pthread_mutex_lock(&poolThreadMtx);
                client_fd = dequeue(reqQueue);
                pthread_mutex_unlock(&poolThreadMtx);
                continue;
            }

            current_time=time(NULL);
            tm = *localtime(&current_time);
            sprintf(timestamp,"%02d/%02d/%04d %02d:%02d:%02d", tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900, tm.tm_hour, tm.tm_min, tm.tm_sec);

            sprintf(str,"[%s]Request arrived \"%s\"\n", timestamp, buf);
            if(write(STDOUT_FILENO,str,strlen(str))<0){
                errExit("write function error");
            }
            response = 0;
            struct Servant *current = servantHead;

            ssret = sscanf(buf, "%s %s %s %s %s", tr_req_type, tr_type, tr_date1, tr_date2, tr_city);
            int flag = 0;
            if (ssret == 5){
                while (current != NULL){
                    if (strcmp(current->city1, tr_city) <= 0 && strcmp(current->city2, tr_city) >= 0){
                        current_time=time(NULL);
                        tm = *localtime(&current_time);
                        sprintf(timestamp,"%02d/%02d/%04d %02d:%02d:%02d", tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900, tm.tm_hour, tm.tm_min, tm.tm_sec);

                        sprintf(str,"[%s]Contacting servant %s\n", timestamp, current->servantPid);
                        if(write(STDOUT_FILENO,str,strlen(str))<0){
                            errExit("write function error");
                        }

                        servant_fd = socket(AF_INET, SOCK_STREAM, 0);
                        if (servant_fd == -1)
                            errExit("servant.c socket creation error");
                        bzero(&servant_addr, sizeof(servant_addr));

                        servant_addr.sin_family = AF_INET;
                        servant_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                        servant_addr.sin_port = htons(current->servantPort);

                        if (connect(servant_fd, (struct sockaddr*)&servant_addr, sizeof(servant_addr)) != 0) {
                            errExit("server.c connection with the servant failed");
                        }

                        write(servant_fd, buf, strlen(buf));
                        bzero(buf, sizeof(buf));
                        read(servant_fd, buf, sizeof(buf));
                        response = atoi(buf);
                        close(servant_fd);
                        flag = 1;
                        break;
                    }
                    current = current->next;
                }
            } else{
                current_time=time(NULL);
                tm = *localtime(&current_time);
                sprintf(timestamp,"%02d/%02d/%04d %02d:%02d:%02d", tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900, tm.tm_hour, tm.tm_min, tm.tm_sec);

                sprintf(str,"[%s]Contacting ALL servants\n", timestamp);
                if(write(STDOUT_FILENO,str,strlen(str))<0){
                    errExit("write function error");
                }

                while (current != NULL){
                    servant_fd = socket(AF_INET, SOCK_STREAM, 0);
                    if (servant_fd == -1)
                        errExit("servant.c socket creation error");
                    bzero(&servant_addr, sizeof(servant_addr));

                    servant_addr.sin_family = AF_INET;
                    servant_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                    servant_addr.sin_port = htons(current->servantPort);

                    if (connect(servant_fd, (struct sockaddr*)&servant_addr, sizeof(servant_addr)) != 0) {
                        errExit("server.c connection with the servant failed");
                    }

                    write(servant_fd, buf, strlen(buf));
                    bzero(buf2, sizeof(buf2));
                    read(servant_fd, buf2, sizeof(buf2));
                    response = response + atoi(buf2);
                    close(servant_fd);
                    current = current->next;
                }
            }

            bzero(buf, sizeof(buf));
            if (ssret == 5 && flag == 0){
                sprintf(buf, "Error, there is no servant responsible for %s", tr_city);
            }else{
                sprintf(buf, "%d", response);
            }
            write(client_fd, buf, strlen(buf));


            current_time=time(NULL);
            tm = *localtime(&current_time);
            sprintf(timestamp,"%02d/%02d/%04d %02d:%02d:%02d", tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
            sprintf(str,"[%s]Response received: %s, forwarded to client\n", timestamp, buf);
            if(write(STDOUT_FILENO,str,strlen(str))<0){
                errExit("write function error");
            }
            total_handled_req++;

            close(client_fd);
            pthread_mutex_lock(&poolThreadMtx);
            client_fd = dequeue(reqQueue);
            pthread_mutex_unlock(&poolThreadMtx);
        }
    }
    pthread_mutex_unlock(&mutexes[threadNum]);

    return NULL;
}

/* Function for sigint handler. Sets sigint_flag to 1 */
void sigint_handler(int signum){
    sigint_flag = 1;
}