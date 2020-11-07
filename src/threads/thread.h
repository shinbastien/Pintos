#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include <threads/synch.h>
#include <hash.h>

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };
typedef int pid_t;

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;

#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */


    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    /*gw 스택의 위치를 찾을 때?*/
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    int past_priority;                  /* jy past_priority for donation */
   //  int init_priority;                  /* jy initial priority for returning to ready queue */
   // struct list lock_item;
   /*gw 도네이션을 위한 것들*/
   //  struct list multiple_donation;
    struct lock* lock_of_holder;
    struct list lock_list;

    struct list_elem allelem;           /* List element for all threads list. */
    /*jy 일단 이거 추가 */
    int64_t wakeup_ticks;
    
    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    struct file* fd_table [130];        /*jy fd table for each process */

   //  int next_fd;                        /* the next index of file */

   //For project 3-1 spt



    tid_t pid;
    struct semaphore wait_lock;
    struct semaphore load_lock;
    struct list child_wait_list;                /*jy The list of children of the parent */
    struct list_elem child_list_elem;           /*jy List element for child processes when inside parent's child_wait_list. */
    struct thread* parent;
    int child_status;
    int exit;
    void* esp;
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */

       struct hash spt;

  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);
void thread_wakeup(void);
void thread_sleep(int64_t);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);


bool list_elem_compare(const struct list_elem *a, const struct list_elem *b, void *aux);
bool list_elem_compare_reverse(const struct list_elem *a, const struct list_elem *b, void *aux);

bool semaphore_elem_compare(const struct list_elem *a, const struct list_elem *b, void *aux);
bool compare_waiter_max_by_elem(const struct list_elem *a, const struct list_elem *b, void *aux);
bool check_holder(struct thread* cur);
bool check_multiple(struct thread* holder);
void refresh(void);
void priority_donate(struct thread* cur);
int check_readylist_prioirty(void);
bool wakeup_ticks_compare(const struct list_elem *a, const struct list_elem *b, void *aux);
#endif /* threads/thread.h */
