#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include <string.h>
#include "threads/palloc.h"


void syscall_entry (void);
void syscall_handler (struct intr_frame *);

static void sys_halt (void);
static void sys_exit (uint64_t status);
static tid_t sys_fork (const char *thread_name, struct intr_frame *f);
static int sys_exec (const char *cmd_line);
static int sys_wait (tid_t child_tid);
static bool sys_create (const char *file, unsigned initial_size);
static bool sys_remove (const char *file);
static int sys_open (const char *file);
static int sys_filesize (int fd);
static int sys_read (uint64_t fd, uint64_t buffer, uint64_t size);
static int sys_write (uint64_t fd, uint64_t buffer, uint64_t size);
static void sys_seek (int fd, unsigned position);
static unsigned sys_tell (int fd);
static void sys_close (int fd);


/*user buffer test*/
static bool is_valid_user_buffer (const void *addr);
static void user_buffer_test_length (const void *buffer, unsigned size);
static void user_buffer_test_string (const void *buffer);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	uint64_t syscall_num = f->R.rax;
	uint64_t ret = -1; // default return value is -1, which indicates an error
	uint64_t arg[6] = {f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8, f->R.r9}; // syscall arguments

	switch (syscall_num) {
		case SYS_HALT:
			sys_halt();
			break;

		case SYS_EXIT:
			sys_exit(arg[0]); // arg[0] is the exit status
			break;

		case SYS_FORK:
			ret = sys_fork((const char *) arg[0], f);
			break;

		case SYS_EXEC:
			ret = sys_exec((const char *) arg[0]);
			break;

		case SYS_WAIT:
			ret = sys_wait((tid_t) arg[0]); // arg[0] is the child_tid
			break;
		
		case SYS_CREATE:
			ret = sys_create((const char *) arg[0], (unsigned) arg[1]);
			break;

		case SYS_REMOVE:
			ret = sys_remove((const char *) arg[0]);
			break;

		case SYS_OPEN:
			ret = sys_open((const char *) arg[0]);
			break;

		case SYS_FILESIZE:
			ret = sys_filesize((int) arg[0]);
			break;

		case SYS_READ:
			ret = sys_read(arg[0], arg[1], arg[2]);
			break;

		case SYS_WRITE:
			ret = sys_write(arg[0], arg[1], arg[2]); // arg[0] is fd, arg[1] is buffer, arg[2] is size
			break;

		case SYS_SEEK:
			sys_seek((int) arg[0], (unsigned) arg[1]);
			break;

		case SYS_TELL:
			ret = sys_tell((int) arg[0]);
			break;

		case SYS_CLOSE:
			sys_close((int) arg[0]);
			break;

		default:
			sys_exit (-1);
			 break;
	}

	f->R.rax = ret; // set return value

	// printf ("system call!\n");
	// thread_exit ();
}

static void
sys_halt (){
	printf("!! System Call - Halt\n");
	power_off();
}

static void
sys_exit (uint64_t status){
	struct thread *curr = thread_current();
	curr->exit_status = (int) status;
	thread_exit();
}

static tid_t 
sys_fork (const char *thread_name, struct intr_frame *f) {
	user_buffer_test_string(thread_name);
	return process_fork((const char *) thread_name, f);
}

static int 
sys_exec (const char *cmd_line){
	user_buffer_test_string(cmd_line);
	
	char *copy = palloc_get_page(0);    
	if (copy == NULL)
        return -1;
    strlcpy(copy, cmd_line, PGSIZE);

	int ret = process_exec(copy);
	if (ret == -1)
		sys_exit(-1);
	
    NOT_REACHED();
}

static int
sys_wait (tid_t child_tid){
	return process_wait(child_tid);
}

static bool 
sys_create (const char *file, unsigned initial_size){	
	user_buffer_test_string(file);
	if (filesys_create(file, initial_size)) {
		return true;
	} else {
		return false;
	}
}

static bool 
sys_remove (const char *file){
	user_buffer_test_string(file);
	return filesys_remove(file);
}

