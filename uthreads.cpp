// uthreads.cpp
#include <iostream>
#include "uthreads.h"
#include "scheduler.h"

int uthread_init(int quantum_usecs) {
  if (quantum_usecs <= 0) {
    std::cerr << "thread library error: quantum_usecs must be positive" << std::endl;
    return -1;
  }
  return Scheduler::init(quantum_usecs);
}

int uthread_spawn(thread_entry_point entry_point) {
  if (entry_point == nullptr) {
    std::cerr << "thread library error: entryPoint cannot be null" << std::endl;
    return -1;
  }
  return Scheduler::spawn(entry_point);
}

int uthread_terminate(int tid) {
  if (tid < 0 || tid >= MAX_THREAD_NUM) {
    std::cerr << "thread library error: invalid tid" << std::endl;
    return -1;
  }
  if (Scheduler::getThreadById(tid) == nullptr) {
    std::cerr << "thread library error: there is no thread with id: " << tid << std::endl;
    return -1;
  }
  return Scheduler::terminate(tid);
}

int uthread_block(int tid) {
  if (tid < 0 || tid >= MAX_THREAD_NUM) {
    std::cerr << "thread library error: invalid tid" << std::endl;
    return -1;
  }
  if (tid == 0) {
    std::cerr << "thread library error: cannot block main thread" << std::endl;
    return -1;
  }
  if (Scheduler::getThreadById(tid) == nullptr) {
    std::cerr << "thread library error: there is no thread with id: " << tid << std::endl;
    return -1;
  }
  return Scheduler::block(tid);
}

int uthread_resume(int tid) {
  if (tid < 0 || tid >= MAX_THREAD_NUM) {
    std::cerr << "thread library error: invalid tid" << std::endl;
    return -1;
  }
  if (Scheduler::getThreadById(tid) == nullptr) {
    std::cerr << "thread library error: there is no thread with id: " << tid << std::endl;
    return -1;
  }
  return Scheduler::resume(tid);
}

int uthread_sleep(int num_quantums) {
  if (num_quantums <= 0) {
    std::cerr << "thread library error: numQuantums must be positive" << std::endl;
    return -1;
  }
  if (Scheduler::getTid() == 0) {
    std::cerr << "thread library error: main thread cannot sleep" << std::endl;
    return -1;
  }
  return Scheduler::sleep (num_quantums);
}

int uthread_get_tid() {
  return Scheduler::getTid();
}

int uthread_get_total_quantums() {
  return Scheduler::getTotalQuantums();
}

int uthread_get_quantums(int tid) {
  return Scheduler::getQuantums(tid);
}