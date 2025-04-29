#include "thread.h"
#include <cstdlib>
#include <iostream>

// Translate address exactly like in demo_jmp.c
address_t Thread::translate_address(address_t addr) {
#ifdef __x86_64__
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
               "rol    $0x11,%0\n"
      : "=g" (ret)
      : "0" (addr));
  return ret;
#else
  address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
#endif
}

Thread::Thread(int id, void (*entryPoint)()) :
    id(id), state(READY), quantumCount(0), stack(nullptr), didUserBlock(false)
{
  if (id == 0) {
    // Main thread: no need to set up stack or context manually
    return;
  }

  stack = new(std::nothrow) char[STACK_SIZE];
  if (stack == nullptr) {
    std::cerr << "system error: cannot allocate stack\n";
    exit(1);
  }

  char* sp_ptr = stack + STACK_SIZE - sizeof(address_t);
  address_t sp = (address_t)(sp_ptr);
  address_t pc = (address_t)(entryPoint);


  if (sigsetjmp(env, 1) == 0) {
    env->__jmpbuf[JB_SP] = translate_address(sp);
    env->__jmpbuf[JB_PC] = translate_address(pc);
    sigemptyset(&env->__saved_mask);
  }
}

Thread::~Thread() {
  if (id != 0 && stack != nullptr) {
    delete[] stack;
  }
}

ThreadState Thread::getState() const {
  return state;
}

void Thread::setState(const ThreadState newState) {
  state = newState;
}

sigjmp_buf& Thread::getEnv() {
  return env;
}

int Thread::getQuantumCount() const {
  return quantumCount;
}

void Thread::incrementQuantumCount() {
  quantumCount++;
}

bool Thread::isUserBlocked()const{
  return didUserBlock;
}

void Thread::setBlockFlag(const bool flag) {
  didUserBlock = flag;
}