static int 
sys_open (const char *file){
	user_buffer_test_string(file);
	
	struct file *opened_file = filesys_open(file);
	if (opened_file == NULL) {
		return -1;
	}
	
	for (int fd = FD_MIN; fd < FD_MAX; fd++) {
		if (thread_current()->fd_file[fd] == NULL) {
			thread_current()->fd_file[fd] = opened_file;
			return fd;
		}
	}
	file_close(opened_file); // No available file descriptor
	return -1;
}

static int 
sys_filesize (int fd){
	if (fd < FD_MIN || fd >= FD_MAX || thread_current()->fd_file[fd] == NULL) {
		return -1;
	}

	return file_length(thread_current()->fd_file[fd]);
}

static int 
sys_read (uint64_t fd, uint64_t buffer, uint64_t size){
	user_buffer_test_length((const void *) buffer, (unsigned) size);

	if (fd == 0) { // read from console
		for (unsigned i = 0; i < size; i++) {
			((char *) buffer)[i] = input_getc();
		}
		return size;
	}

	if (fd >= FD_MAX || thread_current()->fd_file[fd] == NULL) {
		return -1;
	}
	else { // read from file
		struct file *file = thread_current()->fd_file[fd];
		if (file == NULL) {
			return -1;
		}
		return file_read(file, (void *) buffer, size);
	}

	return -1;
}

static int
sys_write (uint64_t fd, uint64_t buffer, uint64_t size){
	//buffer validation
	user_buffer_test_length((const void *) buffer, (unsigned) size);

	if (fd == 1) { // write to console
		putbuf((const char *) buffer, size);
		return size;
	}
	
	if (fd >= FD_MAX || thread_current()->fd_file[fd] == NULL) {
		return -1;
	}
	else { // write to file
		struct file *file = thread_current()->fd_file[fd];
		if (file == NULL) {
			return -1;
		}
		return file_write(file, (const void *) buffer, size);
	}
	
	return -1;
}

static void 
sys_seek (int fd, unsigned position) {
	if (fd < FD_MIN || fd >= FD_MAX || thread_current()->fd_file[fd] == NULL){
		return;
	}
	file_seek(thread_current()->fd_file[fd], position);
}

static unsigned 
sys_tell (int fd) {
	if (fd < FD_MIN || fd >= FD_MAX || thread_current()->fd_file[fd] == NULL) {
		return -1;
	}
	return file_tell(thread_current()->fd_file[fd]);
}

static void 
sys_close (int fd) {
	if (fd < FD_MIN || fd >= FD_MAX || thread_current()->fd_file[fd] == NULL) {
		return;
	}

	file_close(thread_current()->fd_file[fd]);
	thread_current()->fd_file[fd] = NULL;
}


/* user buffer validation */
static bool
is_valid_user_buffer (const void *addr) {
	if (addr == NULL || !is_user_vaddr(addr) || pml4_get_page(thread_current()->pml4, addr) == NULL) {
		return false;
	}
	return true;
}

static void
user_buffer_test_length (const void *buffer, unsigned size) {
	if (size == 0) {
		return;
	}

	if (buffer == NULL || !is_valid_user_buffer(buffer) || !is_valid_user_buffer((const void *) ((uint64_t) buffer + size - 1))) {
		sys_exit(-1);
	}

	for (uint64_t start = (uint64_t) pg_round_down((uint64_t) buffer); start < (uint64_t) buffer + size; start += PGSIZE) {
		if (!is_valid_user_buffer((const void *) start)) {
			sys_exit(-1);
		}
	}
}

static void 
user_buffer_test_string (const void *buffer) {
	if (buffer == NULL || !is_valid_user_buffer(buffer)) {
		sys_exit(-1);
	}

	for (uint64_t i = 0; i < PGSIZE ; i++) {
		if (!is_valid_user_buffer((const void *) ((uint64_t) buffer + i))) {
			sys_exit(-1);
		}

		const char *ptr = (const char *) buffer + i;
		if (*ptr == '\0') {
			return;
		}
	}

	sys_exit(-1);
}
