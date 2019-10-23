#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "pstat.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

struct proc* proc0[NPROC];
struct proc* proc1[NPROC];
struct proc* proc2[NPROC];
struct proc* proc3[NPROC];

int
setpri(int PID, int pri) {
  if (pri > 3 || pri < 0 || PID < 1) {
    return -1;
  }
  struct proc* p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid == PID) {
      break;
    }
  }
  int prevPri = p->priority;
  int i;
  if(prevPri != pri && prevPri != -1){
    //Removes proc from old priority queue
    if (prevPri == 3) {
      for (i = 0; i < NPROC; ++i) {
        if (proc3[i] != 0 && proc3[i]->pid == PID)
          break;
      }
      for (i = i; i < NPROC - 1; i++) {
        if (proc3[i + 1] != 0) {
          proc3[i] = proc3[i + 1];
        }
        else {
          proc3[i] = 0;
          break;
        }
      }
    }
    else if (prevPri == 2) {
      for (i = 0; i < NPROC; ++i) {
        if (proc2[i] != 0 && proc2[i]->pid == PID)
          break;
      }
      for (i = i; i < NPROC - 1; i++) {
        if (proc2[i + 1] != 0) {
          proc2[i] = proc2[i + 1];
        }
        else {
          proc2[i] = 0;
          break;
        }
      }
    }
    else if (prevPri == 1) {
      for (i = 0; i < NPROC; ++i) {
        if (proc1[i] != 0 && proc1[i]->pid == PID)
          break;
      }
      for (i = i; i < NPROC - 1; i++) {
        if (proc1[i + 1] != 0) {
          proc1[i] = proc1[i + 1];
        }
        else {
          proc1[i] = 0;
          break;
        }
      }
    }
    else {
      for (i = 0; i < NPROC; ++i) {
        if (proc0[i] != 0 && proc0[i]->pid == PID)
          break;
      }
      for (i = i; i < NPROC - 1; i++) {
        if (proc0[i + 1] != 0) {
          proc0[i] = proc0[i + 1];
        }
        else {
          proc0[i] = 0;
          break;
        }
      }
    }
  }
  p->priority = pri;
  //places p in its new queue
  if (p->priority == 3) {
    for(i = 0; i < NPROC; i++) {
      if(proc3[i] == 0) {
        proc3[i] = p;
        proc3[i]->qtail[pri]++;
        break;
      }
    }
  } else if (p->priority == 2) {
     for(i = 0; i < NPROC; i++) {
      if(proc2[i] == 0) {
        proc2[i] = p;
        proc2[i]->qtail[pri]++;
        break;
      }
    }
  } else if (p->priority == 1) {
    for(i = 0; i < NPROC; i++) {
      if(proc1[i] == 0) {
        proc1[i] = p;
        proc1[i]->qtail[pri]++;
        break;
      }
    }
  } else {
    for(i = 0; i < NPROC; i++) {
      if(proc0[i] == 0) {
        proc0[i] = p;
        proc0[i]->qtail[pri]++;
        break;
      }
    }
  }
  return pri;
}

int
getpri(int PID) {
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->pid == PID) {
      break;
    }
  }
  if(PID < 1) {
    return -1;
  }
  return p->priority;
}

int getpinfo(struct pstat* pstate) {
  struct proc* p;
  int index = 0;
  
  if(pstate == 0) {
    return -1;
  }

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    pstate->inuse[index] = p->pid == 0 ? 0 : 1;
    pstate->pid[index] = p->pid;
    pstate->priority[index] = p->priority;
    pstate->state[index] = p->state;
    for (int i = 0; i < 4; ++i) {
      pstate->ticks[index][i] = p->ticks[i];
      pstate->qtail[index][i] = p->qtail[i];
    }
    index++;
  }
	return 0;
}

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

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

  return p;
}

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
  // add init to prio queues
  proc3[0] = p;
  // set priority to 3
  p->priority = 3;
  // increment qtail
  p->qtail[3]++;
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork2(int pri)
{
  if(pri < 0 || pri > 3) {
    return -1;
  }
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  np->priority = -1;
  //places p in its new queue
  setpri(np->pid, pri);
  release(&ptable.lock);

  return pid;
}

