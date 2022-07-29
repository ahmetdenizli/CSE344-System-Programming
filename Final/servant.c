/* Ahmet Denizli 161044020 */
/*      CSE344 FINAL       */
/*       16/06/2022        */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include "LinkedList.h"
#include "utilities.h"
#include "queue.h"

/* Function for servant threads */
void *servantThreadFun(void *arg);
/* Controls given process id is the current process id, with cheking argv and given process cmdline */
int check_proc( const char* proc_id, int cmd, int argc, char *argv[]);
/* Function for sigint handler. Sets sigint_flag to 1 */
void sigint_handler(int signum);
/* Function for free allocated resources */
void freeResources();

/* variables for sigint and total handled requests number */
int sigint_flag = 0, total_handled_req = 0, subDirNum;
/* head pointer for cityNode struct linked list */
struct cityNode *cityNodeHead = NULL;
/* queue for containing connection fds */
struct Queue *connQueue= NULL;
/* mutex for queue operations, enqueue and dequeue */
pthread_mutex_t connQueueMtx;
/* dirent pointer for scandir method */
struct dirent **namelist;

int main(int argc, char *argv[]) {
    int opt, PORT, i, fd, cities[2], sfd, server_sfd;
    char c, str[750], directoryPath[256], ipAdrr[20], citiesArg[12], buf[257], pid[20];
    size_t n;
    struct sockaddr_in servaddr, listenaddr, cli;
    DIR *dir, *dirp;
    struct dirent *dirent_ptr, *dp;
    struct stat lstt;
    char subDir[515], subDirEnt[850], buffer[5][30], rev_date[9];

    while ((opt = getopt(argc, argv, ":d:c:r:p:")) != -1) {
        switch (opt) {
            case 'd':
                strcpy(directoryPath, optarg);
                break;
            case 'c':
                strcpy(citiesArg, optarg);
                break;
            case 'r':
                strcpy(ipAdrr, optarg);
                break;
            case 'p':
                for(i=0;i<(int)strlen(optarg);++i){
                    if(optarg[i]<48 || optarg[i]>57)
                        errExit("servant.c -p port argument must be non negative number");
                }
                PORT=atoi(optarg);
                break;
            case ':':
                errno = EINVAL;
                errExit("You must write like this: \"./servant -d directoryPath -c 10-19 -r IP -p PORT\"");
                break;
            case '?':
                errno = EINVAL;
                errExit("You must write like this: \"./servant -d directoryPath -c 10-19 -r IP -p PORT\"");
                break;
            default:
                break;
        }
    }
    if(optind!=9){
        errno=EINVAL;
        errExit("You must write like this: \"./servant -d directoryPath -c 10-19 -r IP -p PORT\"");
    }

    sscanf(citiesArg, "%d-%d", &cities[0], &cities[1]);
    if(cities[0] >= cities[1] || cities[0] < 1){
        errno=EINVAL;
        errExit("Argument c order is wrong. You must write like this: \"./servant -d directoryPath -c 10-19 -r IP -p PORT\"");
        exit(EXIT_FAILURE);
    }

    if ((dirp = opendir("/proc")) == NULL) {
        perror("couldn't open '/proc'");
        exit( EXIT_FAILURE );
    }

    int ret = 0;
    while( (dp = readdir(dirp)) != NULL){
        if( dp->d_type == DT_DIR && isdigit( dp->d_name[0] ) ){
            if( (ret = check_proc( dp->d_name, 1, argc, argv)) != 0 ){
                strcpy(pid, dp->d_name);
                break;
            }
        }
    }
    closedir(dirp);
    if (ret == 0)
        errExit("Can not get pid from proc");

    struct sigaction act;
    /* Handler settings */
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = sigint_handler;
    /* sigaction for SIGINT */
    if (sigaction(SIGINT, &act, NULL) != 0){
        errExit("sigaction function error");
    }

    if(pthread_mutex_init(&connQueueMtx,NULL)!=0){
        errExit("Mutex initialize error");
    }

    subDirNum = scandir(directoryPath, &namelist, NULL, alphasort);
    if (subDirNum == -1) {
        errExit("servant.c - scandir error");
    } else if(subDirNum-2 < cities[1]){
        errExit("Argument is wrong, there is not specified directories. You must write like this: \"./servant -d directoryPath -c 10-19 -r IP -p PORT\"");
    }

    for (i = 0; i < subDirNum; ++i)  {
        /* Controls current dirent_ptr is equal to '.' and '..' if it is, then continue next */
        if ( (strcmp(namelist[i]->d_name, ".") == 0) || (strcmp(namelist[i]->d_name, "..") == 0) )
            continue;

        if(i-1 < cities[0])
            continue;
        else if(i-1 > cities[1])
            break;

        sprintf(subDir, "%s/%s", directoryPath, namelist[i]->d_name);

        if (!(dir = opendir(subDir))){
            errExit("servant.c - opendir error");
        }
        struct cityNode *cityNodetemp = (struct cityNode*)calloc(1,sizeof(struct cityNode));
        strcpy(cityNodetemp->name, namelist[i]->d_name);

        struct dateNode *dateNodeHead = NULL;
        while ((dirent_ptr = readdir(dir)) != NULL) {
            sprintf(subDirEnt, "%s/%s", subDir, dirent_ptr->d_name);

            if (lstat(subDirEnt, &lstt) != 0){
                errExit("servant.c - lstat error");
            }

            if (S_ISREG(lstt.st_mode)) {
                fd = open(subDirEnt, O_RDONLY);
                if (fd < 0)
                    continue;

                int k, j;

                for (j = 0; j < 4; ++j)
                    rev_date[j] = dirent_ptr->d_name[6+j];

                for (k = 0; k < 2; ++k) {
                    rev_date[4+k] = dirent_ptr->d_name[3+k];
                    rev_date[6+k] = dirent_ptr->d_name[k];
                }
                rev_date[8] = '\0';
                k=0;
                j=0;
                struct transactionNode *head = NULL;
                while ((n = read(fd, &c, 1)) > 0) {
                    if(c == ' '){
                        buffer[j][k] = '\0';
                        j++;
                        k = 0;
                    }
                    else if (c == '\n' || c == '\r') {
                        if(k != 0)
                            buffer[j][k] = '\0';
                        j = 0;
                        k = 0;
                        if (buffer[0][0] != '\0'){
                            head = insertFirstTransactionLL(head, atoi(buffer[0]), buffer[1], buffer[2], atoi(buffer[3]), atoi(buffer[4]));
                        }
                    }
                    else if(j < 5 && k < 29){
                        buffer[j][k] = c;
                        k++;
                    }
                }
                close(fd);
                reverseTransactionLL(&head);
                dateNodeHead = sortedInsertDateLL(dateNodeHead, rev_date, dirent_ptr->d_name, head);
            }
        }
        closedir(dir);
        cityNodetemp->entries = dateNodeHead;
        cityNodetemp->next = cityNodeHead;
        cityNodeHead = cityNodetemp;
    }

    sprintf(str,"Servant %s: loaded dataset, cities %s-%s\n", pid, namelist[cities[0]+1]->d_name, namelist[cities[1]+1]->d_name);
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }

    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
        errExit("servant.c socket creation error");
    bzero(&listenaddr, sizeof(listenaddr));

    listenaddr.sin_family = AF_INET;
    listenaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    listenaddr.sin_port = 0;

    if ((bind(sfd, (struct sockaddr *)&listenaddr, sizeof(listenaddr))) != 0)
        errExit("servant.c socket bind failed...\n");

    if ((listen(sfd, 128)) != 0)
        errExit("servant.c socket listen failed...\n");

    struct sockaddr_in sin;
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(sfd, (struct sockaddr *)&sin, &len) == -1)
        errExit("getsockname");

    sprintf(str,"Servant %s: listening at port %d\n", pid, ntohs(sin.sin_port));
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }

    server_sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sfd == -1)
        errExit("servant.c socket creation error");
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ipAdrr);
    servaddr.sin_port = htons(PORT);

    if (connect(server_sfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        freeResources();
        errExit("servant.c connection with the server failed");
    }
    write(server_sfd, "1", 2);
    read(server_sfd, buf, sizeof(buf));
    bzero(buf, sizeof(buf));

    write(server_sfd, pid, strlen(pid));
    read(server_sfd, buf, sizeof(buf));
    bzero(buf, sizeof(buf));

    sprintf(buf,"%d", ntohs(sin.sin_port));
    write(server_sfd, buf, strlen(buf));
    read(server_sfd, buf, sizeof(buf));
    bzero(buf, sizeof(buf));

    sprintf(buf,"%s", namelist[cities[0]+1]->d_name);
    write(server_sfd, buf, strlen(buf));
    read(server_sfd, buf, sizeof(buf));
    bzero(buf, sizeof(buf));

    sprintf(buf,"%s", namelist[cities[1]+1]->d_name);
    write(server_sfd, buf, strlen(buf));
    read(server_sfd, buf, sizeof(buf));

    read(server_sfd, buf, sizeof(buf));
    close(server_sfd);

    len = sizeof(cli);
    int connfd;
    connQueue = create_queue(200);
    while(sigint_flag == 0){
        connfd = accept(sfd, (struct sockaddr*)&cli, &len);
        if (sigint_flag == 1)
            break;
        if (connfd == -1)
            continue;
        pthread_t tid;

        pthread_mutex_lock(&connQueueMtx);
        enqueue(connQueue, connfd);
        pthread_mutex_unlock(&connQueueMtx);

        if (pthread_create (&tid, NULL, servantThreadFun, NULL) != 0) {
            errExit ("pthread_create error");
        }
        if (pthread_detach(tid) != 0) {
            errExit ("pthread_detach error");
        }
    }

    sprintf(str,"Servant %s: termination message received, handled %d requests in total.\n", pid, total_handled_req);
    if(write(STDOUT_FILENO,str,strlen(str))<0){
        errExit("write function error");
    }

    freeResources();
    close(sfd);
    exit(EXIT_SUCCESS);
}

