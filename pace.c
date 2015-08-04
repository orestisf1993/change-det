/*
    Multiple Change Detector
    RTES 2015

    Nikos P. Pitsianis
    Orestis Floros-Malivitsis
    AUTh 2015
*/

//TODO: condition variables instead of acknowledgement
//TODO: condition variables in signalArray
//TODO: free() - clean ups?
//TODO: buffer instead of stdout prints may speed up things?
//TODO: different executables for each version: a) initial b) multi w/o ack c) multi w/ ack

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

#define EXECUTION_TIME 1
#define TIME_MULTIPLIER 0
#define UNUSED(x) ((void) x)

typedef struct {
    unsigned int tid;
} parm;

volatile unsigned int* signalArray;
volatile unsigned int* oldValues;

#ifdef USE_ACKNOWLEDGEMENT
    #define USE_ACK(x) x
#else
    #define USE_ACK(x)
#endif

USE_ACK(volatile unsigned int* acknowledged);

struct timeval* timeStamp;

unsigned int N;
int use_bitfields;
int use_multis;
unsigned int total_N;

/* when execution time is over changing_signals is set to 0
 * and signals' values stop changing before canceling the detectors. */
volatile int changing_signals = 1;

#define NTHREADS 5

void* SensorSignalReader(void* args);
void* ChangeDetector(void* args);
void* MultiChangeDetector(void* arg);
void* BitfieldChangeDetector(void* arg);

pthread_mutex_t* count_mutex;
pthread_cond_t* count_threshold_cv;

int main(int argc, char** argv) {
    // usage prompt and exit
    if (argc != 2) {
        printf("Usage: %s N\n"
               "    where:\n"
               "        N: number of signals to monitor\n"
               , argv[0]);
        return 1;
    }

    N = atoi(argv[1]);

    use_bitfields = (N / NTHREADS) >= 32;
    use_multis = (N > NTHREADS) && (!use_bitfields);

    void* (*target_function)(void*);
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

    fprintf(stderr, "open threads: %d array elements: %d actual signals: %d\n", open_threads,
            total_N, N);
    fprintf(stderr, "use_bitfields: %d use_multis: %d\n", use_bitfields, use_multis);

    // Allocate signal, time-stamp arrays and thread handles.
    signalArray = calloc(total_N, sizeof(int));
    timeStamp = malloc(N * sizeof(struct timeval));
    oldValues = calloc(total_N, sizeof(int));

    USE_ACK(acknowledged = malloc(N * sizeof(int)));
    USE_ACK(for (unsigned int i = 0; i < N; i++) acknowledged[i] = 1);

    parm* p = malloc(open_threads * sizeof(parm));
    pthread_t sigGen;
    pthread_t* sigDet = malloc(open_threads * sizeof(pthread_t));

    count_mutex = malloc(N *sizeof(pthread_mutex_t));
    count_threshold_cv = malloc(N * sizeof(pthread_cond_t));

    /* Initialize mutex and condition variable objects */
    for (unsigned int i = 0; i < N ; i++) pthread_mutex_init(&count_mutex[i], NULL);
    for (unsigned int i = 0; i < N ; i++) pthread_cond_init(&count_threshold_cv[i], NULL);

    for (int i = 0; i < open_threads; i++) {
        p[i].tid = i;
        pthread_create(&sigDet[i], NULL, target_function, (void*) &p[i]);
    }

    pthread_create(&sigGen, NULL, SensorSignalReader, NULL);

    /* Sleep EXECUTION_TIME seconds and then cancel all threads.
     * Solves some problems with stdout redirection when used instead of alarm().*/
    sleep(EXECUTION_TIME);
    changing_signals = 0;

    fprintf(stderr, "joining\n");
    pthread_join(sigGen, NULL);
    fprintf(stderr, "joined\n");

    #ifndef USE_ACKNOWLEDGEMENT
    // Allow the detectors to find the last changes.
    usleep(500);
    #endif

    for (int i = 0; i < open_threads; i++) {
        pthread_cancel(sigDet[i]);
    }

    return 0;
}

int toggle_signal(int r) {
    /* Toggles the value of signal r.
     * timeStamp[r] is updated before the signal actually changes it's value.
     * Otherwise, the detectors can detect the change with before timeStamp is updated. */

    if (use_bitfields) {
        const int array_idx = r / 32;
        const int bit_idx = r % 32;
        const int return_value = !((signalArray[array_idx] >> bit_idx) & 1);

        gettimeofday(&timeStamp[r], NULL);
        signalArray[array_idx] ^= 1 << bit_idx;

        return return_value;
    } else {
        gettimeofday(&timeStamp[r], NULL);
        return signalArray[r] ^= 1;
    }
}

