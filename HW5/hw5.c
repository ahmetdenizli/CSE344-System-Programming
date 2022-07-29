/* Ahmet Denizli 161044020 */
/*       CSE344 HW5        */
/*       23/05/2022        */
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
#include <pthread.h>
#include <math.h>

/* Function for threads */
void *threadFun();
/* This function returns current timestamp in seconds */
unsigned long get_time_seconds();
/* This function returns current timestamp in microseconds */
unsigned long get_time_microseconds();
/* This function prints given error via perror then exits. */
void errExit(char *s);
/* Sigint handler for wholesaler */
void sigint_handler(int signum);

int  sigint_flag = 0, n_arg, m_arg, powerOf2_n, arrived_1stPart=0;
int *mat1_ptr, *mat2_ptr, *matMult_ptr;
double *DFT_RE_ptr, *DFT_IM_ptr;

struct ThreadParams {
    int threadNum;
    int firstCol;
    int lastCol;
};
#define PI 3.14159265359

pthread_mutex_t mutex;
pthread_cond_t condition;

int main(int argc, char *argv[]) {
    int opt, n, i, j, fd, fd2, out_fd;
    char filePath1[100], filePath2[100], outFilePath[100];
    char c, str[300];
    unsigned long startTime, finishTime;

    while ((opt = getopt(argc, argv, ":i:j:o:n:m:")) != -1) {
        switch (opt) {
            case 'i':
                strcpy(filePath1, optarg);
                break;
            case 'j':
                strcpy(filePath2, optarg);
                break;
            case 'o':
                strcpy(outFilePath, optarg);
                break;
            case 'n':
                n_arg=atoi(optarg);
                if(n_arg<3){
                    errno=EINVAL;
                    errExit("n must be greater than 2.");
                }
                break;
            case 'm':
                m_arg=atoi(optarg);
                if(m_arg<2 || m_arg%2 != 0){
                    errno=EINVAL;
                    errExit("m must be, m >= 2k, k>=1.");
                }
                break;
            case ':':
                errno = EINVAL;
                errExit("You must write like this: \"./hw5 -i filePath1 -j filePath2 -o output -n 4 -m 2\"");
                break;
            case '?':
                errno = EINVAL;
                errExit("You must write like this: \"./hw5 -i filePath1 -j filePath2 -o output -n 4 -m 2\"");
                break;
            default:
                abort();
        }
    }
    if (optind != 11) {
        errno = EINVAL;
        errExit("You must write like this: \"./hw5 -i filePath1 -j filePath2 -o output -n 4 -m 2\"");
    }
    powerOf2_n = (int)pow(2.0, (double)n_arg);
    if(m_arg > powerOf2_n){
        errno = EINVAL;
        errExit("hw5.c main - m argument can not bigger than 2^n");
    }

    fd = open(filePath1, O_RDWR | O_SYNC);
    if (fd < 0)
    {
        errExit("hw5.c main - Cannot open filePath of argument i");
    }
    fd2 = open(filePath2, O_RDWR | O_SYNC);
    if (fd2 < 0)
    {
        errExit("hw5.c main - Cannot open filePath of argument j");
    }
    /* Opening output file */
    out_fd = open(outFilePath,O_WRONLY | O_CREAT | O_TRUNC,0777);
    if( out_fd < 0){
        errExit("hw5.c main - Cannot open output file");
    }

    if(pthread_mutex_init(&mutex,NULL)!=0){
        errExit("Mutex initialize error");
    }
    if(pthread_cond_init(&condition,NULL)!=0){
        errExit("Condition initialize error");
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

    mat1_ptr = (int *)malloc((powerOf2_n * powerOf2_n) * sizeof(int));
    mat2_ptr = (int *)malloc((powerOf2_n * powerOf2_n) * sizeof(int));
    matMult_ptr = (int *)malloc((powerOf2_n * powerOf2_n) * sizeof(int));
    DFT_RE_ptr = (double *)malloc((powerOf2_n * powerOf2_n) * sizeof(double));
    DFT_IM_ptr = (double *)malloc((powerOf2_n * powerOf2_n) * sizeof(double));

    i=0, j=0;
    while ((n = read(fd, &c, 1)) > 0) {
        if(j >= powerOf2_n){
            j = 0;
            i++;
        }
        if(i >= powerOf2_n){
            break;
        }

        *(mat1_ptr + i*powerOf2_n + j) = (int)c;
        j++;
    }
    if(i < powerOf2_n){
        close(fd);
        close(fd2);
        errno = EINVAL;
        errExit("hw5.c main - Fatal error file1 has insufficient content");
    }

    i=0, j=0;
    while ((n = read(fd2, &c, 1)) > 0) {
        if(j >= powerOf2_n){
            j = 0;
            i++;
        }
        if(i >= powerOf2_n){
            break;
        }

        *(mat2_ptr + i*powerOf2_n + j)  = (int)c;
        j++;
    }
    if(i < powerOf2_n){
        close(fd);
        close(fd2);
        errno = EINVAL;
        errExit("hw5.c main - Fatal error file2 has insufficient content");
    }
    startTime = get_time_microseconds();

    sprintf(str,"[%lu]Two matrices of size %dx%d have been read. The number of threads is %d\n", startTime, powerOf2_n, powerOf2_n, m_arg);
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }

    int readColNum = powerOf2_n/m_arg;
    int firstCol = 0;
    /* Create threads */
    pthread_t tid_consumer[m_arg];
    struct ThreadParams params[m_arg];
    for (i = 0; i < m_arg; i++) {
        params[i].threadNum = i;
        params[i].firstCol = firstCol;
        firstCol += readColNum;
        if(i == m_arg-1)
            params[i].lastCol = powerOf2_n;
        else
            params[i].lastCol = firstCol;
        if (pthread_create (&tid_consumer[i], NULL, threadFun, (void *) &params[i]) != 0) {
            errExit ("pthread_create error");
        }
    }

    /* join for consumer to terminate */
    for (i = 0; i < m_arg; i++){
        if (pthread_join (tid_consumer[i], NULL) != 0) {
            errExit("pthread_join error");
        }
    }
    if(sigint_flag==1){
        /* Closes the used files */
        if(close(fd)<0 || close(fd2)<0 || close(out_fd)<0){
            errExit("close file error");
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
        /* free the allocated memory */
        free(mat1_ptr);
        free(mat2_ptr);
        free(matMult_ptr);
        free(DFT_RE_ptr);
        free(DFT_IM_ptr);
        return 0;
    }

    for(i=0;i<powerOf2_n;i++)
    {
        for(j=0;j<powerOf2_n;j++)
        {
            sprintf(str,"%lf + i(%lf),",*(DFT_RE_ptr + i*powerOf2_n + j),*(DFT_IM_ptr + i*powerOf2_n + j));
            if(write(out_fd,str,strlen(str))<0){
                errExit("write function error");
            }
        }
        sprintf(str,"\n");
        if(write(out_fd,str,strlen(str))<0){
            errExit("write function error");
        }
    }
    finishTime = get_time_microseconds();

    sprintf(str,"[%lu]The process has written the output file. The total time spent is %lf seconds\n", finishTime, (double)(finishTime-startTime)/1000000.0);
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }

    /* Closes the used files */
    if(close(fd)<0 || close(fd2)<0 || close(out_fd)<0){
        errExit("close file error");
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
    /* free the allocated memory */
    free(mat1_ptr);
    free(mat2_ptr);
    free(matMult_ptr);
    free(DFT_RE_ptr);
    free(DFT_IM_ptr);

    return 0;
}

/* Function for threads */
void *threadFun(void *arg){
    char str[300];
    int i, j, k, ii, jj;
    struct ThreadParams *params = arg;
    unsigned long startTime, finishTime;

    startTime = get_time_microseconds();
    for(i=params->firstCol; i < params->lastCol; i++)
    {
        for(j=0; j < powerOf2_n; j++)
        {
            *(matMult_ptr+j*powerOf2_n+i)=0;
            for(k=0; k < powerOf2_n; k++)
            {
                if(sigint_flag==1)
                    return NULL;
                *(matMult_ptr+j*powerOf2_n+i) += *(mat1_ptr+j*powerOf2_n+k) * (*(mat2_ptr+k*powerOf2_n+i));
            }
        }
    }
    finishTime = get_time_microseconds();

    sprintf(str,"[%lu]Thread %d has reached the rendezvous point in %lf seconds\n", finishTime, params->threadNum, (double)(finishTime-startTime)/1000000.0);
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }

    pthread_mutex_lock(&mutex);
    arrived_1stPart++;
    while (arrived_1stPart < m_arg){
        pthread_cond_wait(&condition, &mutex);
        if(sigint_flag==1){
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
    }
    pthread_cond_broadcast(&condition);
    pthread_mutex_unlock(&mutex);

    startTime = get_time_microseconds();
    sprintf(str,"[%lu]Thread %d is advancing to the second part\n", startTime, params->threadNum);
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }

    for(i=params->firstCol;i<params->lastCol;i++)
    {
        for(j=0;j<powerOf2_n;j++)
        {
            double ak=0;
            double bk=0;
            for(ii=0;ii<powerOf2_n;ii++)
            {
                for(jj=0;jj<powerOf2_n;jj++)
                {
                    if(sigint_flag==1)
                        return NULL;

                    double x=-2.0*PI*j*ii/(double)powerOf2_n;
                    double y=-2.0*PI*i*jj/(double)powerOf2_n;
                    ak+= *(matMult_ptr+ii*powerOf2_n+jj) *cos(x+y);
                    bk+= *(matMult_ptr+ii*powerOf2_n+jj) *1.0*sin(x+y);
                }
            }
            *(DFT_RE_ptr+j*powerOf2_n+i) = ak;
            *(DFT_IM_ptr+j*powerOf2_n+i) = bk;
        }
    }
    finishTime = get_time_microseconds();

    sprintf(str,"[%lu]Thread %d has has finished the second part in %lf seconds\n", finishTime, params->threadNum, (double)(finishTime-startTime)/1000000.0);
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }
    return NULL;
}

/* Sigint handler for wholesaler */
void sigint_handler(int signum){
    char str[200];
    sprintf(str,"(pid %d) SIGINT received. Free resources and exits\n", getpid());
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }
    sigint_flag = 1;
    pthread_cond_broadcast(&condition);
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