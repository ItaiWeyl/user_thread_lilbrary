#include "scheduler.h"
#include "uthreads.h"
#include <iostream>
#include <sys/time.h>

Scheduler scheduler;
// Static member definitions
int Scheduler::quantumUsecs = 0;
int Scheduler::totalQuantums = 0;
int Scheduler::currentTid = 0;
int Scheduler::lastPendingDeleteId = -1;
bool Scheduler::shouldExit = false;
Thread* Scheduler::threads[MAX_THREAD_NUM] = {nullptr};
int Scheduler::readyQueue[MAX_THREAD_NUM];
int Scheduler::readyFront = 0;
int Scheduler::readyBack = 0;
int Scheduler::sleepUntil[MAX_THREAD_NUM] = {0};
int Scheduler::pendingDeletionQueue[MAX_THREAD_NUM];
int Scheduler::pendingDeletionSize = 0;


//************************* Implementation of the private functions ****************************************************
void Scheduler::removeFromReadyQueue(int tid)
{
  blockTimerSignal();

  int newQueue[MAX_THREAD_NUM];
  int newBack = 0;

  while (readyFront != readyBack)
  {
    int current = readyQueue[readyFront];
    readyFront = (readyFront + 1) % MAX_THREAD_NUM;

    if (current != tid)
    {
      newQueue[newBack++] = current;
    }
  }

  // Copy filtered queue back
  for (int i = 0; i < newBack; ++i)
  {
    readyQueue[i] = newQueue[i];
  }

  readyFront = 0;
  readyBack = newBack;

  unblockTimerSignal();
}


int Scheduler::nextAvailableTid()
{
  for (int tid = 0; tid < MAX_THREAD_NUM; ++tid)
  {
    if (threads[tid] == nullptr)
    {
      return tid;
    }
  }
  return -1; // No available TID found
}


