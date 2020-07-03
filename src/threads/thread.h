#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "synch.h"
#include "fixed_point.h"
#include "filesys/off_t.h"
#include "filesys/directory.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

typedef int mapid_t;

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

struct thread;
struct child_message {
    struct thread *tchild;              
    tid_t tid;                          
    bool exited;                        
    bool terminated;                    
    bool load_failed;                   
    int return_value;                   
    struct semaphore *sema_finished;  
    struct semaphore *sema_started;    
    struct list_elem elem;           
    struct list_elem allelem;        
};

struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */

   /* Added */
    int64_t ticks_blocked;              /* Record the time the thread has been blocked. */

    int base_priority;                  

    struct list locks;                  /* Locks that the thread is holding. */
    struct lock *lock_waiting;          /* The lock that the thread is waiting for. */

    int nice;                          
    fixed_t recent_cpu;                 
    /* Added */

    //
    int return_value; 

    struct list child_list;           
    struct semaphore sema_finished;    
    struct semaphore sema_started;     
    bool grandpa_died;             
    struct child_message *message_to_grandpa;

#ifdef USERPROG
    /* Owned by userprog/process.c. */
   uint32_t *pagedir;                  /* Page directory. */

   struct file* exec_file;
#endif
#ifdef VM
	struct hash* page_table;
   void *esp;
    
   struct list mmap_file_list;
   mapid_t next_mapid;
#endif
#ifdef FILESYS
    struct dir *current_dir;
#endif
};

struct file_handle{
    int fd;
    struct file* opened_file;
    struct thread* owned_thread;
    struct list_elem elem;
#ifdef FILESYS
    struct dir* opened_dir;
#endif
};

struct mmap_handler{
    mapid_t mapid; 
    struct file* mmap_file; 
    void* mmap_addr; 
    int num_page; 
    int last_page_size; 
    struct list_elem elem;
    bool writable;
    bool is_segment; 
    bool is_static_data;
    int num_page_with_segment; 
    off_t file_ofs;
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

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

/* Added */
void blocked_thread_check(struct thread *t, void *aux UNUSED);
bool thread_cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
void thread_hold_the_lock(struct lock *);
void thread_remove_lock(struct lock *);
void thread_donate_priority(struct thread *);
void thread_update_priority(struct thread *);
void thread_mlfqs_increase_recent_cpu_by_one(void);
void thread_mlfqs_update_priority(struct thread *);
void thread_mlfqs_update_load_avg_and_recent_cpu(void);
struct child_message *thread_get_child_message(tid_t tid);
// void thread_exit_with_return_value(struct intr_frame *f, int return_value);
void thread_file_list_inster(struct file_handle* fh);
struct file_handle* syscall_get_file_handle(int fd);
#ifdef FILESYS
void set_main_thread_dir();
#endif
/* Added */

#endif /* threads/thread.h */
