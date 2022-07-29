/* Ahmet Denizli 161044020 */
/*       CSE344 HW4        */
/*       14/05/2022        */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <pthread.h>

/* Function for Supplier thread */
void *supplierFun(void *arg);
/* Function for Consumer threads */
void *consumerFun(void *arg);
/* This function returns current timestamp in seconds */
unsigned long get_time_seconds();
/* This function returns current timestamp in microseconds */
unsigned long get_time_microseconds();
/* This function prints given error via perror then exits. */
void errExit(char *s);
/* Sigint handler for wholesaler */
void sigint_handler(int signum);

int sems;
int  sigint_flag = 0, fd, N, ConsumerNum;

union semun
{
    int val;
    struct semid_ds *buf;
    ushort array [1];
};

int main(int argc, char *argv[]) {
    int opt, i = 0;
    char inputFilePath[100];

    while ((opt = getopt(argc, argv, ":C:N:F:")) != -1) {
        switch (opt) {
            case 'C':
                ConsumerNum=atoi(optarg);
                if(ConsumerNum<5){
                    errno=EINVAL;
                    errExit("C must be greater than 4.");
                }
                break;
            case 'N':
                N=atoi(optarg);
                if(N<2){
                    errno=EINVAL;
                    errExit("N must be greater than 1.");
                }
                break;
            case 'F':
                strcpy(inputFilePath, optarg);
                break;
            case ':':
                errno = EINVAL;
                errExit("You must write like this: \"./hw4 -C 10 -N 5 -F inputfilePath\"");
                break;
            case '?':
                errno = EINVAL;
                errExit("You must write like this: \"./hw4 -C 10 -N 5 -F inputfilePath\"");
                break;
            default:
                abort();
        }
    }
    if (optind != 7) {
        errno = EINVAL;
        errExit("You must write like this: \"./hw4 -C 10 -N 5 -F inputfilePath\"");
    }

    fd = open(inputFilePath, O_RDWR | O_SYNC);
    if (fd < 0)
    {
        errExit("hw4.c main - Open file error");
    }

    struct sigaction act;
    /* Handler settings */
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    act.sa_handler = sigint_handler;
    /* sigaction for SIGINT */
    if (sigaction(SIGINT, &act, NULL) != 0){
        errExit("sigaction function error");
    }

    /* set STDOUT without buffering */
    setbuf(stdout,NULL);

    /* Creates a semaphores */
    if ((sems = semget (IPC_PRIVATE, 2, 0660 | IPC_CREAT)) == -1) {
        errExit("semget error");
    }

    union semun sem_val;
    /* mutex_sem initialized by 0 */
    sem_val.val = 0;
    if (semctl (sems, 0, SETVAL, sem_val) == -1) {
        errExit ("semctl error\n");
    }
    if (semctl (sems, 1, SETVAL, sem_val) == -1) {
        errExit ("semctl error\n");
    }

    /* Create C consumer threads */
    int thread_no[ConsumerNum];
    pthread_t tid_consumer[ConsumerNum];
    for (i = 0; i < ConsumerNum; i++) {
        thread_no [i] = i;
        if (pthread_create (&tid_consumer[i], NULL, consumerFun, (void *) &thread_no[i]) != 0) {
            errExit ("pthread_create error");
        }
    }

    pthread_t supplierThread;
    if (pthread_create (&supplierThread, NULL, supplierFun, NULL) != 0) {
        errExit ("pthread_create error");
    }
    if (pthread_detach(supplierThread) != 0) {
        errExit ("pthread_detach error");
    }

    /* join for consumer to terminate */
    for (i = 0; i < ConsumerNum; i++){
        if (pthread_join (tid_consumer[i], NULL) != 0) {
            errExit("pthread_join error");
        }
    }

    if (semctl (sems, 0, IPC_RMID) == -1) {
        perror ("semctl IPC_RMID"); exit (1);
    }
    /* Closes the used files */
    if(close(fd)<0){
        errExit("close file error");
    }

    exit(0);
}