void* SensorSignalReader(void* arg) {
    UNUSED(arg);
    srand(time(NULL));

    while (changing_signals) {
        // t in [1, 10]
        const int t = rand() % 10 + 1;
        usleep(t * TIME_MULTIPLIER);
        const int r = rand() % N;

        pthread_mutex_lock(&count_mutex[r]);
        USE_ACK(while (!acknowledged[r]) {pthread_cond_wait(&count_threshold_cv[r], &count_mutex[r]);});
        USE_ACK(acknowledged[r] = 0);

        if (toggle_signal(r)) {
            printf("C %d %lu\n", r, (timeStamp[r].tv_sec) * 1000000 + (timeStamp[r].tv_usec));
        }

        pthread_mutex_unlock(&count_mutex[r]);
    }

    pthread_exit(NULL);
}

void* ChangeDetector(void* arg) {
    const parm* p = (parm*) arg;
    const int target = p->tid;

    // active waiting until target value changes to 1
    while (1) {
        //use a temporary variable in order to load signalArray[target] once in each loop
        unsigned int t;
        while ((t = signalArray[target]) == oldValues[target]) {}

        pthread_mutex_lock(&count_mutex[target]);

        oldValues[target] = t;
        if (t) {
            // signal activated: 0->1
            struct timeval tv;
            gettimeofday(&tv, NULL);
            printf("D %d %lu\n", target, (tv.tv_sec) * 1000000 + (tv.tv_usec));
        }

        USE_ACK(acknowledged[target] = 1);

        pthread_cond_signal(&count_threshold_cv[target]);
        pthread_mutex_unlock(&count_mutex[target]);
    }
}

void* MultiChangeDetector(void* arg) {
    const parm* p = (parm*) arg;
    const unsigned int tid = p->tid;
    const unsigned int start = tid * (N / NTHREADS) +
                               (tid < N % NTHREADS ? tid : N % NTHREADS);
    const unsigned int end = start + (N / NTHREADS) + (tid < N % NTHREADS);
    unsigned int target = start;

    while (1) {
        unsigned int t;
        while ((t = signalArray[target]) == oldValues[target]) {
            // ~ pthread_cond_signal(&count_threshold_cv[target]);
            target ++;

            if (target == end) {
                target = start;
            }
        }

        pthread_mutex_lock(&count_mutex[target]);
        
        oldValues[target] = t;
        if (t) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            printf("D %d %lu\n", target, (tv.tv_sec) * 1000000 + (tv.tv_usec));
        }

        USE_ACK(acknowledged[target] = 1);
        pthread_cond_signal(&count_threshold_cv[target]);
        pthread_mutex_unlock(&count_mutex[target]);
    }
}

#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
const char LogTable256[256] = {
    -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
    LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
};

int msb_changed(int target) {
    /* Use bitwise XOR to find the different bits between signalArray[target] and
     * oldValues[target]. Return the most significant of them using log2.
     * Kinda faster than gcc's __builtin_clz() */
    // diff is 32-bit word to find the log2 of
    unsigned int diff = signalArray[target] ^ oldValues[target];
    unsigned int t, tt; // temporaries

    if ((tt = diff >> 16)) {
        return (t = tt >> 8) ? 24 + LogTable256[t] : 16 + LogTable256[tt];
    } else {
        return (t = diff >> 8) ? 8 + LogTable256[t] : LogTable256[diff];
    }
}

void* BitfieldChangeDetector(void* arg) {
    parm* p = (parm*) arg;
    const unsigned int tid = p->tid;
    const unsigned int start = tid * (total_N / NTHREADS) +
                               (tid < total_N % NTHREADS ? tid : total_N % NTHREADS);
    const unsigned int end = start + (total_N / NTHREADS) +
                             (tid < total_N % NTHREADS);
    unsigned int target = start;

    while (1) {
        unsigned int t;
        while ((t = signalArray[target]) == oldValues[target]) {
            target ++;

            if (target == end) {
                target = start;
            }
        }


        //TODO: msb should get t and oldValues[target]
        const int bit_idx = msb_changed(target);
        const int actual = bit_idx + 32 * target;
        pthread_mutex_lock(&count_mutex[actual]);
        /* oldValues[target] = t; <-- this way we lose signal changes
         * when 2 or more signals change at the same time within a bitfield. */
        /* if multiple changes happen then msb_changed() each time will find
         * the change at the most significant bit
         * because ceil(log2(x)) is the MSB of x */
        oldValues[target] ^= 1 << bit_idx;

        if ((t >> bit_idx) & 1) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            printf("D %d %lu\n", actual, (tv.tv_sec) * 1000000 + (tv.tv_usec));
        }

        USE_ACK(acknowledged[actual] = 1);
        pthread_cond_signal(&count_threshold_cv[actual]);
        pthread_mutex_unlock(&count_mutex[actual]);
    }
}
