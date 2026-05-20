/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "threads/mmu.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include <string.h>
#include "threads/synch.h"
#include "lib/kernel/list.h"


static struct list frame_list;
static struct lock frame_list_lock;

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
	list_init (&frame_list);
	lock_init (&frame_list_lock);
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);
static uint64_t page_hash (const struct hash_elem *p_, void *aux UNUSED);
static bool page_less (const struct hash_elem *a_, const struct hash_elem *b_,
		void *aux UNUSED);
static void page_destroy (struct hash_elem *e, void *aux UNUSED);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) != NULL)
		goto err;

	/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
	struct page *page = malloc(sizeof *page);
	if (page == NULL)
		goto err;

	bool (*initializer) (struct page *, enum vm_type, void *);
	switch (VM_TYPE(type)) {
		case VM_ANON:
			initializer = anon_initializer;
			break;
		case VM_FILE:
			initializer = file_backed_initializer;
			break;
		default:
			free(page);
			goto err;
	}


	uninit_new (page, pg_round_down(upage), init, type, aux, initializer);
	page->writable = writable;
	page->owner = thread_current();

	/* TODO: Insert the page into the spt. */
	if (spt_insert_page (spt, page))
		return true;

	free(page);
	goto err;

err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	if (spt == NULL || va == NULL)
		return NULL;

	struct page *page = NULL;
	void *upage = pg_round_down (va);

	struct page p;
	p.va = upage;

	struct hash_elem *e = hash_find (&spt->pages, &p.hash_elem);
	if (e == NULL)
		return NULL;

	page = hash_entry (e, struct page, hash_elem);
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	if (spt == NULL || page == NULL)
		return false;

	int succ = false;
	if (!hash_insert (&spt->pages, &page->hash_elem))
		succ = true;

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete (&spt->pages, &page->hash_elem);
	vm_dealloc_page (page);
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
	lock_acquire(&frame_list_lock);

	size_t limit = list_size (&frame_list) * 2;
	struct list_elem *e = list_begin (&frame_list);
	for (size_t i = 0; i < limit; i++) {
		if (e == list_end (&frame_list))
			e = list_begin (&frame_list);

		struct frame *e_frame = (struct frame *) list_entry(e, struct frame, list_elem);
		e = list_next (e);

		if (e_frame->pinned || e_frame->page == NULL)
			continue;

		if (!pml4_is_accessed (e_frame->page->owner->pml4, e_frame->page->va)) {
			e_frame->pinned = true;
			victim = e_frame;
			break;
		}

		pml4_set_accessed(e_frame->page->owner->pml4, e_frame->page->va, false);
	}

	lock_release (&frame_list_lock);

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	/* TODO: swap out the victim and return the evicted frame. */
	struct frame *victim = vm_get_victim ();
	if (victim == NULL)
		return NULL;

	struct page *page = victim->page;
	if (!swap_out (page)) {
		victim->pinned = false;
		return NULL;
	}

	frame_list_remove(victim);
	victim->page = NULL;
	victim->pinned = true;

	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	frame = malloc (sizeof (struct frame));
	if (frame == NULL)
		return NULL;


	frame->kva = palloc_get_page (PAL_USER | PAL_ZERO);
	if (frame->kva == NULL){
		free (frame);
		frame = vm_evict_frame();
		if (frame == NULL)
			return NULL;
	}

	frame->page = NULL;
	frame->pinned = true;
	frame_list_insert(frame);

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static bool
vm_stack_growth (void *addr) {
	void *upage = pg_round_down (addr);

	if (addr == NULL || is_kernel_vaddr (addr))
		return false;

	if (!vm_alloc_page (VM_ANON | VM_MARKER_0, upage, true))
		return false;

	return vm_claim_page (upage);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
	return false;
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user, bool write, bool not_present) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if (addr == NULL || is_kernel_vaddr(addr))
		return false;

	if (!not_present)
		return false;

	if ((page = spt_find_page(spt, addr)) == NULL) {
		void *rsp = user ? (void *) f->rsp : thread_current ()->user_rsp;

		if (is_stack_growth_candidate (addr, rsp))
			if (vm_stack_growth (addr)) // stack growth!
				return true;	// successed!
			else
				return false;	// failed!
		else
			return false; 	// invaild user addr!
	}
	else {

		//READ ONLY!
		if (write && (page->writable == false))
				return false;

		//lazy load
		if (vm_do_claim_page (page))
			return true;	//claim page!
		else
			return false;	// failed!

	}

	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	if (va == NULL)
		return false;

	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL)
		return false;

	if (!vm_do_claim_page (page))
		return false;

	return true;
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	if (page == NULL)
		return false;

	struct frame *frame = vm_get_frame ();
	if (frame == NULL)
		return false;
	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if (!pml4_set_page (thread_current()->pml4, page->va, frame->kva, page->writable)){
		frame->page = NULL;
        page->frame = NULL;
        palloc_free_page (frame->kva);
		frame_list_remove (frame);
        free (frame);
        return false;
	}

	frame->pinned = true;

	if (!swap_in (page, frame->kva)){
		pml4_clear_page (thread_current ()->pml4, page->va);
        frame->page = NULL;
    	page->frame = NULL;
        palloc_free_page (frame->kva);
		frame_list_remove (frame);
        free (frame);
        return false;
	}

	frame->pinned = false;

	return true;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init (&spt->pages, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
	if (dst == NULL || src == NULL)
		return false;

	struct hash_iterator i;

	hash_first(&i, &src->pages);
	while (hash_next (&i) != NULL) {
		struct page *page = hash_entry (hash_cur (&i), struct page, hash_elem);

		switch (VM_TYPE (page->operations->type)) {
			case VM_UNINIT: {
				if (VM_TYPE (page->uninit.type) == VM_FILE)
					break;

				struct file_aux *old_aux = page->uninit.aux;
				struct file_aux *new_aux = malloc(sizeof *new_aux);
				if (new_aux == NULL)
					goto err;

				*new_aux = *old_aux;
				new_aux->file = file_reopen(old_aux->file);
				if (new_aux->file == NULL){
					free (new_aux);
					goto err;
				}

				if (!vm_alloc_page_with_initializer (page->uninit.type,
						page->va, page->writable, page->uninit.init, new_aux)) {
					file_close (new_aux->file);
					free (new_aux);
					goto err;
				}

				break;
			}

			case VM_ANON: {
				// is parent on mem?
				if (page->frame == NULL) {
					if (!vm_do_claim_page (page)) //swap in parent
						goto err;
				}

				if (!vm_alloc_page_with_initializer (VM_ANON, page->va, page->writable, NULL, NULL))
					goto err;

				struct page *new_page = spt_find_page (dst, page->va);
				if (new_page == NULL)
					goto err;

				if (!vm_do_claim_page (new_page))
					goto err;

				memcpy (new_page->frame->kva, page->frame->kva, PGSIZE);
				new_page->owner = thread_current();
				break;
			}

			case VM_FILE:
				break;

			default:
				goto err;
		}
	}

	return true;

	err:
		supplemental_page_table_kill(dst);
		return false;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	if (spt == NULL)
		return;

	hash_destroy(&spt->pages, page_destroy);
}

static uint64_t
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
	const struct page *p = hash_entry (p_, struct page, hash_elem);
	return hash_bytes (&p->va, sizeof (p->va));
}

static bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
		void *aux UNUSED) {
	const struct page *a = hash_entry (a_, struct page, hash_elem);
	const struct page *b = hash_entry (b_, struct page, hash_elem);

	return a->va < b->va;
}

static void
page_destroy (struct hash_elem *e, void *aux UNUSED) {
	struct page *page = hash_entry (e, struct page, hash_elem);
	vm_dealloc_page (page);
}

bool is_stack_growth_candidate (void *addr, void *rsp) {
	return rsp <= addr + 8
       && addr < USER_STACK
       && addr >= USER_STACK - (1 << 20);
}

void
frame_list_insert (struct frame *frame) {
	lock_acquire (&frame_list_lock);
	list_push_back(&frame_list, &frame->list_elem);
	lock_release (&frame_list_lock);
}

void
frame_list_remove (struct frame *frame) {
	lock_acquire (&frame_list_lock);
	list_remove(&frame->list_elem);
	lock_release (&frame_list_lock);
}
