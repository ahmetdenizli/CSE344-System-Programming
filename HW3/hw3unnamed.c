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
/* initialization funtion for semaphores */
void init_semaphores();
/* destroy funtion for semaphores */
void dest_semaphores();

struct shmbuf {
    char ingredients[2];             /* The ingredients char array*/
    int isMilk;
    int isFlour;
    int isWalnuts;
    int isSugar;
    sem_t  wholesalerSem;                        /* POSIX unnamed semaphore */
    sem_t  milk;                                 /* POSIX unnamed semaphore */
    sem_t  flour;                                /* POSIX unnamed semaphore */
    sem_t  walnuts;                              /* POSIX unnamed semaphore */
    sem_t  sugar;                                /* POSIX unnamed semaphore */
    sem_t  mut;                                  /* POSIX unnamed semaphore */
    sem_t  MF;                                   /* POSIX unnamed semaphore */
    sem_t  MS;                                   /* POSIX unnamed semaphore */
    sem_t  MW;                                   /* POSIX unnamed semaphore */
    sem_t  SW;                                   /* POSIX unnamed semaphore */
    sem_t  SF;                                   /* POSIX unnamed semaphore */
    sem_t  FW;                                   /* POSIX unnamed semaphore */
};
struct shmbuf *shmp;

int  sigint_flag = 0;
char str[300];

int main(int argc, char *argv[]) {
    int fd, opt, i = 0;
    int chefPidArr[chefNum], pusherPidArr[pusherNum];
    char inputFilePath[20];
    char c, chefs_lacks[chefNum][3];

    while ((opt = getopt(argc, argv, ":i:")) != -1) {
        switch (opt) {
            case 'i':
                strcpy(inputFilePath, optarg);
                break;
            case ':':
                errno = EINVAL;
                errExit("You must write like this: \"./hw3unnamed -i inputFilePath\"");
                break;
            case '?':
                errno = EINVAL;
                errExit("You must write like this: \"./hw3unnamed -i inputFilePath\"");
                break;
            default:
                abort();
        }
    }

    if (optind != 3) {
        errno = EINVAL;
        errExit("You must write like this: \"./hw3unnamed -i inputFilePath\"");
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
    init_semaphores();

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

            int totalNumDess = 0;
            sem_t *chefSem;
            switch (i) {
                case 0:
                {
                    chefSem = &shmp->MF;
                    break;
                }
                case 1:
                {
                    chefSem = &shmp->MS;
                    break;
                }
                case 2:
                {
                    chefSem = &shmp->MW;
                    break;
                }
                case 3:
                {
                    chefSem = &shmp->SW;
                    break;
                }
                case 4:
                {
                    chefSem = &shmp->SF;
                    break;
                }
                case 5:
                {
                    chefSem = &shmp->FW;
                    break;
                }
                default:
                    break;
            }
            int chefPid = getpid();

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

                sem_post(&shmp->wholesalerSem);
                totalNumDess++;
            }
            sprintf(str,"chef%d (pid %d) is exiting\n",i,chefPid);
            if(write(STDOUT_FILENO,str,strlen(str))<0){
                errExit("write function error");
            }
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

    int wholesalerPid = getpid();
    ssize_t n;
    int k = 0;
    while ((n = read(fd, &c, 1)) > 0) {
        if(c == '\n')
            continue;

        if(c == 'M'){
            shmp->ingredients[k] = c;
            sem_post(&shmp->milk);
        }
        else if(c == 'F'){
            shmp->ingredients[k] = c;
            sem_post(&shmp->flour);
        }
        else if(c == 'W'){
            shmp->ingredients[k] = c;
            sem_post(&shmp->walnuts);
        }
        else if(c == 'S'){
            shmp->ingredients[k] = c;
            sem_post(&shmp->sugar);
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
            if (sem_wait(&shmp->wholesalerSem) == -1 && sigint_flag)
                break;
            sprintf(str,"the wholesaler (pid %d) has obtained the dessert and left\n",wholesalerPid);
            if(write(STDOUT_FILENO,str,strlen(str))<0){
                errExit("write function error");
            }
            fflush(stdout);
        }
    }
    close(fd);

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

    dest_semaphores();
    munmap(shmp,sizeof (*shmp));
    exit(EXIT_SUCCESS);
}

