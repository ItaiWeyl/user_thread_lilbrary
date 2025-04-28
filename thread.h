#ifndef THREAD_H
#define THREAD_H

#include <setjmp.h>
#include <signal.h>
#include <cassert>    // or <assert.h>


#define STACK_SIZE 4096

#ifdef __x86_64__
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
    sigjmp_buf env{};
    int id;
    ThreadState state;
    char* stack;
    int quantumCount;
    bool didUserBlock;

    static address_t translate_address(address_t addr);

public:
    Thread(int id, void (*entryPoint)());

    ~Thread();

    ThreadState getState() const;

    void setState(ThreadState newState);

    sigjmp_buf& getEnv();

    int getQuantumCount() const;

    void incrementQuantumCount();

    bool isUserBlocked() const;

    void setBlockFlag(bool shouldSleep);

};

#endif // THREAD_H