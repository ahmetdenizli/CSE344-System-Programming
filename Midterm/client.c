/*Ahmet Denizli 161044020 */
/*     CSE344 Midterm     */
/*       16/04/2022       */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include "output.h"

void sigint_handler(int signum);
int fd, fd2;
char str[300], clientFifo[30];

int main(int argc, char *argv[]) {
    int opt, i=0, n, ret;
    char pathToServerFifo[20];
    char pathToDataFile[20];
    char c, buf2[BUFSIZE], clientPid[20];

    while((opt = getopt(argc, argv, ":s:o:")) != -1)
    {
        switch(opt)
        {
            case 's':
                strcpy(pathToServerFifo,optarg);
                break;
            case 'o':
                strcpy(pathToDataFile,optarg);
                break;
            case ':':
                errno=EINVAL;
                perror("You must write like this: \"./client -s pathToServerFifo -o pathToDataFile\"");
                exit(EXIT_FAILURE);
                break;
            case '?':
                errno=EINVAL;
                perror("You must write like this: \"./client -s pathToServerFifo -o pathToDataFile\"");
                exit(EXIT_FAILURE);
                break;
            default:
                abort ();
        }
    }

    if(optind!=5){
        errno=EINVAL;
        perror("You must write like this: \"./client -s pathToServerFifo -o pathToDataFile\"");
        exit(EXIT_FAILURE);
    }

    struct sigaction act;
    /* Handler settings */
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = sigint_handler;
    /* sigaction for SIGINT */
    if (sigaction(SIGINT, &act, NULL) != 0){
        exit(EXIT_FAILURE);
    }

    fd = open(pathToDataFile, O_RDWR | O_SYNC);
    if (fd < 0)
    {
        perror("client.c - Open file error");
        exit(EXIT_FAILURE);
    }

    int rowNum = 1, temp = 0;
    while ((n = read(fd, &c, 1)) > 0) {
        if(c == ',' && temp == 0)
            rowNum++;
        if(c == '\n')
            temp = 1;
        if(i >= BUFSIZE){
            close(fd);
            errno=EINVAL;
            perror("matrix size over the buffer size.");
            exit(EXIT_FAILURE);
        }

        buf2[i] = c;
        i++;
    }
    buf2[i] = '\0';
    close(fd);

    if (rowNum < 2)
    {
        perror("matrix size must be greater than 1.");
        exit(EXIT_FAILURE);
    }

    sprintf(clientPid,"%d", getpid());
    sprintf(clientFifo,"/tmp/%s_X", clientPid);

    if(mkfifo(clientFifo,0666) == -1){
        perror("client.c - Open fifo error");
        exit(EXIT_FAILURE);
    }
    /*if(mkfifo(pathToServerFifo,0666) == -1 && errno != EEXIST){
        perror("client.c - Server fifo error");
        exit(EXIT_FAILURE);
    }*/

    sprintf(str,"[%lu]Client PID#%s (%s) is submitting a %dx%d matrix\n",get_time_seconds(),clientPid,pathToDataFile,rowNum,rowNum);
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        exit(EXIT_FAILURE);
    }
    unsigned long startTime = get_time_microseconds();

    fd = open(pathToServerFifo, O_WRONLY);
    write(fd, clientPid, 20);
    close(fd);

    fd = open(clientFifo, O_WRONLY);
    write(fd, &rowNum, sizeof(int));
    write(fd, buf2, BUFSIZE);
    close(fd);

    fd2 = open(clientFifo, O_RDONLY);
    read(fd2, &ret, sizeof(int));

    if(ret == 1){
        sprintf(str,"[%lu]Client PID#%s: the matrix is invertible, total time %.2lf seconds, goodbye.\n",get_time_seconds(),clientPid, (double)(get_time_microseconds()-startTime)/1000000.0);
        if(write(STDOUT_FILENO,str,strlen(str))<0){
            exit(EXIT_FAILURE);
        }
    } else{
        sprintf(str,"[%lu]Client PID#%s: the matrix IS NOT invertible, total time %.2lf seconds, goodbye.\n",get_time_seconds(),clientPid, (double)(get_time_microseconds()-startTime)/1000000.0);
        if(write(STDOUT_FILENO,str,strlen(str))<0){
            exit(EXIT_FAILURE);
        }
    }
    close(fd2);
    unlink(clientFifo);

    return 0;
}

/* SIGINT handler function */
void sigint_handler(int signum){
    sprintf(str,"[%lu]SIGINT received, exiting client.\n",get_time_seconds());
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        exit(EXIT_FAILURE);
    }
    close(fd);
    close(fd2);
    unlink(clientFifo);
    exit(EXIT_SUCCESS);
}
