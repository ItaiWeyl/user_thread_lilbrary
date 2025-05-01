#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_
#include "thread.h"
#define MAX_THREAD_NUM 100

class Scheduler {
private:
    static void setupSignalHandler();
    static void setupTimer();
    static int nextAvailableTid();
    static void removeFromReadyQueue(int tid);
    static void wakeSleepingThreads();
    static void blockTimerSignal();
    static void unblockTimerSignal();

    static int quantumUsecs;
    static int totalQuantums;
    static bool shouldExit;
    static Thread* threads[MAX_THREAD_NUM];
    static int readyQueue[MAX_THREAD_NUM];
    static int readyFront, readyBack;
    static int sleepUntil[MAX_THREAD_NUM]; // 0 means not sleeping
    static int pendingDeletionQueue[MAX_THREAD_NUM];
    static int pendingDeletionSize;
    static int currentTid;
    static int lastPendingDeleteId;


public:
    static int init(int quantumUsecs);
    static int spawn(void (*entryPoint)(void));
    static int terminate(int tid);
    static int block(int tid);
    static int resume(int tid);
    static int sleep(int numQuantums);
    static void timerHandler(int sig);
    static void doContextSwitch();
    static int getTid();
    static int getTotalQuantums();
    static int getQuantums(int tid);
    static Thread *getThreadById (int tid);
    static void safeExit ();
    static void prepareExit ();
    static void jumpToNextThread ();
    static void cleanupPendingThreads ();
};

#endif //_SCHEDULER_H_