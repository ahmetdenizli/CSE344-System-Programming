/* Ahmet Denizli 161044020 */
/*       CSE344 HW3        */
/*       01/05/2022        */
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

#define chefNum 6
#define pusherNum 4

/* Function for pushers */
void pusherFun(int i);
/* This function prints given error via perror then exits. */
void errExit(char *s);
/* Sigint handler for wholesaler */
void sigint_handler(int signum);
/* Sigint handler for chefs and pushers */
void sigint_handler2(int signum);

struct shmbuf {
    char ingredients[2];             /* The ingredients char array*/
    int isMilk;
    int isFlour;
    int isWalnuts;
    int isSugar;
};
struct shmbuf *shmp;

int  sigint_flag = 0;
char str[300], semaphoreName[200];

int main(int argc, char *argv[]) {
    int fd, opt, i = 0;
    int chefPidArr[chefNum], pusherPidArr[pusherNum];
    char inputFilePath[20];
    char c, chefs_lacks[chefNum][3];

    while ((opt = getopt(argc, argv, ":i:n:")) != -1) {
        switch (opt) {
            case 'i':
                strcpy(inputFilePath, optarg);
                break;
            case 'n':
                strcpy(semaphoreName, optarg);
                break;
            case ':':
                errno = EINVAL;
                errExit("You must write like this: \"./hw3named -i inputFilePath -n name\"");
                break;
            case '?':
                errno = EINVAL;
                errExit("You must write like this: \"./hw3named -i inputFilePath -n name\"");
                break;
            default:
                abort();
        }
    }

    if (optind != 5) {
        errno = EINVAL;
        errExit("You must write like this: \"./hw3named -i inputFilePath -n name\"");
    }

    fd = open(inputFilePath, O_RDWR | O_SYNC);
    if (fd < 0)
    {
        errExit("hw3named.c - Open file error");
    }

    strcpy(chefs_lacks[0],"WS");
    strcpy(chefs_lacks[1],"FW");
    strcpy(chefs_lacks[2],"SF");
    strcpy(chefs_lacks[3],"MF");
    strcpy(chefs_lacks[4],"MW");
    strcpy(chefs_lacks[5],"SM");

    shmp = mmap(NULL, sizeof (*shmp), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    shmp->isMilk = 0;
    shmp->isFlour = 0;
    shmp->isWalnuts = 0;
    shmp->isSugar = 0;
    shmp->ingredients[0] = '-';
    shmp->ingredients[1] = '-';

    for(i=0; i<chefNum; i++) // loop will run 6 times (chefNum=6)
    {
        chefPidArr[i] = fork();
        if(chefPidArr[i] == 0)
        {
            struct sigaction act;
            /* Handler settings */
            sigemptyset(&act.sa_mask);
            act.sa_flags = 0;
            act.sa_handler = sigint_handler2;
            /* sigaction for SIGINT */
            if (sigaction(SIGINT, &act, NULL) != 0){
                errExit("sigaction function error");
            }
            if (sigaction(SIGTERM, &act, NULL) != 0){
                errExit("sigaction function error");
            }

            char chefSemName[220];
            int totalNumDess = 0;
            switch (i) {
                case 0:
                {
                    sprintf(chefSemName,"%s_MF", semaphoreName);
                    break;
                }
                case 1:
                {
                    sprintf(chefSemName,"%s_MS", semaphoreName);
                    break;
                }
                case 2:
                {
                    sprintf(chefSemName,"%s_MW", semaphoreName);
                    break;
                }
                case 3:
                {
                    sprintf(chefSemName,"%s_SW", semaphoreName);
                    break;
                }
                case 4:
                {
                    sprintf(chefSemName,"%s_SF", semaphoreName);
                    break;
                }
                case 5:
                {
                    sprintf(chefSemName,"%s_FW", semaphoreName);
                    break;
                }
                default:
                    break;
            }
            int chefPid = getpid();
            sem_t *chefSem = sem_open(chefSemName, O_CREAT, 0666, 0);
            sem_t *wholesalerSem = sem_open(semaphoreName, O_CREAT, 0666, 0);

            while (sigint_flag==0){
                sprintf(str,"chef%d (pid %d) is waiting for %c and %c (ingredients array [%c,%c])\n",i,chefPid,chefs_lacks[i][0],chefs_lacks[i][1],shmp->ingredients[0],shmp->ingredients[1]);
                if(write(STDOUT_FILENO,str,strlen(str))<0){
                    errExit("write function error");
                }
                fflush(stdout);

                if (sem_wait(chefSem) == -1 && sigint_flag)
                    break;

                sprintf(str,"chef%d (pid %d) has taken the %c (ingredients array [%c,%c])\n",i,chefPid,shmp->ingredients[0],shmp->ingredients[0],shmp->ingredients[1]);
                if(write(STDOUT_FILENO,str,strlen(str))<0){
                    errExit("write function error");
                }
                shmp->ingredients[0] = '-';
                sprintf(str,"chef%d (pid %d) has taken the %c (ingredients array [%c,%c])\n",i,chefPid,shmp->ingredients[1],shmp->ingredients[0],shmp->ingredients[1]);
                if(write(STDOUT_FILENO,str,strlen(str))<0){
                    errExit("write function error");
                }
                shmp->ingredients[1] = '-';
                sprintf(str,"chef%d (pid %d) is preparing the dessert (ingredients array [%c,%c])\n",i,chefPid,shmp->ingredients[0],shmp->ingredients[1]);
                if(write(STDOUT_FILENO,str,strlen(str))<0){
                    errExit("write function error");
                }
                sprintf(str,"chef%d (pid %d) has delivered the dessert (ingredients array [%c,%c])\n",i,chefPid,shmp->ingredients[0],shmp->ingredients[1]);
                if(write(STDOUT_FILENO,str,strlen(str))<0){
                    errExit("write function error");
                }
                fflush(stdout);

                sem_post(wholesalerSem);
                totalNumDess++;
            }
            sprintf(str,"chef%d (pid %d) is exiting\n",i,chefPid);
            if(write(STDOUT_FILENO,str,strlen(str))<0){
                errExit("write function error");
            }
            sem_close(chefSem);
            sem_close(wholesalerSem);
            sem_unlink(chefSemName);
            return totalNumDess;
        }
        else if(chefPidArr[i] < 0)
        {
            errExit("Fork Failed");
        }
        else
        {
            continue;
        }
    }

    for(i=0; i<pusherNum; i++) // loop will run 4 times (pusherNum=4)
    {
        pusherPidArr[i] = fork();
        if(pusherPidArr[i] == 0)
        {
            struct sigaction act;
            /* Handler settings */
            sigemptyset(&act.sa_mask);
            act.sa_flags = 0;
            act.sa_handler = sigint_handler2;
            /* sigaction for SIGINT */
            if (sigaction(SIGINT, &act, NULL) != 0){
                errExit("sigaction function error");
            }
            if (sigaction(SIGTERM, &act, NULL) != 0){
                errExit("sigaction function error");
            }

            pusherFun(i);
            return 0;
        }
        else if(pusherPidArr[i] < 0)
        {
            errExit("Fork Failed");
        }
        else
        {
            continue;
        }
    }

    // Wholesaler continue
    struct sigaction act;
    /* Handler settings */
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = sigint_handler;
    /* sigaction for SIGINT */
    if (sigaction(SIGINT, &act, NULL) != 0){
        errExit("sigaction function error");
    }
    if (sigaction(SIGTERM, &act, NULL) != 0){
        errExit("sigaction function error");
    }

    sem_t *wholesalerSem = sem_open(semaphoreName, O_CREAT, 0666, 0);
    sprintf(str,"%s_M", semaphoreName);
    sem_t *milk = sem_open(str, O_CREAT, 0666, 0);
    sprintf(str,"%s_F", semaphoreName);
    sem_t *flour = sem_open(str, O_CREAT, 0666, 0);
    sprintf(str,"%s_W", semaphoreName);
    sem_t *walnuts = sem_open(str, O_CREAT, 0666, 0);
    sprintf(str,"%s_S", semaphoreName);
    sem_t *sugar = sem_open(str, O_CREAT, 0666, 0);

    int wholesalerPid = getpid();
    ssize_t n;
    int k = 0;
    while ((n = read(fd, &c, 1)) > 0) {
        if(c == '\n')
            continue;

        if(c == 'M'){
            shmp->ingredients[k] = c;
            sem_post(milk);
        }
        else if(c == 'F'){
            shmp->ingredients[k] = c;
            sem_post(flour);
        }
        else if(c == 'W'){
            shmp->ingredients[k] = c;
            sem_post(walnuts);
        }
        else if(c == 'S'){
            shmp->ingredients[k] = c;
            sem_post(sugar);
        } else{
            continue;
        }

        k++;
        if (k == 2){
            sprintf(str,"the wholesaler (pid %d) delivers %c and %c\n",wholesalerPid,shmp->ingredients[0],shmp->ingredients[1]);
            if(write(STDOUT_FILENO,str,strlen(str))<0){
                errExit("write function error");
            }
            sprintf(str,"the wholesaler (pid %d) is waiting for the dessert\n",wholesalerPid);
            if(write(STDOUT_FILENO,str,strlen(str))<0){
                errExit("write function error");
            }
            fflush(stdout);
            k = 0;
            if (sem_wait(wholesalerSem) == -1 && sigint_flag)
                break;
            sprintf(str,"the wholesaler (pid %d) has obtained the dessert and left\n",wholesalerPid);
            if(write(STDOUT_FILENO,str,strlen(str))<0){
                errExit("write function error");
            }
            fflush(stdout);
        }
    }
    close(fd);
    sem_close(milk);
    sem_close(flour);
    sem_close(walnuts);
    sem_close(sugar);
    sem_close(wholesalerSem);

    int total = 0, wstatus, return_value;
    for (i = 0; i < pusherNum; ++i) {
        kill(pusherPidArr[i], SIGINT);
        waitpid(pusherPidArr[i], NULL, 0);
    }
    for (i = 0; i < chefNum; ++i) {
        kill(chefPidArr[i], SIGINT);
        waitpid(chefPidArr[i], &wstatus, 0);
        return_value = WEXITSTATUS(wstatus);
        total += return_value;
    }
    sprintf(str,"the wholesaler (pid %d) is done (total desserts: %d)\n",wholesalerPid, total);
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }
    fflush(stdout);

    sem_unlink(semaphoreName);
    sprintf(str,"%s_M", semaphoreName);
    sem_unlink(str);
    sprintf(str,"%s_F", semaphoreName);
    sem_unlink(str);
    sprintf(str,"%s_W", semaphoreName);
    sem_unlink(str);
    sprintf(str,"%s_S", semaphoreName);
    sem_unlink(str);
    munmap(shmp,sizeof (*shmp));
    exit(EXIT_SUCCESS);
}

