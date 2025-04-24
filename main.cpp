// main.cpp
#include "uthreads.h"
#include <iostream>
#include <cassert>

void child_thread() {
    int tid = uthread_get_tid();
    std::cout << "Child: tid=" << tid << std::endl;
    assert(tid == 1); // should be the first spawned thread

    // Wait for at least one tick (quantum)
    int current_q = uthread_get_quantums(tid);
    while (uthread_get_quantums(tid) == current_q)
    {
        // Busy wait until the quantum count increases
    }

    int q = uthread_get_quantums(tid);
    std::cout << "Child: quantums=" << q << std::endl;
    assert(q > 0);

    uthread_terminate(tid);
}

int main() {
    // 1) Initialize with a 1ms quantum
    assert(uthread_init(1000000) == 0);
    std::cout << "Main: init OK" << std::endl;

    // 2) Check that main’s tid is 0
    int mainTid = uthread_get_tid();
    std::cout << "Main: tid=" << mainTid << std::endl;
    assert(mainTid == 0);

    // 3) Spawn one child
    int childTid = uthread_spawn(child_thread);
    std::cout << "Main: spawn returned " << childTid << std::endl;
    assert(childTid == 1);

    // 4) When child finishes, we're back here
    std::cout << "Main: back after child termination" << std::endl;

    // 5) Inspect total quantums so far
    int totalQ = uthread_get_total_quantums();
    std::cout << "Main: total quantums=" << totalQ << std::endl;
    assert(totalQ >= 2);

    // 6) Clean up and exit
    uthread_terminate(0);
    return 0; // never reached
}
