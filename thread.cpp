#include "Thread.h"
#include <cstdlib>
#include <iostream>
#include <unistd.h>

// Translate address exactly like in demo_jmp.c
address_t Thread::translate_address(address_t addr) {
#ifdef _x86_64_
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
    if (id == 0) {
        // Main thread: does not need stack or manual context setup
        return;
    }

    stack = new char[STACK_SIZE];
    if (stack == nullptr) {
        std::cerr << "system error: cannot allocate stack\n";
        exit(1);
    }

    if (sigsetjmp(env, 1) != 0) {
        return; // Should not happen during construction
    }

    // Set initial stack pointer and program counter
    address_t sp = (address_t)(stack + STACK_SIZE - sizeof(address_t));
    address_t pc = (address_t)(entryPoint);

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

int Thread::getId() const {
    return id;
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