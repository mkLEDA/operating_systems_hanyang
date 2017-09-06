#include "types.h"
#include "mmu.h"
#include "stat.h"
#include "user.h"
#include "param.h"
#include "fs.h"
#include "proc.h"
#include "x86.h"

int thread_create(thread_t *thread, void* (*start_routine)(void*), void *arg) {
    void *new_stack = malloc(2048);
    int return_val = duplicate_process(new_stack);
    if(return_val == 0) {
        (*start_routine)(arg);
        free(new_stack);
        return 0;
    }
    *thread = return_val;
    if(return_val != -1) {
        return 0;
    }
    return 1;
}

int thread_join(thread_t thread, void **retval) {
    void* stack_ptr = 0;
    stack_ptr = (void*) thread_join_sys(thread, retval);
    if(!stack_ptr) {
        printf(1,"Could not retrieve stack pointer.\n");
        return 1;
    }
    //printf(1,"Freeing stack of %d \n", thread);
    free(stack_ptr);
    return 0;
}
