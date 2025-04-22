//
// Created by dvirs on 4/22/2025.
//

#include "scheduler.h"
#include "uthreads.h"
#include <iostream>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>

// Static variables initialization
int Scheduler::quantumUsecs = 0;
int Scheduler::totalQuantums = 0;
int Scheduler::currentTid = 0;
std::unordered_map<int, Thread*> Scheduler::threads;
std::queue<int> Scheduler::readyQueue;
std::unordered_map<int, int> Scheduler::sleepingThreads;

// Implementation of the private functions
void Scheduler::removeFromReadyQueue(int tid) {
  std::queue<int> tempQueue;
  while (!readyQueue.empty()) {
    int current = readyQueue.front();
    readyQueue.pop();
    if (current != tid) {
      tempQueue.push(current);
    }
  }
  readyQueue = tempQueue;
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
  for (auto it = sleepingThreads.begin(); it != sleepingThreads.end(); ) {
    if (totalQuantums >= it->second) {
      int tid = it->first;
      threads[tid]->setState(READY);
      readyQueue.push(tid);
      it = sleepingThreads.erase(it);
    } else {
      ++it;
    }
  }
}

void Scheduler::timerHandler(int sig) {
  totalQuantums++;
  wakeSleepingThreads();
  doContextSwitch();
}

void Scheduler::setupSignalHandler() {
  struct sigaction sa = {};
  sa.sa_handler = &Scheduler::timerHandler;
  sigemptyset(&sa.sa_mask); // optional: don't block any signals during handler
  sa.sa_flags = 0;

  if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
    std::cerr << "system error: failed to set signal handler" << std::endl;
    exit(1);
  }
}

