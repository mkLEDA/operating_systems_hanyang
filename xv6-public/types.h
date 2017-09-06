typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;
typedef struct Thread_t {
    int pid;
    void* stack_ptr;
    int finished;
    void *retval;
}thread_t_struct;
typedef int thread_t;
