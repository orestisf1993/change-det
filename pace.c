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

#define EXECUTION_TIME 1
#define TIME_MULTIPLIER 2000
#define UNUSED(x) ((void) x)

//TODO: clean

typedef struct {
    unsigned int tid;
} parm;

volatile unsigned int *signalArray;
volatile unsigned int *oldValues;
struct timeval *timeStamp;

int N;
int use_bitfields;
int use_multis;
int total_N;

// when execution time is over changing_signals is set to 0
// and signals' values stop changing before canceling the detectors
int changing_signals = 1;

#define NTHREADS 5

void *SensorSignalReader (void *args);
void *BitfieldSensorSignalReader (void *arg);
void *ChangeDetector (void *args);
void *MultiChangeDetector (void *arg);
void *BitfieldChangeDetector (void *arg);

int main(int argc, char **argv)
{
    // usage prompt and exit
    if (argc != 2) {
        printf("Usage: %s N\n"
               " where\n"
               " N    : number of signals to monitor\n"
               , argv[0]);

        return 1;
    }

    N = atoi(argv[1]);
    use_bitfields = (N / NTHREADS) >= 32;
    use_multis = (N > NTHREADS) && (!use_bitfields);

    void *(*target_function)(void *);
    int open_threads;

    if (use_bitfields) {
        target_function = BitfieldChangeDetector;
        open_threads = NTHREADS;
        total_N = N / 32 + (N % 32 != 0);
    } else if (use_multis) {
        target_function = MultiChangeDetector;
        open_threads = NTHREADS;
        total_N = N;
    } else {
        target_function = ChangeDetector;
        open_threads = N;
        total_N = N;
    }

    fprintf(stderr, "open threads: %d array elements: %d\n", open_threads, total_N);
    fprintf(stderr, "use_bitfields: %d use_multis: %d\n", use_bitfields, use_multis);

    // Allocate signal, time-stamp arrays and thread handles
    signalArray = calloc(total_N, sizeof(int));
    oldValues = calloc(total_N, sizeof(int));
    timeStamp = malloc(N * sizeof(struct timeval));

    parm *p = malloc (open_threads * sizeof(parm));
    pthread_t sigGen;
    pthread_t *sigDet = malloc(open_threads * sizeof(pthread_t));

    for (int i = 0; i < open_threads; i++) {
        p[i].tid = i;
        pthread_create (&sigDet[i], NULL, target_function, (void *) &p[i]);
    }

    pthread_create (&sigGen, NULL, SensorSignalReader, NULL);
    // sleep EXECUTION_TIME seconds and then cancel all threads
    // solves some problems with stdout redirection
    sleep(EXECUTION_TIME);
    changing_signals = 0;
    usleep(5000);

    // wait here until the signal - code never reaches this point
    for (int i = 0; i < open_threads; i++)
        pthread_cancel (sigDet[i]);

    pthread_join (sigGen, NULL);

    return 0;
}

int toggle_signal(int r)
{
    if (use_bitfields) {
        const int array_idx = r / 32;
        const int bit_idx = r % 32;

        gettimeofday(&timeStamp[r], NULL);
        signalArray[array_idx] ^= 1 << bit_idx;

        // ~ printf("array_idx = %d, bit_idx = %d, value = %d\n", array_idx, bit_idx, (signalArray[array_idx] >> bit_idx) & 1);
        return (signalArray[array_idx] >> bit_idx) & 1;
    } else {
        gettimeofday(&timeStamp[r], NULL);
        signalArray[r] ^= 1;
        return signalArray[r];
    }
}

void *SensorSignalReader (void *arg)
{
    UNUSED(arg);

    // ~ char buffer[30];
    // ~ struct timeval tv;
    // ~ time_t curtime;

    srand(time(NULL));

    while (changing_signals) {
        // t in [1, 10]
        int t = rand() % 10 + 1;
        // wait 0.1 to 1 secs
        usleep(t * TIME_MULTIPLIER);

        int r = rand() % N;

        if (toggle_signal(r)) {

            // ~ timeStamp[r] = tv;
            // ~ curtime = tv.tv_sec;
            // ~ strftime(buffer, 30, "%d-%m-%Y  %T.", localtime(&curtime));
            // ~ printf("Changed %5d at Time %s%ld\n", r, buffer, tv.tv_usec);
            printf("C %d %lu\n", r, (timeStamp[r].tv_sec) * 1000000 + (timeStamp[r].tv_usec));
        }

        // ~ else printf("C %5d 1 -> 0\n", r);

    }

    pthread_exit(NULL);
}