void pusherFun(int i){
    switch (i) {
        case 0:
        {
            while (sigint_flag==0){
                if (sem_wait(&shmp->milk) == -1 && sigint_flag)
                    break;
                sem_wait(&shmp->mut);
                if(shmp->isFlour == 1){
                    shmp->isFlour = 0;
                    sem_post(&shmp->SW);
                }
                else if(shmp->isSugar == 1){
                    shmp->isSugar = 0;
                    sem_post(&shmp->FW);
                }
                else if(shmp->isWalnuts == 1){
                    shmp->isWalnuts = 0;
                    sem_post(&shmp->SF);
                }
                else
                {
                    shmp->isMilk = 1;
                }
                sem_post(&shmp->mut);
            }
            break;
        }
        case 1:
        {
            while (sigint_flag==0){
                if (sem_wait(&shmp->flour) == -1 && sigint_flag)
                    break;
                sem_wait(&shmp->mut);
                if(shmp->isMilk == 1){
                    shmp->isMilk = 0;
                    sem_post(&shmp->SW);
                }
                else if(shmp->isSugar == 1){
                    shmp->isSugar = 0;
                    sem_post(&shmp->MW);
                }
                else if(shmp->isWalnuts == 1){
                    shmp->isWalnuts = 0;
                    sem_post(&shmp->MS);
                }
                else
                {
                    shmp->isFlour = 1;
                }
                sem_post(&shmp->mut);
            }
            break;
        }
        case 2:
        {
            while (sigint_flag==0){
                if (sem_wait(&shmp->walnuts) == -1 && sigint_flag)
                    break;
                sem_wait(&shmp->mut);
                if(shmp->isFlour == 1){
                    shmp->isFlour = 0;
                    sem_post(&shmp->MS);
                }
                else if(shmp->isSugar == 1){
                    shmp->isSugar = 0;
                    sem_post(&shmp->MF);
                }
                else if(shmp->isMilk == 1){
                    shmp->isMilk = 0;
                    sem_post(&shmp->SF);
                }
                else
                {
                    shmp->isWalnuts = 1;
                }
                sem_post(&shmp->mut);
            }
            break;
        }
        case 3:
        {
            while (sigint_flag==0){
                if (sem_wait(&shmp->sugar) == -1 && sigint_flag)
                    break;
                sem_wait(&shmp->mut);
                if(shmp->isFlour == 1){
                    shmp->isFlour = 0;
                    sem_post(&shmp->MW);
                }
                else if(shmp->isMilk == 1){
                    shmp->isMilk = 0;
                    sem_post(&shmp->FW);
                }
                else if(shmp->isWalnuts == 1){
                    shmp->isWalnuts = 0;
                    sem_post(&shmp->MF);
                }
                else
                {
                    shmp->isSugar = 1;
                }
                sem_post(&shmp->mut);
            }
            break;
        }
        default:
            break;
    }
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

/* initialization funtion for semaphores */
void init_semaphores(){
    if (sem_init(&shmp->wholesalerSem, 1, 0) == -1){
        errExit("sem_init");
    }
    if (sem_init(&shmp->milk, 1, 0) == -1){
        errExit("sem_init");
    }
    if (sem_init(&shmp->flour, 1, 0) == -1){
        errExit("sem_init");
    }
    if (sem_init(&shmp->walnuts, 1, 0) == -1){
        errExit("sem_init");
    }
    if (sem_init(&shmp->sugar, 1, 0) == -1){
        errExit("sem_init");
    }
    if (sem_init(&shmp->mut, 1, 1) == -1){
        errExit("sem_init");
    }
    if (sem_init(&shmp->MF, 1, 0) == -1){
        errExit("sem_init");
    }
    if (sem_init(&shmp->MS, 1, 0) == -1){
        errExit("sem_init");
    }
    if (sem_init(&shmp->MW, 1, 0) == -1){
        errExit("sem_init");
    }
    if (sem_init(&shmp->SW, 1, 0) == -1){
        errExit("sem_init");
    }
    if (sem_init(&shmp->SF, 1, 0) == -1){
        errExit("sem_init");
    }
    if (sem_init(&shmp->FW, 1, 0) == -1){
        errExit("sem_init");
    }
}

/* destroy funtion for semaphores */
void dest_semaphores(){
    sem_destroy(&shmp->wholesalerSem);
    sem_destroy(&shmp->milk);
    sem_destroy(&shmp->flour);
    sem_destroy(&shmp->walnuts);
    sem_destroy(&shmp->sugar);
    sem_destroy(&shmp->mut);
    sem_destroy(&shmp->MF);
    sem_destroy(&shmp->MS);
    sem_destroy(&shmp->MW);
    sem_destroy(&shmp->SW);
    sem_destroy(&shmp->SF);
    sem_destroy(&shmp->FW);
}