/*
    Multiple Change Detector
    RTES 2015

    Nikos P. Pitsianis
    Orestis Floros-Malivitsis
    AUTh 2015
*/

/* TODO: different executables for each version: */
/* a) initial b) multi w/o ack c) multi w/ ack */

#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define MILLION 1000000
#define DEFAULT_EXECUTION_TIME 20      /* default was 20 */
#define DEFAULT_TIME_MULTIPLIER 100000 /* default was 100000 */
#define DEFAULT_REQUESTED_THREADS 4

#define UNUSED(x) ((void)x)

#define INT_BIT (sizeof(int) * CHAR_BIT)

typedef struct { unsigned int tid; } parm;

static volatile unsigned int *signalArray;
static unsigned int *oldValues;

#ifdef USE_ACKNOWLEDGEMENT
#define USE_ACK(x) x

#ifdef USE_CONDITION_VARIABLES
#define USE_CV(x) x
#else
#define USE_CV(x)
#endif
#else
#define USE_ACK(x)
#define USE_CV(x)
#endif

USE_ACK(static volatile unsigned int *acknowledged;)

static struct timeval *timeStamp;

static unsigned int N;
static int use_bitfields;
static int use_multis;
static unsigned int total_N;

static unsigned int time_multiplier = DEFAULT_TIME_MULTIPLIER;
static unsigned int execution_time = DEFAULT_EXECUTION_TIME;
static unsigned int requested_threads = DEFAULT_REQUESTED_THREADS;

/* when execution time is over changing_signals is set to 0
 * and signals' values stop changing before cancelling the detectors. */
static volatile int changing_signals = 1;

void *SensorSignalReader(void *arg);
void *ChangeDetector(void *arg);
void *MultiChangeDetector(void *arg);
void *BitfieldChangeDetector(void *arg);
unsigned int toggle_signal(unsigned int r);
unsigned int msb_changed(unsigned int current, unsigned int old);

USE_CV(static pthread_mutex_t *signal_mutex;)
USE_CV(static pthread_cond_t *signal_cv;)

int main(int argc, char **argv) {
    /* usage prompt and exit */
    if (argc < 2) {
        printf("Usage: %s N T E NTHR\n"
               "    where:\n"
               "        N: number of signals to monitor\n"
               "        T(optional): time multiplier. Signal interval between T"
               " and 10 * T usec\n"
               "        E(optional): execution duration\n"
               "        NTHR(optional): number of threads\n",
               argv[0]);
        return 1;
    }

    N = (unsigned int)strtoul(argv[1], NULL, 0);

    if (argc > 2) {
        time_multiplier = (unsigned int)strtoul(argv[2], NULL, 0);
    }
    if (argc > 3) {
        execution_time = (unsigned int)strtoul(argv[3], NULL, 0);
    }
    if (argc > 4) {
        requested_threads = (unsigned int)strtoul(argv[4], NULL, 0);
    }

    use_bitfields = (N / requested_threads) >= INT_BIT;
    use_multis = (N > requested_threads) && (!use_bitfields);

    void *(*target_function)(void *);
    unsigned int open_threads;

    if (use_bitfields) {
        target_function = BitfieldChangeDetector;
        open_threads = requested_threads;
        total_N = N / INT_BIT + (N % INT_BIT != 0);
    } else if (use_multis) {
        target_function = MultiChangeDetector;
        open_threads = requested_threads;
        total_N = N;
    } else {
        target_function = ChangeDetector;
        open_threads = N;
        total_N = N;
    }

    fprintf(stderr, "open threads: %d array elements: %u actual signals: %u\n", open_threads, total_N, N);
    fprintf(stderr, "use_bitfields: %d use_multis: %d\n", use_bitfields, use_multis);

    /* Allocate signal, time-stamp arrays and thread handles. */
    signalArray = calloc(total_N, sizeof(int));
    timeStamp = malloc(N * sizeof(struct timeval));
    oldValues = calloc(total_N, sizeof(int));

    USE_ACK(acknowledged = malloc(N * sizeof(int));)
    USE_ACK(for (unsigned int i = 0; i < N; i++) acknowledged[i] = 1;)

    parm *p = malloc(open_threads * sizeof(parm));
    pthread_t sigGen;
    pthread_t *sigDet = malloc(open_threads * sizeof(pthread_t));

    USE_CV(signal_mutex = malloc(N * sizeof(pthread_mutex_t));)
    USE_CV(signal_cv = malloc(N * sizeof(pthread_cond_t));)

    /* Initialize mutex and condition variable objects */
    USE_CV(for (unsigned int i = 0; i < N; i++) pthread_mutex_init(&signal_mutex[i], NULL);)
    USE_CV(for (unsigned int i = 0; i < N; i++) pthread_cond_init(&signal_cv[i], NULL);)

    for (unsigned int i = 0; i < open_threads; i++) {
        p[i].tid = i;
        pthread_create(&sigDet[i], NULL, target_function, (void *)&p[i]);
    }

    pthread_create(&sigGen, NULL, SensorSignalReader, NULL);

    /* Sleep execution_time seconds and then cancel all threads.
     * Solves some problems with stdout redirection when used instead of alarm().*/
    sleep(execution_time);
    changing_signals = 0;

    fprintf(stderr, "joining\n");
    pthread_join(sigGen, NULL);
    fprintf(stderr, "joined\n");

#ifndef USE_ACKNOWLEDGEMENT
    /* Allow the detectors to find the last changes. */
    usleep(500);
#endif

    for (unsigned int i = 0; i < open_threads; i++) {
        pthread_cancel(sigDet[i]);
    }

    fflush(stdout);
    _exit(0);
}

