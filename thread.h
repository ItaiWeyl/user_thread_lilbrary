#ifndef THREAD_H
#define THREAD_H

#include <setjmp.h>
#include <signal.h>

#define STACK_SIZE 4096

#ifdef _x86_64_
    #define JB_SP 6
    #define JB_PC 7
    typedef unsigned long address_t;
#else
#define JB_SP 4
#define JB_PC 5
typedef unsigned int address_t;
#endif

// Thread states
enum ThreadState { READY, RUNNING, BLOCKED };

class Thread {

private:
    int id;
    ThreadState state;
    sigjmp_buf env;
    char* stack;
    int quantumCount;

    static address_t translate_address(address_t addr);

public:
    Thread(int id, void (*entryPoint)());

    ~Thread();

    int getId() const;

    ThreadState getState() const;

    void setState(ThreadState newState);

    sigjmp_buf& getEnv();

    int getQuantumCount() const;

    void incrementQuantumCount();
};

#endif // THREAD_H