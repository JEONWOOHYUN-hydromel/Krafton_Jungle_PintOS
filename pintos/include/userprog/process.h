#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include <stdint.h>
#include "filesys/file.h"
#include "filesys/off_t.h"

struct exec_info {
    char *file_name;
    struct child_status *status;
};

struct fork_info {
    struct intr_frame if_;
    struct thread *parent;
    struct child_status *child_status;
    struct semaphore fork_sema;
    bool is_fork_success;
};

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

#endif /* userprog/process.h */
