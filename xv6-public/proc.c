#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  struct proc *mlfq[3][NPROC]; //Double tab : three queues of NPROC cells
  int quantum[3]; // Time_quantum of each queue
  int first[3]; //The current location of the first element in each queue
  int mlfq_ticks; //The number of ticks the mlfq used
  struct proc *stride[NPROC]; //The stride list of processes
  int mlfq_share; //The cpu time share allocated to the mlfq (on percent basis)
  int current_least_pass; //Current least pass in the stride
  thread_t_struct thread[NPROC];
} ptable;


static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);


void init_MLFQ() {
    //Clean the mlfq
    int i = 0;
    for(; i < 3; i++) {
        int j = 0;
        for (; j < NPROC; j++) {
            ptable.mlfq[i][j] = 0;
        }
    }

    //First elements are at 0
    ptable.first[0] = 0;
    ptable.first[1] = 0;
    ptable.first[2] = 0;

    //Define time quantums for each queue and ticks = 0
    ptable.quantum[0] = 5;
    ptable.quantum[1] = 10;
    ptable.quantum[2] = 20;
    ptable.mlfq_ticks = 0;

    //Clean the stride list
    for(i = 0; i < NPROC; i++) {
        ptable.stride[i] = 0;
    }

    //The MLFQ use 100% of the cpu time at init
    ptable.mlfq_share = 100;

    //Least pass in stride is 0 (no process yet)
    ptable.current_least_pass = 0;
}

/*
 * Print the current processes in the MLFQ and their level
 */
void Print_MLFQ() {
    int i = 0;
    for(; i < 3; i++) {
        int  j = 0;
        for (; j < NPROC; j++) {
            if(ptable.mlfq[i][j] && ptable.mlfq[i][j]->state != UNUSED) {
                cprintf("Level %d : process id %d.\n", i, ptable.mlfq[i][j]->pid);
            }
        }
    }
}

/* 
 * Insert process in the MLFQ in the first
 * empty cell after the current "first" cell
 * @param int : level to insert in
 * @param pointer : pointer to the process to insert
*/
void Insert_MLFQ(int level, struct proc *p) {
    int i = 0;
    for(i = ptable.first[level]; i != (ptable.first[level]- 1)%NPROC; i = (i+1)%NPROC) {
        if(ptable.mlfq[level][i] == 0 || ptable.mlfq[level][i]->state == UNUSED) {
            ptable.mlfq[level][i] = p;
            p->tick_on_queue = 0;
            return;
        }
    }
    if(ptable.mlfq[level][i] == 0 || ptable.mlfq[level][i]->state == UNUSED) {
        ptable.mlfq[level][i] = p;
        p->tick_on_queue = 0;
    }
}

/*
 * Dequeue from the queue without deleting
 * Only returning the first element and 
 * increase the first variable of this queue
 * @param int : level to dequeue in
 * @return pointer : pointer to last first process
 */
struct proc *Dequeue_MLFQ(int level) {
    struct proc *p = ptable.mlfq[level][ptable.first[level]];
    ptable.first[level] = (ptable.first[level] + 1) %NPROC;
    return p;
}

/*
 * Delete from the MLFQ level the process given
 * useful in case of exiting (since the dequeue does not delete)
 * @param int : the level to delete in
 * @param pointer : the pointer to the process to delete
 */
void Delete_MLFQ(int level, struct proc *p) {
    if(p && p->mlfq_level == 3) {
        return;
    }
    int i = 0;
    for(; i < NPROC; i++) {
        if(ptable.mlfq[level][i] == p) {
            ptable.mlfq[level][i] = 0;
            p->tick_on_queue = 0;
            break;
        }
    }
}

/*
 * Insert the given process in the stride
 * @param pointer : the pointer to the process to insert
 */
void Insert_Stride(struct proc *p) {
    int i = 0;
    for(; i < NPROC; i++) {
        if(!ptable.stride[i] || ptable.stride[i]->state == UNUSED) {
            ptable.stride[i] = p;
            break;
        }
    }
}

