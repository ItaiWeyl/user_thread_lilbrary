//
// Created by dvirs on 4/22/2025.
//

#include "scheduler.h"
#include "uthreads.h"
#include <iostream>
#include <sys/time.h>

Scheduler scheduler;
// Static variables initialization
int Scheduler::quantumUsecs = 0;
int Scheduler::totalQuantums = 0;
int Scheduler::currentTid = 0;
std::queue<int> Scheduler::pendingDeletionQueue;
int Scheduler::lastPendingDeleteId = -1;
bool Scheduler::shouldExit = false;
std::unordered_map<int, Thread*> Scheduler::threads;
std::queue<int> Scheduler::readyQueue;
std::unordered_map<int, int> Scheduler::sleepingThreads;

//************************* Implementation of the private functions ****************************************************
void Scheduler::removeFromReadyQueue(int tid) {
  blockTimerSignal();
  std::queue<int> tempQueue;
  while (!readyQueue.empty()) {
    int current = readyQueue.front();
    readyQueue.pop();
    if (current != tid) {
      tempQueue.push(current);
    }
  }
  readyQueue = tempQueue;
  unblockTimerSignal();
}

int Scheduler::nextAvailableTid() {
  for (int tid = 0; tid < MAX_THREAD_NUM; ++tid) {
    if (threads.count(tid) == 0) {
      return tid;
    }
  }
  return -1; // No available TID found
}

void Scheduler::wakeSleepingThreads() {
  auto it = sleepingThreads.begin();
  for ( ;it != sleepingThreads.end(); ) {
    if (totalQuantums >= it->second) {
      int tid = it->first;
      if (!threads[tid]->isUserBlocked()) {
        threads[tid]->setState(READY);
        readyQueue.push(tid);
      }
      it = sleepingThreads.erase(it);
    } else {
      ++it;
    }
  }
}

void Scheduler::timerHandler(int sig) {
  blockTimerSignal();
  wakeSleepingThreads();
  doContextSwitch();
  unblockTimerSignal();
}

void Scheduler::setupSignalHandler() {
  struct sigaction sa = {};\


  sa.sa_handler = &Scheduler::timerHandler;
  sigemptyset(&sa.sa_mask); // optional: don't block any signals during handler
  sa.sa_flags = 0;

  if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
    std::cerr << "system error: failed to set signal handler" << std::endl;
    exit(1);
  }
}

void Scheduler::setupTimer() {
  struct itimerval timer{};
  timer.it_value.tv_sec = quantumUsecs / 1000000;
  timer.it_value.tv_usec = quantumUsecs % 1000000;
  timer.it_interval.tv_sec = timer.it_value.tv_sec;
  timer.it_interval.tv_usec = timer.it_value.tv_usec;

  if (setitimer(ITIMER_VIRTUAL, &timer, nullptr) < 0) {
    std::cerr << "system error: failed to set timer" << std::endl;
    exit(1);
  }
}

void Scheduler::blockTimerSignal() {
  sigset_t set;
  if (sigemptyset(&set) == -1) {
    std::cerr << "system error: sigemptyset failed" << std::endl;
    exit(1);
  }
  if (sigaddset(&set, SIGVTALRM) == -1) {
    std::cerr << "system error: sigaddset failed" << std::endl;
    exit(1);
  }
  if (sigprocmask(SIG_BLOCK, &set, nullptr) == -1) {
    std::cerr << "system error: sigprocmask BLOCK failed" << std::endl;
    exit(1);
  }
}

void Scheduler::unblockTimerSignal() {
  sigset_t set;
  if (sigemptyset(&set) == -1) {
    std::cerr << "system error: sigemptyset failed" << std::endl;
    exit(1);
  }
  if (sigaddset(&set, SIGVTALRM) == -1) {
    std::cerr << "system error: sigaddset failed" << std::endl;
    exit(1);
  }
  if (sigprocmask(SIG_UNBLOCK, &set, nullptr) == -1) {
    std::cerr << "system error: sigprocmask UNBLOCK failed" << std::endl;
    exit(1);
  }
}

void Scheduler::safeExit()
{
  blockTimerSignal();
  for (auto it = threads.begin(); it != threads.end(); )
  {
    if (it->second != nullptr)
    {
      delete it->second;
    }
    it = threads.erase(it);
  }

  struct itimerval timer{};
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 0;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;
  setitimer(ITIMER_VIRTUAL, &timer, nullptr);

  unblockTimerSignal();
  fflush(stdout);
  exit(0);
}

void Scheduler::prepareExit()
{
  // Clear ready queue
  while (!readyQueue.empty())
  {
    readyQueue.pop();
  }

  // Push thread 0 to be the only ready thread
  readyQueue.push(0);
}

void Scheduler::jumpToNextThread() {
  int nextTid = readyQueue.front();
  readyQueue.pop();
  currentTid = nextTid;
  threads[currentTid]->setState(RUNNING);
  threads[currentTid]->incrementQuantumCount();
  totalQuantums++;
  setupTimer();
  unblockTimerSignal();
  siglongjmp(threads[currentTid]->getEnv(), 1);
}

void Scheduler::cleanupPendingThreads() {
  while (!pendingDeletionQueue.empty()) {
    int toDelete = pendingDeletionQueue.front();
    pendingDeletionQueue.pop();

    if (threads.count(toDelete)) {
      delete threads[toDelete];
      threads.erase(toDelete);
    }
  }
}