unsigned int toggle_signal(unsigned int r) {
    /* Toggles the value of signal r.
     * timeStamp[r] is updated before the signal actually changes it's value.
     * Otherwise, the detectors can detect the change before timeStamp is updated. */

    if (use_bitfields) {
        const unsigned int array_idx = r / INT_BIT;
        const unsigned int bit_idx = r % INT_BIT;
        const unsigned int return_value = !((signalArray[array_idx] >> bit_idx) & 1);

        gettimeofday(&timeStamp[r], NULL);
        signalArray[array_idx] ^= 1 << bit_idx;

        return return_value;
    }
        gettimeofday(&timeStamp[r], NULL);
        return signalArray[r] ^= 1;

}

void *SensorSignalReader(void *arg) {
    UNUSED(arg);
    srand(time(NULL));

    while (changing_signals) {
        // t in [1, 10]
        const unsigned int t = rand() % 10 + 1;
        if (time_multiplier) {
            usleep(t * time_multiplier);
        }

        const unsigned int r = rand() % N;

        USE_CV(pthread_mutex_lock(&signal_mutex[r]);)
        USE_ACK(while (!acknowledged[r]) { USE_CV(pthread_cond_wait(&signal_cv[r], &signal_mutex[r]);) })
        USE_ACK(acknowledged[r] = 0;)

        if (toggle_signal(r)) {
            printf("C %d %lu\n", r, (timeStamp[r].tv_sec) * MILLION + (timeStamp[r].tv_usec));
        }

        USE_CV(pthread_mutex_unlock(&signal_mutex[r]);)
    }

    pthread_exit(NULL);
}

void *ChangeDetector(void *arg) {
    const parm *p = (parm *)arg;
    const unsigned int target = p->tid;

    /* loop stops with pthread_cancel() call at main() */
    while (1) {
        /* use a temporary variable in order to load signalArray[target] once in
         * each loop */
        unsigned int t;
        /* active waiting until target value changes to 1 */
        while ((t = signalArray[target]) == oldValues[target]) {
        }

        USE_CV(pthread_mutex_lock(&signal_mutex[target]));

        oldValues[target] = t;
        if (t) {
            /* signal activated: 0->1 */
            struct timeval tv;
            gettimeofday(&tv, NULL);
            /* print current time in usecs since the Epoch. */
            printf("D %d %lu\n", target, (tv.tv_sec) * MILLION + (tv.tv_usec));
        }

        USE_ACK(acknowledged[target] = 1;)

        USE_CV(pthread_cond_signal(&signal_cv[target]);)
        USE_CV(pthread_mutex_unlock(&signal_mutex[target]);)
    }
}

