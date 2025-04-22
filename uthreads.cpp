// uthreads.cpp
#include "uthreads.h"
#include "scheduler.h"

int uthread_init(int quantum_usecs) {
  return Scheduler::init(quantum_usecs);
}

int uthread_spawn(thread_entry_point entry_point) {
  return Scheduler::spawn(entry_point);
}

int uthread_terminate(int tid) {
  return Scheduler::terminate(tid);
}

int uthread_block(int tid) {
  return Scheduler::block(tid);
}

int uthread_resume(int tid) {
  return Scheduler::resume(tid);
}

int uthread_sleep(int num_quantums) {
  return Scheduler::sleep(num_quantums);
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