/*
 * Delete process from stride
 * @param pointer : pointer to the process to delete
 */
void Delete_Stride(struct proc *p){
    int i = 0;
    for(; i < NPROC; i++) {
        if(ptable.stride[i] == p) {
            ptable.stride[i] = 0;
            ptable.mlfq_share += p->share;
            break;
        }
    }
}

// Print the processes in stride (debug)
void Print_Stride() {
    int i = 0;
    struct proc *p = 0;
    for(; i< NPROC; i++) {
        p = ptable.stride[i];
        if(p && p-> state != UNUSED) {
            cprintf("Stride : Process %d. \n", p->pid);
        }
    }
}

/*
 * Perform priority boost in all the queues of the MLFQ
 * Happens when mlfq_ticks is greater than 100
 */
void priority_boost() {
    int i = 0;
    for (; i < NPROC; i++) {
        if(ptable.mlfq[0][i]) {
            ptable.mlfq[0][i]->tick_on_queue = 0;
        }
    }
    for(i = 1; i < 3; i ++) {
        int j = 0;
        for (; j < NPROC; j++) {
            if(ptable.mlfq[i][j]) {
                ptable.mlfq[i][j]->tick_on_queue = 0;
                Insert_MLFQ(0, ptable.mlfq[i][j]);
                ptable.mlfq[i][j] = 0;
            }
        }
    }
    ptable.mlfq_ticks = 0;
}

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  acquire(&ptable.lock);
  //Init Mlfq at init
  init_MLFQ();
  int i = 0;
  for(i = 0; i < NPROC; i++) {
    ptable.thread[i].pid = -1;
  }
  release(&ptable.lock);
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  // Process is inserted in the highest priority queue
  Insert_MLFQ(0, p);

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }

  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
 
  // Process data on MLFQ/Stride are initialized at 0
  p->tick_on_queue = 0;
  p->mlfq_level = 0;
  p->stride = 0;
  p->pass = 0;
  p->thread = 0;
  p->child_threads = 0;
  return p;
}

void thread_exit(void *retval) {
    int i = 0;
    for(i = 0; i < NPROC; i++) {
         if(ptable.thread[i].pid == proc->pid) {
            acquire(&ptable.lock);
            ptable.thread[i].finished = 1;
            ptable.thread[i].retval = retval;
            //proc->parent->child_threads--;
            release(&ptable.lock);
         }
    }
    /*if(i == NPROC) {
        panic("Thread not found\n");
    }*/
    exit();
}

int sys_thread_exit(void) {
    void *retval;
    if(argint(0, (int*) &retval)< 0) {
        return -1;
    }
    thread_exit(retval);
    return 0;
}

int thread_join_sys(thread_t thread, void **retval) {
    int i;
    for(i = 0; i < NPROC; i++) {
        if(ptable.thread[i].pid == thread) {
            while(ptable.thread[i].finished == 0) {
                wait();
            }
            *retval = ptable.thread[i].retval;
            ptable.thread[i].pid = -1;
            return (int)ptable.thread[i].stack_ptr;
        }
    }
    return 0;
}

int sys_thread_join_sys(void) {
    thread_t thread;
    void **retval;
    if(argint(0, (int*) &thread) < 0 || argint(1, (int*) &retval) < 0) {
        return -1;
    }
    return thread_join_sys(thread, retval);
}