void *supplierFun(void *arg){
    int amount_1=0, amount_2=0, k=0;
    ssize_t n;
    char str[300], c;
    union semun sem_val;

    struct sembuf ops1;
    struct sembuf ops2;
    ops1.sem_num = 0;
    ops2.sem_num = 1;
    ops1.sem_op = 1;
    ops2.sem_op = 1;
    ops1.sem_flg = 0;
    ops2.sem_flg = 0;

    while ((n = read(fd, &c, 1)) > 0) {
        if(sigint_flag==1){
            for (k = 0; k < ConsumerNum; ++k) {
                if (semop (sems, &ops1, 1) == -1) {
                    errExit("supplierFun semop error");
                }
                if (semop (sems, &ops2, 1) == -1) {
                    errExit("supplierFun semop error");
                }
            }
            break;
        }

        if(k >= 2*N*ConsumerNum)
            break;
        if(c == '1'){
            if ((amount_1 = semctl (sems, 0, GETVAL, sem_val)) == -1 || (amount_2 = semctl (sems, 1, GETVAL, sem_val)) == -1) {
                errExit ("supplierFun semctl error\n");
            }

            sprintf(str,"[%lu]Supplier: read from input a ‘1’. Current amounts: %d x ‘1’, %d x ‘2’.\n", get_time_microseconds(),amount_1,amount_2);
            if(write(STDOUT_FILENO,str,strlen(str))<0){
                errExit("write function error");
            }

            if (semop (sems, &ops1, 1) == -1) {
                errExit("supplierFun semop error");
            }
            if(sigint_flag==1){
                for (k = 0; k < ConsumerNum; ++k) {
                    if (semop (sems, &ops1, 1) == -1) {
                        errExit("supplierFun semop error");
                    }
                    if (semop (sems, &ops2, 1) == -1) {
                        errExit("supplierFun semop error");
                    }
                }
                break;
            }
            if ((amount_1 = semctl (sems, 0, GETVAL, sem_val)) == -1 || (amount_2 = semctl (sems, 1, GETVAL, sem_val)) == -1) {
                errExit ("supplierFun semctl error\n");
            }

            sprintf(str,"[%lu]Supplier: delivered a ‘1’. Post-delivery amounts: %d x ‘1’, %d x ‘2’.\n", get_time_microseconds(),amount_1,amount_2);
            if(write(STDOUT_FILENO,str,strlen(str))<0){
                errExit("write function error");
            }
            k++;
        }
        else if(c == '2'){
            if ((amount_1 = semctl (sems, 0, GETVAL, sem_val)) == -1 || (amount_2 = semctl (sems, 1, GETVAL, sem_val)) == -1) {
                errExit ("supplierFun semctl error\n");
            }

            sprintf(str,"[%lu]Supplier: read from input a ‘2’. Current amounts: %d x ‘1’, %d x ‘2’.\n", get_time_microseconds(),amount_1,amount_2);
            if(write(STDOUT_FILENO,str,strlen(str))<0){
                errExit("write function error");
            }

            if (semop (sems, &ops2, 1) == -1) {
                errExit("supplierFun semop error");
            }
            if(sigint_flag==1){
                for (k = 0; k < ConsumerNum; ++k) {
                    if (semop (sems, &ops1, 1) == -1) {
                        errExit("supplierFun semop error");
                    }
                    if (semop (sems, &ops2, 1) == -1) {
                        errExit("supplierFun semop error");
                    }
                }
                break;
            }

            if ((amount_1 = semctl (sems, 0, GETVAL, sem_val)) == -1 || (amount_2 = semctl (sems, 1, GETVAL, sem_val)) == -1) {
                errExit ("supplierFun semctl error\n");
            }

            sprintf(str,"[%lu]Supplier: delivered a ‘2’. Post-delivery amounts: %d x ‘1’, %d x ‘2’.\n", get_time_microseconds(),amount_1,amount_2);
            if(write(STDOUT_FILENO,str,strlen(str))<0){
                errExit("write function error");
            }
            k++;
        } else{
            continue;
        }
    }

    sprintf(str,"[%lu]The Supplier has left.\n", get_time_microseconds());
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }

    return NULL;
}

void *consumerFun(void *arg){
    int i, amount_1, amount_2;
    char str[300];
    union semun sem_val;

    struct sembuf ops[2];
    ops[0].sem_num = 0;
    ops[1].sem_num = 1;
    ops[0].sem_op = -1;
    ops[1].sem_op = -1;
    ops[0].sem_flg = 0;
    ops[1].sem_flg = 0;

    for (i = 0; i < N; ++i) {
        if(sigint_flag==1)
            break;

        if ((amount_1 = semctl (sems, 0, GETVAL, sem_val)) == -1 || (amount_2 = semctl (sems, 1, GETVAL, sem_val)) == -1) {
            errExit ("consumerFun semctl error\n");
        }

        sprintf(str,"[%lu]Consumer-%d at iteration %d (waiting). Current amounts: %d x ‘1’, %d x ‘2’.\n", get_time_microseconds(),*((int *)arg),i,amount_1,amount_2);
        if(write(STDOUT_FILENO,str,strlen(str))<0){
            errExit("write function error");
        }

        if (semop (sems, ops, 2) == -1) {
            errExit("consumerFun semop error");
        }
        if(sigint_flag==1)
            break;

        if ((amount_1 = semctl (sems, 0, GETVAL, sem_val)) == -1 || (amount_2 = semctl (sems, 1, GETVAL, sem_val)) == -1) {
            errExit ("consumerFun semctl error\n");
        }
        sprintf(str,"[%lu]Consumer-%d at iteration %d (consumed). Post-consumption amounts: %d x ‘1’, %d x ‘2’.\n", get_time_microseconds(),*((int *)arg),i,amount_1,amount_2);
        if(write(STDOUT_FILENO,str,strlen(str))<0){
            errExit("write function error");
        }
    }

    sprintf(str,"[%lu]Consumer-%d has left.\n", get_time_microseconds(),*((int *)arg));
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }
    return NULL;
}

/* Sigint handler for wholesaler */
void sigint_handler(int signum){
    char str[200];
    sprintf(str,"(pid %d) SIGINT received\n", getpid());
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }
    sigint_flag = 1;
}

/* gets time according to seconds */
unsigned long get_time_seconds(){
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec;
}

/* gets time according to microseconds */
unsigned long get_time_microseconds(){
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return 1000000 * tv.tv_sec + tv.tv_usec;;
}

/* This function prints error via perror and exit. */
void errExit(char *s){
    perror(s);
    exit(EXIT_FAILURE);
}