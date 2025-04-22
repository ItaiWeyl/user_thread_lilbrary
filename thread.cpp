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

Thread::Thread(int id, void (*entryPoint)()):
    id(id), state(READY), quantumCount(0), stack(nullptr)
{
  if (sigsetjmp(env, 1) != 0) {
    return; // Should not happen during construction
  }

  if (id == 0) {
      // Main thread: does not need stack or manual context setup
      return;
  }

  stack = new(std::nothrow) char[STACK_SIZE];
  if (stack == nullptr) {
    std::cerr << "system error: cannot allocate stack\n";
    exit(1);
  }

  // Set initial stack pointer and program counter
  auto sp = (address_t)(stack + STACK_SIZE - sizeof(address_t));
  auto pc = (address_t)(entryPoint);

  sp = translate_address(sp);
  pc = translate_address(pc);

  env->__jmpbuf[JB_SP] = sp;
  env->__jmpbuf[JB_PC] = pc;

  sigemptyset(&env->__saved_mask);
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