void Scheduler::wakeSleepingThreads()
{
  for (int tid = 0; tid < MAX_THREAD_NUM; ++tid)
  {
    if (sleepUntil[tid] > 0 && totalQuantums >= sleepUntil[tid])
    {
      if (threads[tid] != nullptr && !threads[tid]->isUserBlocked())
      {
        threads[tid]->setState(READY);
        readyQueue[readyBack] = tid;
        readyBack = (readyBack + 1) % MAX_THREAD_NUM;
      }
      sleepUntil[tid] = 0;
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

  for (int tid = 0; tid < MAX_THREAD_NUM; ++tid)
  {
    if (threads[tid] != nullptr)
    {
      delete threads[tid];
      threads[tid] = nullptr;
    }
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
  readyFront = 0;
  readyBack = 0;

  // Push thread 0 to be the only ready thread
  readyQueue[readyBack++] = 0;
  readyBack %= MAX_THREAD_NUM;
}


void Scheduler::jumpToNextThread()
{
  int nextTid = readyQueue[readyFront];
  readyFront = (readyFront + 1) % MAX_THREAD_NUM;

  currentTid = nextTid;
  threads[currentTid]->setState(RUNNING);
  threads[currentTid]->incrementQuantumCount();
  totalQuantums++;
  setupTimer();
  unblockTimerSignal();
  siglongjmp(threads[currentTid]->getEnv(), 1);
}


void Scheduler::cleanupPendingThreads()
{
  for (int i = 0; i < pendingDeletionSize; ++i)
  {
    int toDelete = pendingDeletionQueue[i];
    if (threads[toDelete] != nullptr)
    {
      delete threads[toDelete];
      threads[toDelete] = nullptr;
    }
  }
  pendingDeletionSize = 0; // reset queue
}


// **************************** Implementation of the Scheduler API ****************************************************
int Scheduler::init(int quantum_usecs) {
  quantumUsecs = quantum_usecs;
  for (int i = 0; i < MAX_THREAD_NUM; ++i) {
    threads[i] = nullptr;
    sleepUntil[i] = 0;
  }
  readyFront = readyBack = 0;
  pendingDeletionSize = 0;


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


int Scheduler::spawn(void (*entryPoint)())
{
  // Find the smallest available TID
  int tid = nextAvailableTid();
  if (tid == -1)
  {
    std::cerr << "thread library error: reached maximum thread limit" << std::endl;
    return -1;
  }

  blockTimerSignal();
  auto* newThread = new Thread(tid, entryPoint);
  threads[tid] = newThread;

  // Push to ready queue
  readyQueue[readyBack] = tid;
  readyBack = (readyBack + 1) % MAX_THREAD_NUM;

  unblockTimerSignal();
  return tid;
}


int Scheduler::terminate(int tid)
{
  blockTimerSignal();

  if (tid == 0)
  {
    if (currentTid == 0)
    {
      safeExit();
    }
    else
    {
      prepareExit();
      shouldExit = true;
      unblockTimerSignal();
      doContextSwitch();
    }
    return 0;
  }

  if (threads[tid] != nullptr && threads[tid]->getState() == READY)
  {
    removeFromReadyQueue(tid);
  }

  if (tid != currentTid)
  {
    sleepUntil[tid] = 0;
    delete threads[tid];
    threads[tid] = nullptr;
    unblockTimerSignal();
    return 0;
  }

  if (readyFront == readyBack)
  {
    std::cerr << "thread library error: no threads left to run after termination\n";
    unblockTimerSignal();
    return -1;
  }

  // Delay deletion of the current thread
  pendingDeletionQueue[pendingDeletionSize++] = currentTid;
  lastPendingDeleteId = currentTid;
  threads[currentTid]->setState(READY);

  unblockTimerSignal(); // Unblock before switching
  doContextSwitch();
  return 0;
}


int Scheduler::block(int tid)
{
  blockTimerSignal();
  Thread* thread = threads[tid];
  ThreadState state = thread->getState();

  if (thread->isUserBlocked())
  {
    unblockTimerSignal();
    return 0; // Already blocked by user
  }

  if (state == READY || state == BLOCKED)
  {
    if (state == READY)
    {
      removeFromReadyQueue(tid);
    }
    thread->setState(BLOCKED);
    thread->setBlockFlag(true);
    unblockTimerSignal();
    return 0;
  }

  if (tid == currentTid)
  {
    thread->setState(BLOCKED);

    if (readyFront == readyBack) // No other ready thread
    {
      std::cerr << "thread library error: no threads left to run after blocking\n";
      thread->setState(RUNNING);
      unblockTimerSignal();
      return -1;
    }

    thread->setBlockFlag(true);
    unblockTimerSignal();  // Make sure it's unblocked before switching
    doContextSwitch();
    return 0;
  }

  unblockTimerSignal();
  return 0;
}


int Scheduler::resume(int tid)
{
  blockTimerSignal();

  if (tid < 0 || tid >= MAX_THREAD_NUM || threads[tid] == nullptr)
  {
    std::cerr << "thread library error: invalid tid" << std::endl;
    unblockTimerSignal();
    return -1;
  }

  Thread* thread = threads[tid];

  // Do nothing if the thread is not currently blocked
  if (thread->getState() != BLOCKED)
  {
    unblockTimerSignal();
    return 0;
  }

  // If the thread is also sleeping, only update blocked flag
  if (sleepUntil[tid] > 0)
  {
    thread->setBlockFlag(false);
    unblockTimerSignal();
    return 0;
  }

  // Move the thread to READY state and enqueue
  thread->setState(READY);
  thread->setBlockFlag(false);
  readyQueue[readyBack] = tid;
  readyBack = (readyBack + 1) % MAX_THREAD_NUM;

  unblockTimerSignal();
  return 0;
}


int Scheduler::sleep(int numQuantums)
{
  blockTimerSignal();
  Thread* thread = threads[currentTid];
  thread->setState(BLOCKED);
  sleepUntil[currentTid] = totalQuantums + numQuantums;

  if (readyFront == readyBack) // No other ready thread
  {
    std::cerr << "thread library error: no threads left to run after sleep\n";
    thread->setState(RUNNING);
    sleepUntil[currentTid] = 0;
    unblockTimerSignal();
    return -1;
  }

  unblockTimerSignal();
  doContextSwitch();
  return 0;
}


void Scheduler::doContextSwitch()
{
  blockTimerSignal();

  if (lastPendingDeleteId != currentTid &&
      threads[currentTid] != nullptr &&
      threads[currentTid]->getState() == RUNNING)
  {
    threads[currentTid]->setState(READY);
    readyQueue[readyBack] = currentTid;
    readyBack = (readyBack + 1) % MAX_THREAD_NUM;
  }

  int ret_val = sigsetjmp(threads[currentTid]->getEnv(), 1);
  if (ret_val != 0)
  {
    if (shouldExit && currentTid == 0)
    {
      safeExit();
    }
    if (lastPendingDeleteId != -1)
    {
      cleanupPendingThreads();
      lastPendingDeleteId = -1;
    }
    unblockTimerSignal();
    return;
  }

  if (readyFront == readyBack) // No threads ready
  {
    if (lastPendingDeleteId != -1)
    {
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


Thread* Scheduler::getThreadById(int tid)
{
  if (tid < 0 || tid >= MAX_THREAD_NUM || threads[tid] == nullptr)
  {
    return nullptr;
  }
  return threads[tid];
}


int Scheduler::getTotalQuantums() {
  return totalQuantums;
}


int Scheduler::getQuantums(int tid)
{
  blockTimerSignal();
  if (tid < 0 || tid >= MAX_THREAD_NUM || threads[tid] == nullptr)
  {
    std::cerr << "thread library error: invalid tid" << std::endl;
    unblockTimerSignal();
    return -1;
  }

  int quantumCount = threads[tid]->getQuantumCount();
  unblockTimerSignal();
  return quantumCount;
}