void pusherFun(int i){
    sprintf(str,"%s_PusherM", semaphoreName);
    sem_t *mut = sem_open(str, O_CREAT, 0666, 1);
    switch (i) {
        case 0:
        {
            sprintf(str,"%s_M", semaphoreName);
            sem_t *milk = sem_open(str, O_CREAT, 0666, 0);
            while (sigint_flag==0){
                if (sem_wait(milk) == -1 && sigint_flag)
                    break;
                sem_wait(mut);
                if(shmp->isFlour == 1){
                    shmp->isFlour = 0;
                    sprintf(str,"%s_SW", semaphoreName);
                    sem_t *sem = sem_open(str, O_CREAT, 0666, 0);
                    sem_post(sem);
                    sem_close(sem);
                }
                else if(shmp->isSugar == 1){
                    shmp->isSugar = 0;
                    sprintf(str,"%s_FW", semaphoreName);
                    sem_t *sem = sem_open(str, O_CREAT, 0666, 0);
                    sem_post(sem);
                    sem_close(sem);
                }
                else if(shmp->isWalnuts == 1){
                    shmp->isWalnuts = 0;
                    sprintf(str,"%s_SF", semaphoreName);
                    sem_t *sem = sem_open(str, O_CREAT, 0666, 0);
                    sem_post(sem);
                    sem_close(sem);
                }
                else
                {
                    shmp->isMilk = 1;
                }
                sem_post(mut);
            }
            sem_close(milk);
            break;
        }
        case 1:
        {
            sprintf(str,"%s_F", semaphoreName);
            sem_t *flour = sem_open(str, O_CREAT, 0666, 0);
            while (sigint_flag==0){
                if (sem_wait(flour) == -1 && sigint_flag)
                    break;
                sem_wait(mut);
                if(shmp->isMilk == 1){
                    shmp->isMilk = 0;
                    sprintf(str,"%s_SW", semaphoreName);
                    sem_t *sem = sem_open(str, O_CREAT, 0666, 0);
                    sem_post(sem);
                    sem_close(sem);
                }
                else if(shmp->isSugar == 1){
                    shmp->isSugar = 0;
                    sprintf(str,"%s_MW", semaphoreName);
                    sem_t *sem = sem_open(str, O_CREAT, 0666, 0);
                    sem_post(sem);
                    sem_close(sem);
                }
                else if(shmp->isWalnuts == 1){
                    shmp->isWalnuts = 0;
                    sprintf(str,"%s_MS", semaphoreName);
                    sem_t *sem = sem_open(str, O_CREAT, 0666, 0);
                    sem_post(sem);
                    sem_close(sem);
                }
                else
                {
                    shmp->isFlour = 1;
                }
                sem_post(mut);
            }
            sem_close(flour);
            break;
        }
        case 2:
        {
            sprintf(str,"%s_W", semaphoreName);
            sem_t *walnuts = sem_open(str, O_CREAT, 0666, 0);
            while (sigint_flag==0){
                if (sem_wait(walnuts) == -1 && sigint_flag)
                    break;
                sem_wait(mut);
                if(shmp->isFlour == 1){
                    shmp->isFlour = 0;
                    sprintf(str,"%s_MS", semaphoreName);
                    sem_t *sem = sem_open(str, O_CREAT, 0666, 0);
                    sem_post(sem);
                    sem_close(sem);
                }
                else if(shmp->isSugar == 1){
                    shmp->isSugar = 0;
                    sprintf(str,"%s_MF", semaphoreName);
                    sem_t *sem = sem_open(str, O_CREAT, 0666, 0);
                    sem_post(sem);
                    sem_close(sem);
                }
                else if(shmp->isMilk == 1){
                    shmp->isMilk = 0;
                    sprintf(str,"%s_SF", semaphoreName);
                    sem_t *sem = sem_open(str, O_CREAT, 0666, 0);
                    sem_post(sem);
                    sem_close(sem);
                }
                else
                {
                    shmp->isWalnuts = 1;
                }
                sem_post(mut);
            }
            sem_close(walnuts);
            break;
        }
        case 3:
        {
            sprintf(str,"%s_S", semaphoreName);
            sem_t *sugar = sem_open(str, O_CREAT, 0666, 0);
            while (sigint_flag==0){
                if (sem_wait(sugar) == -1 && sigint_flag)
                    break;
                sem_wait(mut);
                if(shmp->isFlour == 1){
                    shmp->isFlour = 0;
                    sprintf(str,"%s_MW", semaphoreName);
                    sem_t *sem = sem_open(str, O_CREAT, 0666, 0);
                    sem_post(sem);
                    sem_close(sem);
                }
                else if(shmp->isMilk == 1){
                    shmp->isMilk = 0;
                    sprintf(str,"%s_FW", semaphoreName);
                    sem_t *sem = sem_open(str, O_CREAT, 0666, 0);
                    sem_post(sem);
                    sem_close(sem);
                }
                else if(shmp->isWalnuts == 1){
                    shmp->isWalnuts = 0;
                    sprintf(str,"%s_MF", semaphoreName);
                    sem_t *sem = sem_open(str, O_CREAT, 0666, 0);
                    sem_post(sem);
                    sem_close(sem);
                }
                else
                {
                    shmp->isSugar = 1;
                }
                sem_post(mut);
            }
            sem_close(sugar);
            break;
        }
        default:
            break;
    }
    sem_close(mut);
    sprintf(str,"%s_PusherM", semaphoreName);
    sem_unlink(str);
}

/* Sigint handler for wholesaler */
void sigint_handler(int signum){
    sprintf(str,"(pid %d) SIGINT received\n", getpid());
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }
    fflush(stdout);
    sigint_flag = 1;
}

/* Sigint handler for chefs and pushers */
void sigint_handler2(int signum){
    sigint_flag = 1;
}

/* This function prints error via perror and exit. */
void errExit(char *s){
    perror(s);
    exit(EXIT_FAILURE);
}