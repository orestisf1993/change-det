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

#define EXECUTION_TIME 20
#define TIME_MULTIPLIER 100000
#define UNUSED(x) ((void) x)

void exitfunc(int sig)
{
    UNUSED(sig);
    _exit(0);
}

typedef struct {
    unsigned int tid;
} parm;

volatile int *signalArray;
volatile int *oldValues;
struct timeval *timeStamp;
int N;

#define NTHREADS 5
#define CHUNK (N / NTHREADS)

void *SensorSignalReader (void *args);
void *ChangeDetector (void *args);
void *MultiChangeDetector (void *arg);

int main(int argc, char **argv)
{
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
    alarm(EXECUTION_TIME); // after EXECUTION_TIME sec

    // Allocate signal, time-stamp arrays and thread handles
    signalArray = malloc(N * sizeof(int));
    oldValues = malloc(N * sizeof(int));
    timeStamp = malloc(N * sizeof(struct timeval));

    pthread_t sigGen;
    pthread_t *sigDet;

    for (int i = 0; i < N; i++) {
        signalArray[i] = 0;
        oldValues[i] = 0;
    }

    parm *p = malloc (N * sizeof(parm));
    void *(*target_function)(void *);

    int open_threads;
    if (N > NTHREADS) {
        target_function = MultiChangeDetector;
        open_threads = NTHREADS;
    } else {
        target_function = ChangeDetector;
        open_threads = N;
    }
    sigDet = malloc(open_threads * sizeof(pthread_t));

    for (int i = 0; i < open_threads; i++) {
        p[i].tid = i;
        pthread_create (&sigDet[i], NULL, target_function, (void *) &p[i]);
    }

    pthread_create (&sigGen, NULL, SensorSignalReader, NULL);

    // wait here until the signal - code never reaches this point
    for (int i = 0; i < N; i++)
        pthread_join (sigDet[i], NULL);

    return 0;
}


void *SensorSignalReader (void *arg)
{
    UNUSED(arg);

    char buffer[30];
    struct timeval tv;
    time_t curtime;

    srand(time(NULL));

    while (1) {
        // t in [1, 10]
        int t = rand() % 10 + 1;
        // wait 0.1 to 1 secs
        usleep(t * TIME_MULTIPLIER);

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
    parm *p = (parm *) arg;

    int target = p->tid;

    char buffer[30];
    struct timeval tv;
    time_t curtime;

    while (1) {

        while (signalArray[target] == 0) {}

        gettimeofday(&tv, NULL);
        curtime = tv.tv_sec;
        strftime(buffer, 30, "%d-%m-%Y  %T.", localtime(&curtime));
        printf("Detcted %5d at Time %s%ld after %ld.%06ld sec\n", target, buffer, tv.tv_usec,
               tv.tv_sec - timeStamp[target].tv_sec,
               tv.tv_usec - timeStamp[target].tv_usec);

        while (signalArray[target] == 1) {}

    }

}

void *MultiChangeDetector (void *arg)
{
    parm *p = (parm *) arg;
    printf("%d\n", p->tid);

    const unsigned int tid = p->tid;
    const unsigned int start = tid * CHUNK;
    const unsigned int end = start + CHUNK + (tid == NTHREADS - 1) * (N % NTHREADS);

    char buffer[30];
    struct timeval tv;
    time_t curtime;

    while (1) {
        unsigned int target = start;

        while (signalArray[target] <= oldValues[target]) {
            if (signalArray[target] != oldValues[target])
                oldValues[target] = 0;

            target ++;

            if (target == end) target = start;
        }

        gettimeofday(&tv, NULL);
        curtime = tv.tv_sec;
        strftime(buffer, 30, "%d-%m-%Y  %T.", localtime(&curtime));
        printf("Detcted %5d at Time %s%ld after %ld.%06ld sec by tid=%u\n", target, buffer, tv.tv_usec,
               tv.tv_sec - timeStamp[target].tv_sec,
               tv.tv_usec - timeStamp[target].tv_usec,
               tid);

        // possible race condition without the usleep() at SensorSignalReader().
        oldValues[target] = signalArray[target];

    }

}
