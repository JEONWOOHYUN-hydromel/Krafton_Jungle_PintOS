#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif


/* States in a thread's life cycle. */
/* 스레드 생명주기에서 사용할 수 있는 상태 값들. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	                    /* 현재 CPU에서 실행 중인 스레드. */
	THREAD_READY,       /* Not running but ready to run. */
	                    /* 실행 중은 아니지만 스케줄될 준비가 된 스레드. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	                    /* 어떤 이벤트를 기다리며 잠든 스레드. */
	THREAD_DYING        /* About to be destroyed. */
	                    /* 곧 제거될 스레드. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
/* 스레드 식별자 타입. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */
                                        /* tid 할당 실패를 나타내는 값. */

/* Thread priorities. */
/* PintOS 스레드 우선순위 범위. 값이 클수록 우선순위가 높다. */
#define PRI_MIN 0                       /* Lowest priority. */
                                        /* 가장 낮은 우선순위. */
#define PRI_DEFAULT 31                  /* Default priority. */
                                        /* 기본 우선순위. */
#define PRI_MAX 63                      /* Highest priority. */
                                        /* 가장 높은 우선순위. */

/* File descriptor. */
#define FD_MAX 128
#define FD_MIN 3

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* 각 스레드는 하나의 4 kB 페이지 안에 thread 구조체와 커널 스택을 함께 둔다.
 * 구조체가 너무 커지거나 커널 스택을 너무 많이 쓰면 magic 값이 깨져 오류를 발견할 수 있다. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
/* elem은 ready list와 semaphore waiters list에서 함께 쓰인다.
 * READY 스레드와 BLOCKED 스레드는 동시에 같은 리스트에 들어가지 않기 때문에 공유할 수 있다. */
struct thread {
	/* Owned by thread.c. */
	/* thread.c가 관리하는 기본 스레드 정보. */
	tid_t tid;                          /* Thread identifier. */
	                                    /* 스레드 식별자. */
	enum thread_status status;          /* Thread state. */
	                                    /* 현재 스레드 상태. */
	char name[16];                      /* Name (for debugging purposes). */
	                                    /* 디버깅용 스레드 이름. */

	int priority;                       /* Priority. */
	int64_t wakeup_tick;                   /* Tick to wake up sleeping thread. */
	                                      /* 잠든 스레드가 다시 깨어나야 하는 타이머 틱. */

	/* Shared between thread.c and synch.c. */
	/* ready list 또는 동기화 객체 대기 목록에 연결될 때 사용하는 공용 리스트 요소. */
	struct list_elem elem;              /* List element. */

	/* for sleeping list */
	/* timer_sleep()으로 잠든 스레드를 sleeping_list에 연결하기 위한 리스트 요소. */
	struct list_elem elem_sleep;              /* List element. */

	/* thread parent-child relationship */
	struct list children;
	struct child_status *my_status;
	int exit_status;

	

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	struct file *exec_file; /* The file that the thread is currently running. */
	uint64_t *pml4;                     /* Page map level 4 */
	struct file *fd_file[FD_MAX];				/* Open files. */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */
};


/* child status structure for tracking child thread information */
struct child_status {
    tid_t tid;
    int exit_status;
    bool waited;
    bool exited;
	bool is_orphan;
    struct semaphore wait_sema;
    struct list_elem elem;
};


/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
/* false이면 기본 round-robin 스케줄러를 사용하고, true이면 MLFQS를 사용한다. */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

/* Function pointers for list ordering. */
/* 리스트 정렬에 사용하는 비교 함수들. */
bool thread_wakeup_tick_less (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
bool thread_priority_greater (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

void thread_block (void);
void thread_unblock (struct thread *);

void thread_add_to_sleeping_list (int64_t ticks);
void thread_wakeup (int64_t ticks);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

void set_thread_name (const char *name);

/* parent-child relationship */
struct child_status *create_child_status (struct thread *parent);

#endif /* threads/thread.h */
