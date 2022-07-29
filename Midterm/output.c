/*Ahmet Denizli 161044020 */
/*     CSE344 Midterm     */
/*       16/04/2022       */
#include <string.h>
#include <sys/time.h>
#include "output.h"

/* gets time according to microseconds */
unsigned long get_time_seconds(){
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec;;
}

unsigned long get_time_microseconds(){
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return 1000000 * tv.tv_sec + tv.tv_usec;;
}