void *MultiChangeDetector(void *arg) {
    const parm *p = (parm *)arg;
    const unsigned int tid = p->tid;
    const unsigned int start =
            tid * (N / requested_threads) + (tid < N % requested_threads ? tid : N % requested_threads);
    const unsigned int end = start + (N / requested_threads) + (tid < N % requested_threads);
    unsigned int target = start;

    while (1) {
        unsigned int t;
        while ((t = signalArray[target]) == oldValues[target]) {
            target = (target < end - 1) ? target + 1 : start;
        }

        USE_CV(pthread_mutex_lock(&signal_mutex[target]));

        oldValues[target] = t;
        if (t) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            printf("D %u %lu\n", target, (tv.tv_sec) * MILLION + (tv.tv_usec));
        }

        USE_ACK(acknowledged[target] = 1;)
        USE_CV(pthread_cond_signal(&signal_cv[target]);)
        USE_CV(pthread_mutex_unlock(&signal_mutex[target]);)
    }
}

#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
static const char LogTable256[256] = {-1,    0,     1,     1,     2,     2,     2,     2,     3,     3,     3,
                                      3,     3,     3,     3,     3,     LT(4), LT(5), LT(5), LT(6), LT(6), LT(6),
                                      LT(6), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)};

unsigned int msb_changed(unsigned int current, unsigned int old) {
    /* Use bit-wise XOR to find the different bits between signalArray[target] and
     * oldValues[target]. Return the most significant of them using log2.
     * Kinda faster than gcc's __builtin_clz() */
    /* diff is INT_BIT-bit word to find the log2 of */
    unsigned int diff = current ^ old;
    unsigned int t, tt; /* temporaries */

    if ((tt = diff >> 16)) {
        return (t = tt >> 8) ? 24 + LogTable256[t] : 16 + LogTable256[tt];
    }
        return (t = diff >> 8) ? 8 + LogTable256[t] : LogTable256[diff];

}

void *BitfieldChangeDetector(void *arg) {
    parm *p = (parm *)arg;
    const unsigned int tid = p->tid;
    const unsigned int start = tid * (total_N / requested_threads) +
                               (tid < total_N % requested_threads ? tid : total_N % requested_threads);
    const unsigned int end = start + (total_N / requested_threads) + (tid < total_N % requested_threads);
    unsigned int target = start;

    while (1) {
        unsigned int t;
        while ((t = signalArray[target]) == oldValues[target]) {
            target = (target < end - 1) ? target + 1 : start;
        }

        const unsigned int bit_idx = msb_changed(t, oldValues[target]);
        const unsigned int actual = bit_idx + INT_BIT * target;
        USE_CV(pthread_mutex_lock(&signal_mutex[actual]);)
        /* oldValues[target] = t; <-- this way we lose signal changes
         * when 2 or more signals change at the same time within a bitfield. */
        /* if multiple changes happen then msb_changed() each time will find
         * the change at the most significant bit
         * because ceil(log2(x)) is the MSB of x */
        oldValues[target] ^= 1 << bit_idx;

        if ((t >> bit_idx) & 1) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            printf("D %u %lu\n", actual, (tv.tv_sec) * MILLION + (tv.tv_usec));
        }

        USE_ACK(acknowledged[actual] = 1;)
        USE_CV(pthread_cond_signal(&signal_cv[actual]);)
        USE_CV(pthread_mutex_unlock(&signal_mutex[actual]);)
    }
}