void Scheduler::setupTimer() {
  struct itimerval timer;
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

// // Implementation of the Scheduler API
int Scheduler::init(int quantum_usecs) {
  if (quantum_usecs <= 0) {
    std::cerr << "thread library error: quantum_usecs must be positive" << std::endl;
    return -1;
  }
  quantumUsecs = quantum_usecs;

  // Create main thread (tid 0)
  Thread* mainThread = new Thread(0, nullptr); // No entry point for main thread
  threads[0] = mainThread;
  mainThread->setState(RUNNING);
  currentTid = 0;
  totalQuantums = 1; // Main thread gets the first quantum

  // Set up signal handler for SIGVTALRM
  setupSignalHandler();

  // Set up timer
  setupTimer();

  return 0;
}


int Scheduler::spawn(void (*entryPoint)(void)) {
  if (entryPoint == nullptr) {
    std::cerr << "thread library error: entryPoint cannot be null" << std::endl;
    return -1;
  }

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
  if (tid < 0 || threads.count(tid) == 0) {
    std::cerr << "thread library error: invalid tid" << std::endl;
    return -1;
  }

  if (tid == 0) {
    // Terminating main thread — exit entire process
    blockTimerSignal();
    std::unordered_map<int, Thread*>::iterator it;
    for (it = threads.begin(); it != threads.end(); ++it) {
      delete it->second;
    }
    threads.clear();
    unblockTimerSignal();
    exit(0);
  }

  // If the thread is in the ready queue, remove it
  if (threads[tid]->getState() == READY) {
    blockTimerSignal();
    removeFromReadyQueue(tid);
    unblockTimerSignal();
  }

  blockTimerSignal();
  if (tid == currentTid) {
    unblockTimerSignal();
    doContextSwitch();
  }

  // Delete the thread
  delete threads[tid];
  threads.erase(tid);
  unblockTimerSignal();
  return 0;
}

int Scheduler::block(int tid) {
  if (tid < 0 || threads.count(tid) == 0) {
    std::cerr << "thread library error: invalid tid" << std::endl;
    return -1;
  }

  if (tid == 0) {
    std::cerr << "thread library error: cannot block main thread" << std::endl;
    return -1;
  }

  Thread* thread = threads[tid];
  ThreadState state = thread->getState();

  if (state == BLOCKED) {
    return 0; // Already blocked
  }

  if (state == READY) {
    blockTimerSignal();
    // Remove from ready queue
    removeFromReadyQueue(tid);
    thread->setState(BLOCKED);
    unblockTimerSignal();
    return 0;
  }

  if (tid == currentTid) {
    blockTimerSignal();
    thread->setState(BLOCKED);
    unblockTimerSignal();
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
  // If the thread is also sleeping, only wake it if its sleep time has passed
  if (sleepingThreads.count(tid) > 0) {
    if (totalQuantums < sleepingThreads[tid]) {
      // Sleep time has not passed yet, keep it blocked
      unblockTimerSignal();
      return 0;
    } else {
      // Sleep time has passed, remove from sleepingThreads
      sleepingThreads.erase(tid);
    }
  }

  // Move the thread to READY state and push it to the ready queue
  thread->setState(READY);
  readyQueue.push(tid);

  unblockTimerSignal();
  return 0;
}


int Scheduler::sleep(int numQuantums) {
  if (currentTid == 0) {
    std::cerr << "thread library error: main thread cannot sleep" << std::endl;
    return -1;
  }

  if (numQuantums <= 0) {
    std::cerr << "thread library error: numQuantums must be positive" << std::endl;
    return -1;
  }
  blockTimerSignal();
  Thread* thread = threads[currentTid];
  thread->setState(BLOCKED);
  sleepingThreads[currentTid] = totalQuantums + numQuantums;

  unblockTimerSignal();
  doContextSwitch();
  return 0;
}

void Scheduler::doContextSwitch() {
  blockTimerSignal();

  // Save the current thread state
  int ret_val = sigsetjmp(threads[currentTid]->getEnv(), 1);
  if (ret_val != 0) {
    unblockTimerSignal();
    return; // We're returning to this thread later
  }

  // Count the quantum for the thread that was just running
  threads[currentTid]->incrementQuantumCount();

  // Select the next thread to run
  if (readyQueue.empty()) {
    std::cerr << "thread library error: no threads ready to run" << std::endl;
    exit(1);
  }
  int nextTid = readyQueue.front();
  readyQueue.pop();
  threads[nextTid]->setState(RUNNING);

  if (threads[currentTid]->getState() != BLOCKED){
    threads[currentTid]->setState(READY);
    readyQueue.push(currentTid);
  }

  currentTid = nextTid;

  setupTimer();
  unblockTimerSignal();
  printThreadStatus(threads, readyQueue, sleepingThreads, currentTid, totalQuantums);
  siglongjmp(threads[currentTid]->getEnv(), 1);
}

std::string threadStateToString(ThreadState state) {
  switch (state) {
    case READY: return "READY";
    case RUNNING: return "RUNNING";
    case BLOCKED: return "BLOCKED";
    default: return "UNKNOWN";
  }
}

void Scheduler::printThreadStatus(const std::unordered_map<int, Thread*>& threads,
                                  const std::queue<int>& readyQueue,
                                  const std::unordered_map<int, int>& sleepingThreads,
                                  int currentTid,
                                  int totalQuantums) {
  std::cerr << "==== Thread Map ====\n";
  for (const auto& [tid, thread] : threads) {
    std::cerr << "TID: " << tid
              << " | State: " << (thread ? threadStateToString(thread->getState()) : "NULL")
              << " | Quantum Count: " << (thread ? thread->getQuantumCount() : -1)
              << "\n";
  }

  std::cerr << "==== Ready Queue ====\n";
  std::queue<int> copyQueue = readyQueue;
  while (!copyQueue.empty()) {
    std::cerr << "TID: " << copyQueue.front() << "\n";
    copyQueue.pop();
  }

  std::cerr << "==== Sleeping Threads ====\n";
  for (const auto& [tid, wakeupTime] : sleepingThreads) {
    std::cerr << "TID: " << tid << " | Wake up at quantum: " << wakeupTime << "\n";
  }

  std::cerr << "==== State Info ====\n";
  std::cerr << "Current TID: " << currentTid << " | Total Quantums: " << totalQuantums << "\n";
  std::cerr << "====================\n";
}


int Scheduler::getTid() {
  return currentTid;
}

int Scheduler::getTotalQuantums() {
  return totalQuantums;
}

int Scheduler::getQuantums(int tid) {
  if (threads.count(tid) == 0) {
    std::cerr << "thread library error: invalid tid" << std::endl;
    return -1;
  }
  return threads[tid]->getQuantumCount();
}


