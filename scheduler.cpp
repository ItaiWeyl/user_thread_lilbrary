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
int Scheduler::pendingDeletionTid = -1;
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

    unblockTimerSignal();
    doContextSwitch();
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

    if (tid == 0)
    {
        for (auto & thread : threads)
        {
            delete thread.second;
        }
        threads.clear();
        unblockTimerSignal();
        exit(0);
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
        // Debug: No threads left to run
        std::cerr << "thread library error: no threads left to run after termination\n";
        unblockTimerSignal();
        return -1;
    }

    pendingDeletionTid = currentTid;
    threads[pendingDeletionTid]->setState(READY);
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

    if (currentTid != pendingDeletionTid && threads.size() > 1) {
        threads[currentTid]->setState(READY);
        readyQueue.push(currentTid);
    }

    // Save the current thread's environment
    int ret_val = sigsetjmp(threads[currentTid]->getEnv(), 1);
    if (ret_val != 0) {
        // Returning to this thread
        if (pendingDeletionTid != -1) {
            delete threads[pendingDeletionTid];
            threads.erase(pendingDeletionTid);
            pendingDeletionTid = -1;
        }
        unblockTimerSignal();
        return;
    }

    if (!readyQueue.empty()){
        int nextTid = readyQueue.front();
        readyQueue.pop();
        currentTid = nextTid;
        threads[currentTid]->setState(RUNNING);
    }
    threads[currentTid]->incrementQuantumCount();
    totalQuantums++;

    setupTimer();
    siglongjmp(threads[currentTid]->getEnv(), 1);
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



void Scheduler::debugPrintThreads()
{
    std::cout << "=== THREADS REGISTERS DEBUG INFO ===" << std::endl;
    for (const auto& [tid, thread] : threads)
    {
        std::cout << "Thread ID: " << tid << ", ";

        address_t sp = thread->getEnv()->__jmpbuf[JB_SP];
        address_t pc = thread->getEnv()->__jmpbuf[JB_PC];

        std::cout << "SP: 0x" << std::hex << sp << ", ";
        std::cout << "PC: 0x" << std::hex << pc << ", ";

        std::cout << "State: ";

        switch (thread->getState())
        {
            case READY:
                std::cout << "READY";
                break;
            case RUNNING:
                std::cout << "RUNNING";
                break;
            case BLOCKED:
                std::cout << "BLOCKED";
                break;
            default:
                std::cout << "UNKNOWN";
                break;
        }

        std::cout << ", Quantums: " << std::dec << thread->getQuantumCount() << std::endl;
    }
    // Now print the readyQueue contents
    std::cout << "--- Ready Queue ---" << std::endl;
    std::queue<int> tempQueue = readyQueue; // copy it so we don't destroy the original
    while (!tempQueue.empty())
    {
        std::cout << tempQueue.front() << " ";
        tempQueue.pop();
    }
    std::cout << std::endl;
    std::cout << "====================================" << std::endl;
}

