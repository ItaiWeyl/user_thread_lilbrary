//
// Created by dvirs on 4/22/2025.
//

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_


#include "thread.h"
#include <unordered_map>
#include <queue>
#include <signal.h>

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
  static std::unordered_map<int, Thread*> threads;
  static std::queue<int> readyQueue;
  static std::unordered_map<int, int> sleepingThreads;
  static int currentTid;

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
  static int pendingDeletionTid;
  static Thread *getThreadById (int tid);
};

#endif //_SCHEDULER_H_