/* Function for servant threads */
void *servantThreadFun(void *arg){
    char str[300], buf[BUF_SIZE], rev_date[9], rev_date2[9], tr_req_type[50], tr_type[50], tr_city[50], tr_date1[50], tr_date2[50];
    int transaction_count = 0, j=0, k=0, client_fd, ssret;
    memset(str, 0, sizeof(str));
    memset(buf, 0, sizeof(buf));
    memset(rev_date, 0, sizeof(rev_date));
    memset(rev_date2, 0, sizeof(rev_date2));
    memset(tr_type, 0, sizeof(tr_type));

    pthread_mutex_lock(&connQueueMtx);
    client_fd = dequeue(connQueue);
    pthread_mutex_unlock(&connQueueMtx);

    read(client_fd, buf, sizeof(buf));

    ssret = sscanf(buf, "%s %s %s %s %s", tr_req_type, tr_type, tr_date1, tr_date2, tr_city);

    if (ssret > 3){
        for (j = 0; j < 4; ++j){
            rev_date[j] = tr_date1[6+j];
            rev_date2[j] = tr_date2[6+j];
        }

        for (k = 0; k < 2; ++k) {
            rev_date[4+k] = tr_date1[3+k];
            rev_date[6+k] = tr_date1[k];
            rev_date2[4+k] = tr_date2[3+k];
            rev_date2[6+k] = tr_date2[k];
        }
        rev_date[8] = '\0';
        rev_date2[8] = '\0';

        struct cityNode *current = cityNodeHead;
        while (current != NULL){
            if (ssret == 5){
                if (strcmp(current->name, tr_city) != 0){
                    current = current->next;
                    continue;
                }
            }
            struct dateNode *dateEntry = current->entries;
            while (dateEntry != NULL){
                if (strcmp(dateEntry->rev_date, rev_date2) > 0)
                    break;
                if (strcmp(dateEntry->rev_date, rev_date) >= 0){
                    struct transactionNode *transactionEntry = dateEntry->firstTransaction;
                    while (transactionEntry != NULL){
                        if (strcmp(transactionEntry->type, tr_type) == 0)
                            transaction_count++;
                        transactionEntry = transactionEntry->next;
                    }
                }
                dateEntry = dateEntry->next;
            }
            if (ssret == 5)
                break;
            current = current->next;
        }
    }
    total_handled_req++;
    sprintf(str,"%d", transaction_count);
    write(client_fd, str, strlen(str));

    return NULL;
}