int duplicate_process(void *new_stack_ptr) {
  struct proc *np;
  if((np = allocproc()) == 0) {
    return -1;
  }
  int i = 0;
  np->pgdir = proc->pgdir;
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the thread
  np->tf->eax = 0;
  
  uint StackSize = *(uint *) proc->tf->ebp - proc->tf->esp;
  np->tf->esp = (uint) new_stack_ptr + 2048 - StackSize;
  uint TopSize = *(uint *) proc->tf->ebp -proc->tf->ebp;
  np->tf->ebp = (uint) new_stack_ptr + 2048 - TopSize;
  memmove((void*) (np->tf->esp), (const void *) (proc->tf->esp),StackSize);


  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  int pid = np->pid;
  np->thread = 1;
  acquire(&ptable.lock);
  proc->child_threads++;
  for(i = 0; i < NPROC; i++) {
    if(ptable.thread[i].pid == -1) {
        ptable.thread[i].pid = np->pid;
        ptable.thread[i].finished = 0;
        ptable.thread[i].retval = 0;
        ptable.thread[i].stack_ptr = new_stack_ptr;
        break;
    }
  }
  if(i == NPROC) {
    panic("No place for the new thread");
  }

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

int sys_duplicate_process(void) {
    void * new_stack_pointer;
    if(argint(0, (int*) &new_stack_pointer) <0) {
        return -1;        
    }
    return duplicate_process(new_stack_pointer);

}

//PAGEBREAK: 32i
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;

  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;

    //The process is deleted from the MLFQ (just allocated, first level)
    Delete_MLFQ(0, np);

    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  while(proc->child_threads > 0 ) {
    wait();
  }
  acquire(&ptable.lock);
  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;

  //If the process is in the MLFQ, delete in MLFQ else in stride
  if(proc->mlfq_level < 3) {
    Delete_MLFQ(p->mlfq_level, proc);
  }
  else {
    Delete_Stride(p);
    //cprintf("Old mlfq share : %d", ptable.mlfq_share);

    //cprintf("New mlfq share : %d", ptable.mlfq_share);
  }
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        if(!p->thread){
            freevm(p->pgdir);
        }
        p->pid = 0;
        if(p->thread) {
            p->parent->child_threads--;
        }
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->thread = 0;
        p->state = UNUSED;

        // If the process is in MLFQ, delete in MLFQ else in Stride
        if(p->mlfq_level < 3) {
            Delete_MLFQ(p->mlfq_level, p);
        }
        else {
            Delete_Stride(p); 
           
        }

        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

/*
 * Schedule for the stride
 * return the process with the lowest pass value
 * @return pointer : pointer to the process scheduled
 */
struct proc *stride_schedule(void) {
    struct proc *p = 0;
    int i = 0;
    int least_pass = -1 ;
    // For each cell in the stride
    for(; i < NPROC; i++) {
        // If the process exist and is RUNNABLE
        if(ptable.stride[i] && ptable.stride[i]->state == RUNNABLE) {
            // If its pass value is lower than our previous lowest, switch 
            if(least_pass == -1 || ptable.stride[i]->pass < least_pass) {
                p = ptable.stride[i];
                least_pass = p->pass;
            }
        }
    }
    return p;
}

/*
 * Schedule in the MLFQ
 * @return pointer : pointer to the process to schedule
 */
struct proc *mlfq_schedule(void) {
    struct proc *p = 0;
    int i = 0;
    // Search in each queue, beginning at the highest priority queue
    for (; i < 3; i++) {
        int j = 0;
        // Repeating the process NPROC times (number of cells in the queue)
        for(; j < NPROC; j++) {
            // We dequeue the first process
            p = Dequeue_MLFQ(i);

            //If the process exist and is runnable, schedule it
            if(p && p->state == RUNNABLE) {
                //cprintf("Process %d on queue %d, level %d \n", p->pid,i, p->mlfq_level);
                return p;
            }
        }
    }
    return p;
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers scontrol
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  int choosing = 0;
  int run_mlfq = 1; // 1 if choosing < ptable.mlfq_share, else stride running

  for(;;){
    // Enable interrupts on this processor.
    sti();
    
    //if current run_mlfq is lower than mlfq share of cpu_time, schedule in MLFQ
    run_mlfq = choosing < ptable.mlfq_share;

    acquire(&ptable.lock);

    // Schedule 
    if(run_mlfq) {
        p = mlfq_schedule();
    }
    else {
        p = stride_schedule();
    }

    release(&ptable.lock);
    
    // If scheduled in MLFQ and the process is runnable
    if(run_mlfq && p && p->state == RUNNABLE) {
        //Print_MLFQ();
        //cprintf("Running : %d\n ", p->pid);

        // Get the tick on the queue of the chosen process
        int i = p->tick_on_queue;

        // While the process ticks are lower than its time allotment on the queue
        for(; i <= ptable.quantum[p->mlfq_level]; i++) {
            choosing++;
            if(choosing > ptable.mlfq_share) {
                choosing = ptable.mlfq_share;
            }
            //cprintf("Running process %d, tick on queue %d .\n", p->pid, p->tick_on_queue);
            
            //If the process doesn't exist anymore or is not runnable, exit loop
            if(!p || p->state != RUNNABLE) {
                break;
            }
            acquire(&ptable.lock);

            //One more ticks on the MLFQ
            //ptable.mlfq_ticks ++;

            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
            proc = p;
            switchuvm(p);
            p->state = RUNNING;
            swtch(&cpu->scheduler, p->context);
            switchkvm();
            // Process is done running for now.
            // It should have changed its p->state before coming back.
            proc = 0;
            release(&ptable.lock);
        }
    }

    //If schedule on stride and the process exist
    if(!run_mlfq && p && p->state == RUNNABLE) {
         acquire(&ptable.lock);

         //the lowest pass and process pass are increased by the stride of the process
         ptable.current_least_pass += p->stride;
         p->pass += p->stride;

         proc = p;
         switchuvm(p);
         p->state = RUNNING;
         swtch(&cpu->scheduler, p->context);
         switchkvm();
         // Process is done running for now.
         // It should have changed its p->state before coming back.
         proc = 0;
         release(&ptable.lock);
    }

    //if the process spend its time allotment on the queue, move down
    if(run_mlfq &&p && p->tick_on_queue >= ptable.quantum[p->mlfq_level]) {
        //cprintf("Mlfq ticks : %d \n", ptable.mlfq_ticks);
        p->tick_on_queue = 0;
        if(p->mlfq_level < 2) {
            acquire(&ptable.lock);
            Delete_MLFQ(p->mlfq_level, p);
            p->mlfq_level ++;
            Insert_MLFQ(p->mlfq_level, p);
            release(&ptable.lock);
        }
    }

    //If the MLFQ used 100 ticks, priority boost
    if(run_mlfq && ptable.mlfq_ticks >= 100){
        acquire(&ptable.lock);
        priority_boost();
        release(&ptable.lock);
    }

    //Increase the choosing factor by one (modulo 100 percent)
    choosing = (choosing+1) %100;
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  //Process spend one more tick on queue (if queue)
  proc->tick_on_queue++;
  ptable.mlfq_ticks++;
  //cprintf("Tick on queue : %d process = %d\n", proc->tick_on_queue, proc->pid);
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

/*
 * System call
 * @return : current level of the process in the MLFQ
 */
int getlev(void) {
    return proc->mlfq_level;
}

/*
 * WRAPPER of the syscall getLev
 * @return : the level given by getLev
 */
int sys_getlev(void) {
    return getlev();
}

/*
 * System call
 * If the share is correct, switch process from MLFQ to stride 
 * @param int : share asked by the process
 * @return int : 0 if the allocation worked, -1 otherwise
 */
int set_cpu_share (int share) {
    // If the share asked is not greater than the 80% allowed for the stride
    if(ptable.mlfq_share - share >= 20) {
        ptable.mlfq_share -= share;
        int stride = 100000 / share;
        if (stride == 0) {
            stride++;
        }
        proc->stride = stride;
        proc->pass = ptable.current_least_pass;
        acquire(&ptable.lock);
        Delete_MLFQ(proc->mlfq_level, proc);
        proc->mlfq_level = 3;
        proc->share = share;
        Insert_Stride(proc);
        release(&ptable.lock);
        return 0;
    }
    return -1;
}

/*
 * WRAPPER for the syscall set_cpu_share
 * Get the arguments and call set_cpu_share
 * @return int : the result of the syscall
 */
int sys_set_cpu_share(void) {
        int share;
        // If no argument given, return -1
        if(argint(0, &share) < 0) 
            return -1;
        return set_cpu_share(share);
}

