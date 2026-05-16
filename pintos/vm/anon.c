/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "lib/kernel/bitmap.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <debug.h>
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/malloc.h"

#define SECTORS_PER_PAGE (PGSIZE / DISK_SECTOR_SIZE)

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static struct bitmap *swap_table;
static struct lock swap_lock;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get (1, 1);
	if (swap_disk == NULL)
		PANIC ("No swap disk found");

	swap_table = bitmap_create (disk_size (swap_disk) / SECTORS_PER_PAGE);
	if (swap_table == NULL)
        PANIC ("Cannot create swap table");

	lock_init (&swap_lock);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;

	anon_page->slot_idx = BITMAP_ERROR;

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	uint8_t *addr = kva;
	if (anon_page->slot_idx == BITMAP_ERROR)
		return true;

	for (int i = 0; i < SECTORS_PER_PAGE; i++) {
		disk_read(swap_disk, anon_page->slot_idx * SECTORS_PER_PAGE + i,
				addr + i * DISK_SECTOR_SIZE);
	}

	lock_acquire (&swap_lock);
	bitmap_set (swap_table, anon_page->slot_idx, false);
	lock_release (&swap_lock);

	anon_page->slot_idx = BITMAP_ERROR;

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	struct frame *frame = page->frame;

	if (frame == NULL)
		return false;

	uint8_t *addr = frame->kva;

	lock_acquire (&swap_lock);
	size_t slot = bitmap_scan_and_flip (swap_table, 0, 1, false);
	lock_release (&swap_lock);

	if (slot == BITMAP_ERROR)
		return false;

	anon_page->slot_idx = slot;

	for (int i = 0; i < SECTORS_PER_PAGE; i++) {
		disk_write(swap_disk, slot * SECTORS_PER_PAGE + i,
				addr + i * DISK_SECTOR_SIZE);
	}

	pml4_clear_page(page->owner->pml4, page->va);

	page->frame = NULL;
	frame->page = NULL;

	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	if (page->frame != NULL) {	//frame clear
		pml4_clear_page(page->owner->pml4, page->va);
		palloc_free_page(page->frame->kva);
		frame_list_remove(page->frame);
		free(page->frame);
		page->frame = NULL;
	}
	else if (anon_page->slot_idx != BITMAP_ERROR) {	//swap_table clear
        lock_acquire(&swap_lock);
        bitmap_set(swap_table, anon_page->slot_idx, false);
        lock_release(&swap_lock);

        anon_page->slot_idx = BITMAP_ERROR;
    }
}