// **************************** Implementation of the Scheduler API ****************************************************
int Scheduler::init(int quantum_usecs) {
  quantumUsecs = quantum_usecs;

  // Create main thread (tid 0)
  auto* mainThread = new Thread(0, nullptr); // No entry point for main thread
  threads[0] = mainThread;
  mainThread->setState(RUNNING);
  currentTid = 0;
  mainThread->incrementQuantumCount();
  totalQuantums = 1; // Main thread gets the first quantum

  setupSignalHandler();
  setupTimer();

  return 0;
}

int Scheduler::spawn(void (*entryPoint)()) {
  // Find the smallest available TID
  int tid = nextAvailableTid();
  if (tid == -1) {
    std::cerr << "thread library error: reached maximum thread limit" << std::endl;
    return -1;
  }

  // Create the new thread and add it to the map and ready queue
  blockTimerSignal();
  auto* newThread = new Thread(tid, entryPoint);
  threads[tid] = newThread;
  readyQueue.push(tid);
  unblockTimerSignal();
  return tid;
}

int Scheduler::terminate(int tid) {
  blockTimerSignal();

  if (tid == 0) {
    if (currentTid == 0) {
      safeExit();
    } else {
      prepareExit();
      shouldExit = true;
      unblockTimerSignal();
      doContextSwitch();
    }
    return 0;
  }

  if (threads[tid]->getState() == READY) {
    removeFromReadyQueue(tid);
  }

  if (tid != currentTid) {
    sleepingThreads.erase(tid);
    delete threads[tid];
    threads.erase(tid);
    unblockTimerSignal();
    return 0;
  }

  if (readyQueue.empty()) {
    std::cerr << "thread library error: no threads left to run after termination\n";
    unblockTimerSignal();
    return -1;
  }

  // For self-termination, we need to ensure the thread's resources aren't cleaned up
  // until after we've switched away from it
  pendingDeletionQueue.push(currentTid);
  lastPendingDeleteId = currentTid;
  threads[currentTid]->setState(READY);
  unblockTimerSignal();  // Unblock before context switch
  doContextSwitch();
  return 0;
}

int Scheduler::block(int tid) {
  Thread* thread = threads[tid];
  ThreadState state = thread->getState();

  if (thread->isUserBlocked()) {
    return 0; // Already blocked by user
  }

  if (state == READY || state == BLOCKED) {
    blockTimerSignal();
    // Remove from ready queue if in it
    if (state == READY){
      removeFromReadyQueue(tid);
    }
    thread->setState(BLOCKED);
    thread->setBlockFlag(true);
    unblockTimerSignal();
    return 0;
  }

  if (tid == currentTid) {
    blockTimerSignal();
    thread->setState(BLOCKED);

    if (readyQueue.empty()) { // No option to block without other ready thread
      std::cerr << "thread library error: no threads left to run after blocking\n";
      thread->setState(RUNNING);
      unblockTimerSignal();
      return -1;
    }
    thread->setBlockFlag(true);
    doContextSwitch();
    return 0;
  }

  return 0;
}

int Scheduler::resume(int tid) {
  if (tid < 0 || threads.count(tid) == 0) {
    std::cerr << "thread library error: invalid tid" << std::endl;
    return -1;
  }

  Thread* thread = threads[tid];

  // Do nothing if the thread is not currently blocked
  if (thread->getState() != BLOCKED) {
    return 0;
  }

  blockTimerSignal();
  // If the thread is also sleeping, only change blocked flag
  if (sleepingThreads.count(tid) > 0) {
    // Sleep time has not passed yet, keep it blocked but switch the flag
    thread->setBlockFlag(false);
    unblockTimerSignal();
    return 0;
  }

  // Move the thread to READY state and push it to the ready queue
  thread->setState(READY);
  readyQueue.push(tid);
  unblockTimerSignal();
  return 0;
}

int Scheduler::sleep(int numQuantums) {
  blockTimerSignal();
  Thread* thread = threads[currentTid];
  thread->setState(BLOCKED);
  sleepingThreads[currentTid] = totalQuantums + numQuantums;

  if (readyQueue.empty()) { // No option to sleep without other thread ready
    std::cerr << "thread library error: no threads left to run after sleep\n";
    thread->setState(RUNNING);
    sleepingThreads.erase(currentTid);
    unblockTimerSignal();
    return -1;
  }

  doContextSwitch();
  return 0;
}

void Scheduler::doContextSwitch() {
  blockTimerSignal();

  if (lastPendingDeleteId != currentTid &&
      threads.size() > 1 &&
      threads[currentTid]->getState() == RUNNING) {
    threads[currentTid]->setState(READY);
    readyQueue.push(currentTid);
  }

  int ret_val = sigsetjmp(threads[currentTid]->getEnv(), 1);
  if (ret_val != 0) {
    if (shouldExit && currentTid == 0) {
      safeExit();
    }
    if (lastPendingDeleteId != -1){
      cleanupPendingThreads();
      lastPendingDeleteId = -1;
    }
    unblockTimerSignal();
    return;
  }

  if (readyQueue.empty()) {
    if (lastPendingDeleteId != -1) {
      safeExit();
    }

    threads[currentTid]->incrementQuantumCount();
    threads[currentTid]->setState(RUNNING);
    totalQuantums++;
    setupTimer();
    unblockTimerSignal();
    return;
  }

  jumpToNextThread();
}

int Scheduler::getTid() {
  return currentTid;
}

Thread* Scheduler::getThreadById(int tid) {
  if (threads.count(tid) == 0) return nullptr;
  return threads[tid];
}

int Scheduler::getTotalQuantums() {
  return totalQuantums;
}

int Scheduler::getQuantums(int tid) {
  blockTimerSignal();
  if (threads.count(tid) == 0) {
    std::cerr << "thread library error: invalid tid" << std::endl;
    return -1;
  }
  unblockTimerSignal();
  return threads[tid]->getQuantumCount();
}
