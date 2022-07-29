/*Ahmet Denizli 161044020 */
/*     CSE344 Midterm     */
/*       16/04/2022       */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
#include "output.h"

void sigint_handler(int signum);
void sigint_handler_ServerZ(int signum);
void sigint_handler_ServerY_workers(int signum);
void sigint_handler_ServerZ_workers(int signum);
/*This function calls determinantOfMatrix function and return 1 if matrix is invertible, return 0 if it is not invertible*/
int isInvertible(int n, int mat[n][n]);
/* Function to get cofactor of mat[p][q] in temp[][] n is current dimension of mat[][]*/
void getCofactor(int n, int mat[n][n], int temp[n-1][n-1], int p, int q);
/* Recursive function for finding determinant of matrix.*/
int determinantOfMatrix(int n, int mat[n][n]);
/* This function prints given error via perror then exits. */
void errExit(char *s);
/* This function writes given string to the fd file descriptor then exits. */
void errExitFd(int fd, char *s);
/* This function performs Server Z and its childs operations */
void serverZfun();

struct shmbuf {
    sem_t  sem1;                        /* POSIX unnamed semaphore */
    int runningWorkerProcess;             /* Number of running worker */
    int invertible;             /* Number of invertable matrix request */
    int not_invertible;             /* Number of not invertable matrix request  */
    int forwardedNum;             /* Number of forwarded request */
};
struct shmbuf *shmp;

struct shmbufServerZ {
    sem_t  sem1;                        /* POSIX unnamed semaphore */
    sem_t  sem2;                        /* POSIX unnamed semaphore */
    sem_t  sem3;                        /* POSIX unnamed semaphore */
    int runningWorkerProcess;             /* Number of running worker */
    int invertible;             /* Number of running worker */
    int not_invertible;             /* Number of running worker */
    char queue[50][BUFSIZE];
    int rowN[50];
    char pid[50][20];
    int front;
    int rear;
};
struct shmbufServerZ *shmpServerZ;

int log_fd;
int poolSize, poolSize2, sleepDuration;
char buf2[BUFSIZE], str[300], clientPid[20];
int serverZpipe[2], serverZ_pid, sigint_flag = 0;

/* Makes the current process to daemon */
static void becomeDaemon()
{
    switch (fork()) {                   /* Become background process */
        case -1: {
            unlink("running_ins");
            exit(EXIT_FAILURE);
        }
        case 0:  break;                     /* Child falls through... */
        default: _exit(EXIT_SUCCESS);       /* while parent terminates */
    }

    /* Become leader of new session */
    if (setsid() < 0){
        unlink("running_ins");
        exit(EXIT_FAILURE);
    }

    switch (fork()) {                   /* Ensure we are not session leader */
        case -1: {
            unlink("running_ins");
            exit(EXIT_FAILURE);
        }
        case 0:  break;
        default: _exit(EXIT_SUCCESS);
    }

    /* Unmasks */
    umask(0);

    /* Close core  */
    close(STDERR_FILENO);
    close(STDOUT_FILENO);
    close(STDIN_FILENO);
}