int
fork(void) {
  struct proc* processes1 = myproc();
  return fork2(processes1->priority);
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
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
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

void
removeFromQ() {
  while(proc3[0] != 0 && proc3[0]->state != RUNNABLE) { // need to remove
    for (int j = 0; j < NPROC; ++j) {
      if (proc3[j + 1] != 0) {	// shift left
        proc3[j] = proc3[j + 1];
      }
      else {
        proc3[j] = 0;
      }
    }
  }
  while(proc2[0] != 0 && proc2[0]->state != RUNNABLE) { // need to remove
    for (int j = 0; j < NPROC; ++j) {
      if (proc2[j + 1] != 0) {	// shift left
        proc2[j] = proc2[j + 1];
      }
      else {
        proc2[j] = 0;
      }
    }
  }
  while(proc1[0] != 0 && proc1[0]->state != RUNNABLE) { // need to remove
    for (int j = 0; j < NPROC; ++j) {
      if (proc1[j + 1] != 0) {	// shift left
        proc1[j] = proc1[j + 1];
      }
      else {
        proc1[j] = 0;
      }
    }
  }
  while(proc0[0] != 0 && proc0[0]->state != RUNNABLE) { // need to remove
    for (int j = 0; j < NPROC; ++j) {
      if (proc0[j + 1] != 0) {	// shift left
        proc0[j] = proc0[j + 1];
      }
      else {
        proc0[j] = 0;
      }
    }
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p = 0;
  struct cpu *c = mycpu();
  int i = 0;
  c->proc = 0;
  
  for(;;){
	  // Enable interrupts on this processor.
    sti();
    removeFromQ();  // cleanse our queue

//    for (i = 0; i < NPROC; ++i) {
      if (proc3[i] != 0 && proc3[i]->state == RUNNABLE) {
        p = proc3[i];
//        break;
      }
//    }
    // look for next runnable process IF prio 3 is empty
    if (p == 0) {
//      for (i = 0; i < NPROC; ++i) {
        if (proc2[i] != 0 && proc2[i]->state == RUNNABLE) {
          p = proc2[i];
//          break;
        }
      }
//    }
    if (p == 0) {
//      for (i = 0; i < NPROC; ++i) {
        if (proc1[i] != 0 && proc1[i]->state == RUNNABLE) {
          p = proc1[i];
    //      break;
        }
      }
 //   }
    if (p == 0) {
 //     for (i = 0; i < NPROC; ++i) {
        if (proc0[i] != 0 && proc0[i]->state == RUNNABLE) {
          p = proc0[i];
    //      break;
        }
      }
 //   }
    // idle :(
    if (p == 0) {
      continue;
    }
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);	// LOOOOOOCK
    
    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;

    swtch(&(c->scheduler), p->context);
    switchkvm();

    p->timeSlice++;
    p->ticks[p->priority]++;
    
    // REMOVE BADDIES from queue
    if (p->state != RUNNABLE) {
      // cprintf("%d\n", p->pid);
      if (p->priority == 3) {
        for (int j = i; j < NPROC; ++j) {
          if (proc3[j + 1] != 0) {	// shift left
            proc3[j] = proc3[j + 1];
          }
          else {
            proc3[j] = 0;
            break;
          }
        }
      } else if (p->priority == 2) {
        for (int j = i; j < NPROC; ++j) {
          if (proc2[j + 1] != 0) {	// shift left
            proc2[j] = proc2[j + 1];
          }
          else {
            proc2[j] = 0;
            break;
          }
        }
      } else if (p->priority == 1) {
        for (int j = i; j < NPROC; ++j) {
          if (proc1[j + 1] != 0) {	// shift left
            proc1[j] = proc1[j + 1];
          }
          else {
            proc1[j] = 0;
            break;
          }
        }
      } else {
        for (int j = i; j < NPROC; ++j) {
          if (proc0[j + 1] != 0) {	// shift left
            proc0[j] = proc0[j + 1];
          }
          else {
            proc0[j] = 0;
            break;
          }
        }
      }
      // we're done with this tick cuz this process was a baddie
      p = 0;
      c->proc = 0;
      i = 0;
      release(&ptable.lock);
      continue;
    }
	
    // increment timeslice and total ticks at prio level
  //	cprintf("\npid: %d\n prio: %d\n total ticks: %d\n qtail: %d\n", p->pid, p->priority, p->ticks[p->priority], p->qtail[p->priority]);
    // IF DONE USING CPU
    if (p->timeSlice == (4*(3-(p->priority)) +8)) {
      // reset timeslice
      p->timeSlice = 0;
      // move to back of prio queue
      if (p->priority == 3) {
        for (i = i; i < NPROC; ++i) {
          if (proc3[i + 1] != 0) {	// shift left
            proc3[i] = proc3[i + 1];
          }
          else {
            // insert the good-gone-bad boi
            proc3[i] = p;
            break;
          }
        }
      }
      else if (p->priority == 2) {
        for (i = i; i < NPROC; ++i) {
          if (proc2[i + 1] != 0) {	// shift left
            proc2[i] = proc2[i + 1];
          }
          else {
            proc2[i] = p;
            break;
          }
        }
      }
      else if (p->priority == 1) {
        for (i = i; i < NPROC; ++i) {
          if (proc1[i + 1] != 0) {	// shift left
            proc1[i] = proc1[i + 1];
          }
          else {
            proc1[i] = p;
            break;
          }
        }
      }
      else {
        for (i = i; i < NPROC; ++i) {
          if (proc0[i + 1] != 0) {	// shift left
            proc0[i] = proc0[i + 1];
          }
          else {
            proc0[i] = p;
            break;
          }
        }
      }
      // increment qtail
      p->qtail[p->priority]++;
    }

    // Process is done running for now.
    // It should have changed its p->state before coming back.
	  p = 0;
    c->proc = 0;
    i = 0;
    release(&ptable.lock);	// UNLOOOOOOOOOOCK

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
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
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
  struct proc *p = myproc();
  
  if(p == 0)
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
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
	  if (p->state == SLEEPING && p->chan == chan) {
		  p->state = RUNNABLE;
		  //places p in its new queue
		  setpri(p->pid, p->priority);
      p->timeSlice = 0; // renew that bad boi's timeslice
	  }
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
