
#include <stdio.h>
#include <stdlib.h>
#include "utilities.h"

/* This function prints error via perror and exit. */
void errExit(char *s){
    perror(s);
    exit(EXIT_FAILURE);
}