int main(int argc, char *argv[]) {
    int fd, fd2, opt, i, k, running_instance_fd;
    char pathToServerFifo[100], pathToLogFile[100];

    if(access("running_ins",F_OK)!=-1){
        errno=EAGAIN;
        errExit("ServerY can not run second server program!\n");
    }
    else{
        if((running_instance_fd = open("running_ins",O_CREAT,0777)) < 0){
            errExit("Open file error.\n");
        }
    }

    while((opt = getopt(argc, argv, ":s:o:p:r:t:")) != -1)
    {
        switch(opt)
        {
            case 's':
                strcpy(pathToServerFifo,optarg);
                break;
            case 'o':
                strcpy(pathToLogFile,optarg);
                break;
            case 'p':
                poolSize=atoi(optarg);
                if(poolSize<2){
                    errno=EINVAL;
                    errExit("p must be greater than 1.");
                }
                break;
            case 'r':
                poolSize2=atoi(optarg);
                if(poolSize2<2){
                    errno=EINVAL;
                    errExit("r must be greater than 1.");
                }
                break;
            case 't':
                sleepDuration=atoi(optarg);
                if(sleepDuration<1){
                    errno=EINVAL;
                    errExit("Sleep duration -t must be greater than 0.");
                }
                break;
            case ':':
                errno=EINVAL;
                errExit("You must write like this: \"./serverY -s pathToServerFifo -o pathToLogFile -p poolSize -r poolSize2 -t 2\"");
            case '?':
                errno=EINVAL;
                errExit("You must write like this: \"./serverY -s pathToServerFifo -o pathToLogFile -p poolSize -r poolSize2 -t 2\"");
            default:
                abort();
        }
    }

    if(optind!=11){
        errno=EINVAL;
        errExit("You must write like this: \"./serverY -s pathToServerFifo -o pathToLogFile -p poolSize -r poolSize2 -t 2\"");
    }

    if (poolSize < 2 || poolSize2 < 2)
        errExit("Pool size of the ServerY and pool size of the  ServerZ must be greater than 1.");

    /* Opening log file */
    log_fd = open(pathToLogFile,O_WRONLY | O_CREAT | O_TRUNC,0777);
    if( log_fd < 0){
        errExit("open file error");
        unlink("running_ins");
        exit(EXIT_FAILURE);
    }

    /* Creates the daemon */
    becomeDaemon();

    sprintf(str,"[%lu]Server Y (%s, p=%d, t=%d) started\n",get_time_seconds(),pathToLogFile,poolSize,sleepDuration);
    if(write(log_fd,str,strlen(str))<0){
        unlink("running_ins");
        exit(EXIT_FAILURE);
    }

    if(pipe(serverZpipe) == -1)
        errExitFd(log_fd, "ServerY pipe error");

    sprintf(str,"[%lu]Instantiated server Z\n",get_time_seconds());
    if(write(log_fd,str,strlen(str))<0){
        unlink("running_ins");
        exit(EXIT_FAILURE);
    }

    serverZ_pid = fork();
    int rowNum;
    if(serverZ_pid == 0)
    {
        struct sigaction act;
        /* Handler settings */
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        act.sa_handler = sigint_handler_ServerZ;
        /* sigaction for SIGINT */
        if (sigaction(SIGINT, &act, NULL) != 0){
            exit(EXIT_FAILURE);
        }
        if (sigaction(SIGTERM, &act, NULL) != 0){
            exit(EXIT_FAILURE);
        }

        sprintf(str,"[%lu]Z:Server Z (%s, t=%d, r=%d) started\n",get_time_seconds(),pathToLogFile,sleepDuration,poolSize2);
        if(write(log_fd,str,strlen(str))<0){
            exit(EXIT_FAILURE);
        }

        serverZfun();
        exit(EXIT_SUCCESS);
    }
    else if(serverZ_pid< 0)
    {
        errExitFd(log_fd,"ServerY-ServerZ Fork Failed");
    }
    else
    {
        shmp = mmap(NULL, sizeof (*shmp), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

        if (sem_init(&shmp->sem1, 1, 1) == -1){
            errExitFd(log_fd,"sem_init");
        }
        shmp->runningWorkerProcess = 0;
        shmp->invertible = 0;
        shmp->not_invertible = 0;
        shmp->forwardedNum = 0;

        int pfd[poolSize][2];
        int mainPipe[2];
        pid_t pidArr[poolSize];

        for(i=0;i<poolSize;++i){
            if(pipe(pfd[i]) == -1 || pipe(pfd[i]) == -1){
                errExitFd(log_fd,"ServerY pipe error");
            }
        }
        if(pipe(mainPipe) == -1){
            errExitFd(log_fd,"ServerY pipe error");
        }

        for(i=0; i<poolSize; i++) // loop will run n times (n=poolSize)
        {
            pidArr[i] = fork();
            if(pidArr[i] == 0)
            {
                struct sigaction act;
                /* Handler settings */
                sigemptyset(&act.sa_mask);
                act.sa_flags = 0;
                act.sa_handler = sigint_handler_ServerY_workers;
                /* sigaction for SIGINT */
                if (sigaction(SIGINT, &act, NULL) != 0){
                    exit(EXIT_FAILURE);
                }
                if (sigaction(SIGTERM, &act, NULL) != 0){
                    exit(EXIT_FAILURE);
                }

                if(close(pfd[i][1])==-1)
                    errExitFd(log_fd,"pipe close error");

                int workerPid = getpid();
                while (sigint_flag==0){
                    write(mainPipe[1], &i, sizeof(int));

                    read(pfd[i][0], clientPid, 20);
                    if(sigint_flag)
                        break;
                    read(pfd[i][0], &rowNum, sizeof(int));
                    read(pfd[i][0], buf2, BUFSIZE);
                    sleep(sleepDuration);
                    if(sigint_flag)
                        break;

                    int (*arr)[rowNum] = calloc(rowNum, sizeof *arr);
                    int j = -1, bytes;
                    char * ptr = &buf2[0];
                    for (k = 0; k < rowNum*rowNum; ++k) {
                        if(k%rowNum == 0)
                            j++;
                        sscanf(ptr, "%d%*c%n", &arr[j][k%rowNum], &bytes );
                        ptr = ptr + bytes;
                    }
                    int ret = isInvertible(rowNum,arr);
                    free(arr);

                    if(ret == 1)
                        sprintf(str,"[%lu]Worker PID#%d responding to client PID#%s: the matrix is invertible.\n",get_time_seconds(),workerPid,clientPid);
                    else
                        sprintf(str,"[%lu]Worker PID#%d responding to client PID#%s: the matrix IS NOT invertible.\n",get_time_seconds(),workerPid,clientPid);
                    if(write(log_fd,str,strlen(str))<0){
                        exit(EXIT_FAILURE);
                    }
                    sprintf(buf2,"/tmp/%s_X", clientPid);
                    fd2 = open(buf2, O_WRONLY);
                    write(fd2, &ret, sizeof(int));
                    close(fd2);
                    if (sem_wait(&shmp->sem1) == -1 && sigint_flag)
                        break;
                    (shmp->runningWorkerProcess)--;
                    if(ret == 1)
                        (shmp->invertible)++;
                    else
                        (shmp->not_invertible)++;
                    if (sem_post(&shmp->sem1) == -1)
                        errExitFd(log_fd,"sem_post");
                }
                close(log_fd);
                close(fd2);
                close(pfd[i][0]);
                close(mainPipe[0]);
                close(mainPipe[1]);
                exit(EXIT_SUCCESS);
            }
            else if(pidArr[i] < 0)
            {
                errExitFd(log_fd,"Fork Failed");
            }
            else
            {
                if(close(pfd[i][0])==-1){
                    errExitFd(log_fd,"pipe close error");
                }
            }
        }

        struct sigaction act;
        /* Handler settings */
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        act.sa_handler = sigint_handler;
        /* sigaction for SIGINT */
        if (sigaction(SIGINT, &act, NULL) != 0){
            errExitFd(log_fd,"serverY.c - sigaction");
        }
        if (sigaction(SIGTERM, &act, NULL) != 0){
            errExitFd(log_fd,"serverY.c - sigaction");
        }

        if(mkfifo(pathToServerFifo,0666) == -1 && errno != EEXIST){
            errExitFd(log_fd,"serverY.c - Server fifo error");
        }

        fd = open(pathToServerFifo, O_RDWR);
        i = 0;
        while (sigint_flag==0){
            read(fd, clientPid, 20);
            if(sigint_flag)
                break;

            sprintf(buf2,"/tmp/%s_X", clientPid);
            fd2 = open(buf2, O_RDONLY);
            read(fd2, &rowNum, sizeof(int));
            read(fd2, buf2, BUFSIZE);
            close(fd2);

            if((shmp->runningWorkerProcess) >= poolSize){

                if (sem_wait(&shmp->sem1) == -1 && sigint_flag)
                    break;
                (shmp->forwardedNum)++;
                if (sem_post(&shmp->sem1) == -1)
                    errExitFd(log_fd,"sem_post");

                write(serverZpipe[1], clientPid, 20);
                write(serverZpipe[1], &rowNum, sizeof(int));
                write(serverZpipe[1], buf2, BUFSIZE);

                sprintf(str,"[%lu]Forwarding request of client PID#%s to serverZ, matrix size %dx%d, pool busy %d/%d\n",get_time_seconds(),clientPid,rowNum,rowNum,shmp->runningWorkerProcess,poolSize);
                if(write(log_fd,str,strlen(str))<0){
                    unlink("running_ins");
                    exit(EXIT_FAILURE);
                }
                continue;
            }

            read(mainPipe[0], &i, sizeof(int));
            if (sem_wait(&shmp->sem1) == -1 && sigint_flag)
                break;
            (shmp->runningWorkerProcess)++;
            if (sem_post(&shmp->sem1) == -1)
                errExitFd(log_fd,"sem_post");

            write(pfd[i][1], clientPid, 20);
            write(pfd[i][1], &rowNum, sizeof(int));
            write(pfd[i][1], buf2, BUFSIZE);

            sprintf(str,"[%lu]Worker PID#%d is handling client PID#%s, matrix size %dx%d, pool busy %d/%d\n",get_time_seconds(),pidArr[i],clientPid,rowNum,rowNum,shmp->runningWorkerProcess,poolSize);
            if(write(log_fd,str,strlen(str))<0){
                unlink("running_ins");
                exit(EXIT_FAILURE);
            }
        }
        // Send SIGINT to workers of Y
        for (i = 0; i < poolSize; ++i) {
            kill(pidArr[i], SIGINT);
            close(pfd[i][1]);
        }
        wait(NULL);
        // close and free resources
        close(mainPipe[0]);
        close(mainPipe[1]);
        close(log_fd);
        close(fd);
        close(fd2);
        sem_destroy(&shmp->sem1);
        munmap(shmp,sizeof (*shmp));
        unlink(pathToServerFifo);
        unlink("running_ins");
    }

    return 0;
}

/* This function performs Server Z and its childs operations */
void serverZfun(){
    int rowNum, i, k, fd2;

    shmpServerZ = mmap(NULL, sizeof (*shmpServerZ), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (sem_init(&shmpServerZ->sem1, 1, 1) == -1){
        errExitFd(log_fd,"sem_init");
    }
    if (sem_init(&shmpServerZ->sem2, 1, 0) == -1){
        errExitFd(log_fd,"sem_init");
    }
    if (sem_init(&shmpServerZ->sem3, 1, 50) == -1){
        errExitFd(log_fd,"sem_init");
    }
    shmpServerZ->runningWorkerProcess = 0;
    shmpServerZ->invertible = 0;
    shmpServerZ->not_invertible = 0;
    shmpServerZ->front = 0;
    shmpServerZ->rear = -1;

    pid_t pidArr[poolSize2];

    for (k= 0; k < poolSize2; ++k) {
        pidArr[k] = fork();
        if(pidArr[k] == 0)
        {
            struct sigaction act;
            /* Handler settings */
            sigemptyset(&act.sa_mask);
            act.sa_flags = 0;
            act.sa_handler = sigint_handler_ServerZ_workers;
            /* sigaction for SIGINT */
            if (sigaction(SIGINT, &act, NULL) != 0){
                exit(EXIT_FAILURE);
            }
            if (sigaction(SIGTERM, &act, NULL) != 0){
                exit(EXIT_FAILURE);
            }

            int workerPid = getpid();
            while(sigint_flag == 0){
                if (sem_wait(&shmpServerZ->sem2) == -1 && sigint_flag)
                    break;
                if (sem_wait(&shmpServerZ->sem1) == -1  && sigint_flag)
                    break;
                (shmpServerZ->runningWorkerProcess)++;
                rowNum = shmpServerZ->rowN[shmpServerZ->front];
                strcpy(clientPid, shmpServerZ->pid[shmpServerZ->front]);
                strcpy(buf2,shmpServerZ->queue[shmpServerZ->front]);
                shmpServerZ->front = (shmpServerZ->front + 1)%50;

                sprintf(str,"[%lu]Z:Worker PID#%d is handling client PID#%s, matrix size %dx%d, pool busy %d/%d\n",get_time_seconds(),workerPid,clientPid,rowNum,rowNum,shmpServerZ->runningWorkerProcess,poolSize2);
                if(write(log_fd,str,strlen(str))<0){
                    exit(EXIT_FAILURE);
                }
                if (sem_post(&shmpServerZ->sem1) == -1)
                    errExitFd(log_fd,"sem_post");
                if (sem_post(&shmpServerZ->sem3) == -1)
                    errExitFd(log_fd,"sem_post");

                sleep(sleepDuration);
                if(sigint_flag)
                    break;

                int (*arr)[rowNum] = calloc(rowNum, sizeof *arr);
                int j = -1, bytes;
                char * ptr = &buf2[0];
                for (k = 0; k < rowNum*rowNum; ++k) {
                    if(k%rowNum == 0)
                        j++;
                    sscanf(ptr, "%d%*c%n", &arr[j][k%rowNum], &bytes );
                    ptr = ptr + bytes;
                }
                int ret = isInvertible(rowNum, arr);
                free(arr);

                if(ret == 1)
                    sprintf(str,"[%lu]Z:Worker PID#%d responding to client PID#%s: the matrix is invertible.\n",get_time_seconds(),workerPid,clientPid);
                else
                    sprintf(str,"[%lu]Z:Worker PID#%d responding to client PID#%s: the matrix IS NOT invertible.\n",get_time_seconds(),workerPid,clientPid);
                if(write(log_fd,str,strlen(str))<0){
                    exit(EXIT_FAILURE);
                }

                sprintf(buf2,"/tmp/%s_X", clientPid);
                fd2 = open(buf2, O_WRONLY);
                write(fd2, &ret, sizeof(int));
                close(fd2);
                if (sem_wait(&shmpServerZ->sem1) == -1 && sigint_flag)
                    break;
                (shmpServerZ->runningWorkerProcess)--;
                if(ret == 1)
                    (shmpServerZ->invertible)++;
                else
                    (shmpServerZ->not_invertible)++;
                if (sem_post(&shmpServerZ->sem1) == -1)
                    errExitFd(log_fd,"sem_post");
            }
            close(log_fd);
            exit(EXIT_SUCCESS);
        }
        else if(pidArr[k] < 0)
        {
            errExitFd(log_fd,"Fork Failed");
        }
        else
        {
            //parents continue
            continue;
        }
    }
    while (sigint_flag == 0){
        if (sem_wait(&shmpServerZ->sem3) == -1 && sigint_flag)
            break;
        read(serverZpipe[0], clientPid, 20);
        if(sigint_flag)
            break;
        read(serverZpipe[0], &rowNum, sizeof(int));
        read(serverZpipe[0], buf2, BUFSIZE);

        if (sem_wait(&shmpServerZ->sem1) == -1 && sigint_flag)
            break;

        shmpServerZ->rear = (shmpServerZ->rear + 1) % 50;
        shmpServerZ->rowN[shmpServerZ->rear] = rowNum;
        strcpy(shmpServerZ->pid[shmpServerZ->rear], clientPid);
        strcpy(shmpServerZ->queue[shmpServerZ->rear], buf2);
        if (sem_post(&shmpServerZ->sem2) == -1)
            errExitFd(log_fd,"sem_post");
        if (sem_post(&shmpServerZ->sem1) == -1)
            errExitFd(log_fd,"sem_post");
    }
    // Send SIGINT to workers of Z
    for (i = 0; i < poolSize2; ++i) {
        kill(pidArr[i], SIGINT);
    }
    // close and free resources
    close(serverZpipe[0]);
    close(serverZpipe[1]);
    wait(NULL);
    close(log_fd);
    sem_destroy(&shmpServerZ->sem1);
    sem_destroy(&shmpServerZ->sem2);
    munmap(shmpServerZ,sizeof (*shmpServerZ));
    exit(EXIT_SUCCESS);
}

/* SIGINT handler function */
void sigint_handler(int signum){
    sprintf(str,"[%lu]SIGINT received, terminating Z and exiting server Y. Total requests handled: %d, %d invertible, %d not. %d requests were forwarded.\n",get_time_seconds(),shmp->invertible+shmp->not_invertible,shmp->invertible,shmp->not_invertible,shmp->forwardedNum);
    if(write(log_fd,str,strlen(str))<0){
        unlink("running_ins");
        exit(EXIT_FAILURE);
    }
    sigint_flag = 1;
    // Send SIGINT to serverZ
    kill(serverZ_pid, SIGINT);
}

/* SIGINT handler function */
void sigint_handler_ServerZ(int signum){
    sprintf(str,"[%lu]Z:SIGINT received, exiting server Z. Total requests handled: %d, %d invertible, %d not.\n",get_time_seconds(),shmpServerZ->invertible+shmpServerZ->not_invertible,shmpServerZ->invertible,shmpServerZ->not_invertible);
    if(write(log_fd,str,strlen(str))<0){
        unlink("running_ins");
        exit(EXIT_FAILURE);
    }
    sigint_flag = 1;
}

/* SIGINT handler function */
void sigint_handler_ServerY_workers(int signum){
    sigint_flag = 1;
}

/* SIGINT handler function */
void sigint_handler_ServerZ_workers(int signum){
    sigint_flag = 1;
}

/*This function calls determinantOfMatrix function and return 1 if matrix is invertible, return 0 if it is not invertible*/
int isInvertible(int n, int mat[n][n])
{
    if (determinantOfMatrix(n, mat) != 0)
        return 1;
    else
        return 0;
}

// Function to get cofactor of mat[p][q] in temp[][] n is current dimension of mat[][]
void getCofactor(int n, int mat[n][n], int temp[n-1][n-1], int p, int q)
{
    int i = 0, j = 0;

    // Looping for each element of the matrix
    for (int row = 0; row < n; row++) {
        if(sigint_flag)
            break;
        for (int col = 0; col < n; col++) {
            if(sigint_flag)
                break;

            if (row != p && col != q) {
                temp[i][j++] = mat[row][col];

                // Row is filled, so increase row index and
                // reset col index
                if (j == n - 1) {
                    j = 0;
                    i++;
                }
            }
        }
    }
}

/* Recursive function for finding determinant of matrix. */
int determinantOfMatrix(int n, int mat[n][n])
{
    int D = 0; // Initialize result

    // Base case : if matrix contains single element
    if (n == 1)
        return mat[0][0];

    int (*temp)[n-1] = calloc(n-1, sizeof *temp); // To store cofactors
    int sign = 1; // To store sign multiplier

    // Iterate for each element of first row
    for (int f = 0; f < n; f++) {
        // Getting Cofactor of mat[0][f]
        getCofactor(n, mat, temp, 0, f);
        if(sigint_flag){
            free(temp);
            return 0;
        }
        D += sign * mat[0][f] * determinantOfMatrix(n-1, temp);

        // terms are to be added with alternate sign
        sign = -sign;
    }
    free(temp);

    return D;
}

/* This function prints error via perror and exit. */
void errExit(char *s){
    unlink("running_ins");
    perror(s);
    exit(EXIT_FAILURE);
}

/* This function writes error to the fd file descriptor and exit. */
void errExitFd(int fd, char *s){
    unlink("running_ins");
    if(write(fd,s,strlen(s))<0){
        exit(EXIT_FAILURE);
    }
    exit(EXIT_FAILURE);
}