void *ChangeDetector (void *arg)
{
    parm *p = (parm *) arg;

    int target = p->tid;

    // ~ char buffer[30];
    struct timeval tv;
    // ~ time_t curtime;

    while (1) {

        while (signalArray[target] == 0) {}

        gettimeofday(&tv, NULL);
        // ~ curtime = tv.tv_sec;
        // ~ strftime(buffer, 30, "%d-%m-%Y  %T.", localtime(&curtime));
        // ~ printf("Detcted %5d at Time %s%ld after %ld.%06ld sec\n", target, buffer, tv.tv_usec,
        // ~ tv.tv_sec - timeStamp[target].tv_sec,
        // ~ tv.tv_usec - timeStamp[target].tv_usec);
        printf("D %d %lu\n", target, (tv.tv_sec) * 1000000 + (tv.tv_usec));

        while (signalArray[target] == 1) {}
    }
}

void *MultiChangeDetector (void *arg)
{
    const parm *p = (parm *) arg;

    const unsigned int tid = p->tid;
    const unsigned int start = tid * (N / NTHREADS);
    const unsigned int end = start + (N / NTHREADS) + (tid == NTHREADS - 1) * (N % NTHREADS);

    while (1) {
        unsigned int target = start;

        while (signalArray[target] == oldValues[target]) {

            target ++;

            if (target == end) target = start;
        }

        if (signalArray[target] == 0) {
            oldValues[target] = 0;
            continue;
        }

        struct timeval tv;

        gettimeofday(&tv, NULL);

        printf("D %d %lu\n", target, (tv.tv_sec) * 1000000 + (tv.tv_usec));

        // possible race condition without the usleep() at SensorSignalReader().
        oldValues[target] = signalArray[target];
    }
}

#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
const char LogTable256[256] = {
    -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
    LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
};

int changed_bit(int target)
{
    // 32-bit word to find the log of
    unsigned int diff = signalArray[target] ^ oldValues[target];
    unsigned int t, tt; // temporaries

    if ((tt = diff >> 16))
        return (t = tt >> 8) ? 24 + LogTable256[t] : 16 + LogTable256[tt];
    else
        return (t = diff >> 8) ? 8 + LogTable256[t] : LogTable256[diff];
}

void *BitfieldChangeDetector (void *arg)
{
    parm *p = (parm *) arg;

    const unsigned int tid = p->tid;
    const unsigned int start = tid * (total_N / NTHREADS);
    const unsigned int end = start + (total_N / NTHREADS) + (tid == NTHREADS - 1) * (total_N % NTHREADS);

    // ~ char buffer[30];
    struct timeval tv;
    // ~ time_t curtime;
    unsigned int target = start;

    while (1) {
        while (signalArray[target] == oldValues[target]) {
            // ~ if (signalArray[target] != oldValues[target]) {
            // ~ // race condition??
            // ~ oldValues[target] = signalArray[target];
            // ~ }

            target ++;

            if (target == end) target = start;
        }

        const int bit_idx = changed_bit(target);

        // ~ printf("new=%u old=%u\n", signalArray[target], oldValues[target]);

        // bit = (number >> x) & 1;
        if (((signalArray[target] >> bit_idx) & 1) == 0) {
            // change due to deactivation

            // oldValues[target] = signalArray[target];
            oldValues[target] ^= 1 << bit_idx;
            continue;
        }

        const int actual = bit_idx + 32 * target;

        gettimeofday(&tv, NULL);
        // ~ curtime = tv.tv_sec;
        // ~ strftime(buffer, 30, "%d-%m-%Y  %T.", localtime(&curtime));
        // ~ printf("Detcted %d at Time %s%ld after %ld.%06ld sec by tid=%u\n", actual, buffer, tv.tv_usec,
        // ~ tv.tv_sec - timeStamp[actual].tv_sec,
        // ~ tv.tv_usec - timeStamp[actual].tv_usec,
        // ~ tid);
        printf("D %d %lu\n", actual,
               // ~ (tv.tv_sec - timeStamp[actual].tv_sec)*1000000 + (tv.tv_usec - timeStamp[actual].tv_usec),
               (tv.tv_sec) * 1000000 + (tv.tv_usec)
              );
        // ~ printf("%ld %ld || %ld %ld\n", tv.tv_sec, tv.tv_usec, timeStamp[actual].tv_sec, timeStamp[actual].tv_usec);

        // possible race condition without the usleep() at SensorSignalReader().
        oldValues[target] = signalArray[target];
    }
}
