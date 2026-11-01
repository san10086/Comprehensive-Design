#define _POSIX_C_SOURCE 200809L
#include "ipc_protocol.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t stopping = 0;

static void stop_peer(int signum)
{
    (void)signum;
    stopping = 1;
}

static int parse_id(const char *text, int *id)
{
    char *end;
    long value;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno || !text[0] || *end || value < 0 || value > 2147483647L)
        return -1;
    *id = (int)value;
    return 0;
}

static int change_sem(int id, unsigned short index, short change)
{
    struct sembuf op;
    op.sem_num = index;
    op.sem_op = change;
    op.sem_flg = 0;
    while (!stopping) {
        if (semop(id, &op, 1) == 0)
            return 0;
        if (errno != EINTR)
            return -1;
    }
    return -1;
}

int main(int argc, char **argv)
{
    int sem_id, shm_id = -1, sequence;
    int result = 0;
    int data_mode;
    Ex04IpcMessage *message = NULL;
    struct sigaction action;
    struct timespec delay;

    if (argc < 3 || (strcmp(argv[1], "sync") && strcmp(argv[1], "data"))) {
        fprintf(stderr, "Usage: %s sync SEM_ID | data SEM_ID SHM_ID\n", argv[0]);
        return 2;
    }
    data_mode = strcmp(argv[1], "data") == 0;
    if (argc != (data_mode ? 4 : 3) || parse_id(argv[2], &sem_id) < 0 ||
            (data_mode && parse_id(argv[3], &shm_id) < 0))
        return 2;
    memset(&action, 0, sizeof(action));
    action.sa_handler = stop_peer;
    sigemptyset(&action.sa_mask);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (data_mode) {
        message = (Ex04IpcMessage *)shmat(shm_id, NULL, 0);
        if (message == (void *)-1) {
            perror("shmat");
            return 1;
        }
    }
    for (sequence = 1; sequence <= EX04_IPC_MESSAGES && !stopping; ++sequence) {
        if (change_sem(sem_id, EX04_IPC_WRITABLE, -1) < 0) {
            result = stopping ? 0 : 1;
            break;
        }
        if (data_mode) {
            message->sequence = (unsigned int)sequence;
            snprintf(message->text, sizeof(message->text), "ARM C process: message %d", sequence);
        }
        if (change_sem(sem_id, EX04_IPC_READABLE, 1) < 0) {
            result = stopping ? 0 : 1;
            break;
        }
        printf("IPC_SENT sequence=%d\n", sequence);
        delay.tv_sec = 0;
        delay.tv_nsec = 200000000L;
        while (!stopping && nanosleep(&delay, &delay) < 0 && errno == EINTR) {}
    }
    /* Wait for the final acknowledgement before exiting. */
    if (!result && !stopping && change_sem(sem_id, EX04_IPC_WRITABLE, -1) < 0)
        result = 1;
    if (message)
        shmdt(message);
    if (result)
        perror("IPC peer");
    return result;
}
