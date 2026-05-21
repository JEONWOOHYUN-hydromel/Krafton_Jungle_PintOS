/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include <debug.h>
#include <string.h>
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
static bool lazy_load_file (struct page *page, void *aux);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	if (page == NULL || kva == NULL)
		return false;

	struct file_page *file_page = &page->file;
	bool succ = false;

	lock_acquire (filesys_lock);
	if (file_read_at(file_page->file, kva,
		file_page->read_bytes, file_page->ofs) == (off_t) file_page->read_bytes){
		memset((uint8_t *) kva + file_page->read_bytes, 0, file_page->zero_bytes);
		succ = true;
	}
	lock_release (filesys_lock);

	return succ;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page = &page->file;
	struct frame *frame = page->frame;

	if (frame == NULL)
		return false;

	if (pml4_is_dirty (page->owner->pml4, page->va)) {
		lock_acquire (filesys_lock);
		file_write_at (file_page->file, frame->kva, file_page->read_bytes, file_page->ofs);
		lock_release (filesys_lock);
	}

	pml4_clear_page(page->owner->pml4, page->va);

	page->frame = NULL;
	frame->page = NULL;

	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;

	if (pml4_is_dirty (page->owner->pml4, page->va) && page->frame != NULL) {
		lock_acquire (filesys_lock);
		file_write_at (file_page->file, page->frame->kva, file_page->read_bytes, file_page->ofs);
		lock_release (filesys_lock);
	}

	lock_acquire (filesys_lock);
	file_close (file_page->file);
	lock_release (filesys_lock);

	if (page->frame != NULL) {	//frame clear
		pml4_clear_page(page->owner->pml4, page->va);
		palloc_free_page(page->frame->kva);
		frame_list_remove(page->frame);
		free(page->frame);
		page->frame = NULL;
	}
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	/* The mmap address is not an existing user buffer.  Check only whether
	 * this range is a valid, page-aligned user virtual range to map into. */
	if (addr == NULL || pg_ofs (addr) != 0 || length == 0 || file == NULL)
		return NULL;

	/* Pintos mmap offsets are page based; unaligned offsets must fail. */
	if (offset < 0 || offset % PGSIZE != 0)
		return NULL;

	/* Reject wraparound and mappings that would cross into kernel space. */
	uint8_t *start = addr;
	uint8_t *last = start + length - 1;
	if (last < start || !is_user_vaddr (start) || !is_user_vaddr (last))
		return NULL;
		
	lock_acquire (filesys_lock);
	off_t flength = file_length(file);
	lock_release (filesys_lock);

	if (flength <= 0 || offset >= flength)
  		return NULL;

	/* The whole mmap range must be empty in the supplemental page table. */
	uint8_t *end = pg_round_up (start + length);
	for (uint8_t *va = start; va < end; va += PGSIZE) {
		if (spt_find_page (&thread_current()->spt, va) != NULL)
			return NULL;
	}

	/* Map the requested length, but read only the bytes that exist in file.
	 * Remaining bytes in the last pages are zero-filled by lazy_load_file. */
	off_t ofs = offset;
	size_t file_bytes = flength - offset;
	uint32_t read_bytes = file_bytes < length ? file_bytes : length;
	for (uint8_t *va = start; va < end; va += PGSIZE) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct file_aux *aux = malloc (sizeof (struct file_aux));
		if (aux == NULL)
			goto err;

		lock_acquire (filesys_lock);
		*aux = (struct file_aux){
			.file = file_reopen(file),
			.ofs = ofs,
			.read_bytes = page_read_bytes,
			.zero_bytes = page_zero_bytes,
			.mmap_addr = start,
		};
		lock_release (filesys_lock);

		if (aux->file == NULL) {
    		free (aux);
			goto err;
		}

		if (!vm_alloc_page_with_initializer (VM_FILE, va,
					writable, lazy_load_file, aux)) {
			file_close(aux->file);
			free(aux);
			goto err;
		}

		read_bytes -= page_read_bytes;
		ofs += page_read_bytes;
	}

	return addr;
	
	err:
		return NULL;

}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct supplemental_page_table *spt = &thread_current()->spt;

	for (uint8_t *va = addr; ; va += PGSIZE) {
		struct page *page = spt_find_page (spt, va);

		if (page == NULL)
			return;

		void *mmap_addr;
		switch (VM_TYPE (page->operations->type)) {
			case VM_UNINIT:
				if (page->uninit.type != VM_FILE || page->uninit.aux == NULL)
					return;
				
				mmap_addr = ((struct file_aux *) page->uninit.aux)->mmap_addr;
				break;

			case VM_FILE:
				mmap_addr = page->file.mmap_addr;
				break;

			default:
				return;
		}

		if (mmap_addr != addr)
            return;

        spt_remove_page (spt, page);
	}
}

static bool 
lazy_load_file (struct page *page, void *aux) {
	if (page == NULL || aux == NULL)
		return false;
	
	struct file_aux *file_aux = aux;
	struct file_page *file_page = &page->file;

	file_page->file = file_aux->file;
	file_page->ofs = file_aux->ofs;
	file_page->read_bytes = file_aux->read_bytes;
	file_page->zero_bytes = file_aux->zero_bytes;
	file_page->mmap_addr = file_aux->mmap_addr;

	free (file_aux);

	return file_backed_swap_in (page, page->frame->kva);
}