/* Function for free allocated resources */
void freeResources(){
    int i;
    struct cityNode *current = cityNodeHead;
    while (current != NULL){
        struct dateNode *dateEntry = current->entries;
        while (dateEntry != NULL){
            struct transactionNode *transactionEntry = dateEntry->firstTransaction;
            while (transactionEntry != NULL){
                struct transactionNode *temp = transactionEntry->next;
                free(transactionEntry);
                transactionEntry = temp;
            }
            struct dateNode *Datetemp = dateEntry->next;
            free(dateEntry);
            dateEntry = Datetemp;
        }
        struct cityNode *citytemp = current->next;
        free(current);
        current = citytemp;
    }

    for (i = 0; i < subDirNum; ++i)
        free(namelist[i]);
    free(namelist);

    if (connQueue != NULL)
        free_queue(connQueue);

    if(pthread_mutex_destroy(&connQueueMtx) != 0){
        pthread_mutex_unlock(&connQueueMtx);
        if(pthread_mutex_destroy(&connQueueMtx) != 0){
            errExit("Mutex destroy error");
        }
    }
}

/* Sigint handler for servant */
void sigint_handler(int signum){
    sigint_flag = 1;
}

/* Controls given process id is the current process id, with cheking argv and given process cmdline */
int check_proc( const char* proc_id, int cmd, int argc, char *argv[]){
    int i;
    char sfile[255];
    char sbuffer[1024];

    if( cmd ){
        sprintf( sfile, "/proc/%s/cmdline", proc_id );
    } else {
        sprintf( sfile, "/proc/%s/comm", proc_id );
    }

    FILE* f = fopen( sfile, "r" );
    if( !f ){
        return 0;
    }

    if( !fgets( sbuffer, 1024, f ) ){
        fclose( f );
        return 0;
    }

    fclose( f );
    int l = strlen(sbuffer);
    if( l > 0 && sbuffer[l-1] == '\n' ) sbuffer[l-1] = 0;

    char *ptr = sbuffer;
    for (i = 0; i < argc; ++i) {
        if (strcmp(argv[i], ptr) == 0){
            while (*ptr != '\0') ptr++;
            ptr++;
            continue;
        } else{
            return 0;
        }
    }

    return 1;
}