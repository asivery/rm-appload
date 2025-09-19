#pragma once
#include <sys/types.h>
#include <poll.h>
#include <map>
#include "fileident.h"

#define SHIM_INPUT_RM1 0
#define SHIM_INPUT_RM2 1
#define SHIM_INPUT_RMPP 2
#define SHIM_INPUT_RMPPM 3

int inputShimOpen(fileident_t ident, int flags, mode_t mode);
int inputShimClose(int fd, int (*realClose)(int fd));
int inputShimIoctl(int fd, unsigned long request, char *ptr, int (*realFopen)(int fd, unsigned long request, char *ptr));
void startPollingThread();


struct PerFDEventQueue {
    int queueType;
    int queueRead;
    int queueWrite;

    PerFDEventQueue(int e, int r, int w): queueType(e), queueRead(r), queueWrite(w) {}
};

extern struct PIDEventQueue {
    std::map<int, PerFDEventQueue> eventQueue;
    struct PIDEventQueue *next;
} *pidEventQueue;