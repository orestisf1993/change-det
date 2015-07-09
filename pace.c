/*
  Multiple Change Detector
  RTES 2015

  Nikos P. Pitsianis
  AUTh 2015
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

#define ALARM_TIME 20

void exitfunc(int sig)
{
    _exit(0);
}

volatile int *signalArray;
struct timeval *timeStamp;
int N;

void *SensorSignalReader (void *args);
void *ChangeDetector (void *args);

int main(int argc, char **argv)
{
    int i;

    N = atoi(argv[1]);

    // usage prompt and exit
    if (argc != 2) {
        printf("Usage: %s N\n"
               " where\n"
               " N    : number of signals to monitor\n"
               , argv[0]);

        return 1;
    }

    // set a timed signal to terminate the program
    signal(SIGALRM, exitfunc);
    alarm(ALARM_TIME); // after ALARM_TIME sec

    // Allocate signal, time-stamp arrays and thread handles
    signalArray = malloc(N * sizeof(int));
    timeStamp = malloc(N * sizeof(struct timeval));

    pthread_t sigGen;
    pthread_t sigDet;

    for (i = 0; i < N; i++)
        signalArray[i] = 0;

    pthread_create (&sigDet, NULL, ChangeDetector, NULL);
    pthread_create (&sigGen, NULL, SensorSignalReader, NULL);

    // wait here until the signal - code never reaches this point
    pthread_join (sigDet, NULL);

    return 0;
}


void *SensorSignalReader (void *arg)
{

    char buffer[30];
    struct timeval tv;
    time_t curtime;

    srand(time(NULL));

    while (1) {
        int t = rand() % 10 + 1; // wait up to 1 sec in 10ths
        usleep(t * 100000);

        int r = rand() % N;
        signalArray[r] ^= 1;

        if (signalArray[r]) {
            gettimeofday(&tv, NULL);
            timeStamp[r] = tv;
            curtime = tv.tv_sec;
            strftime(buffer, 30, "%d-%m-%Y  %T.", localtime(&curtime));
            printf("Changed %5d at Time %s%ld\n", r, buffer, tv.tv_usec);
        }
    }
}

void *ChangeDetector (void *arg)
{
    char buffer[30];
    struct timeval tv;
    time_t curtime;

    while (1) {

        while (signalArray[0] == 0) {}

        gettimeofday(&tv, NULL);
        curtime = tv.tv_sec;
        strftime(buffer, 30, "%d-%m-%Y  %T.", localtime(&curtime));
        printf("Detcted %5d at Time %s%ld after %ld.%06ld sec\n", 0, buffer, tv.tv_usec,
               tv.tv_sec - timeStamp[0].tv_sec,
               tv.tv_usec - timeStamp[0].tv_usec);

        while (signalArray[0] == 1) {}

    }

}
