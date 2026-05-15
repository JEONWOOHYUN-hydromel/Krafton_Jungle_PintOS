/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "threads/mmu.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include <string.h>

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
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
static bool spt_alloc_page_with_initializer(struct supplemental_page_table *spt,
		enum vm_type type, void *upage, bool writable,vm_initializer *init, void *aux);
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
	struct supplemental_page_table *spt = &thread_current ()->spt;
	return spt_alloc_page_with_initializer(spt, type, upage, writable, init, aux);
}


static bool
spt_alloc_page_with_initializer (struct supplemental_page_table *spt, enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

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

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
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
		return NULL;
		//swap out to get the available page
		//frame = vm_evict_frame ();

	}

	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
	return false;
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
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
        free (frame);
        return false;
	}

	if (!swap_in (page, frame->kva)){
		pml4_clear_page (thread_current ()->pml4, page->va);
        frame->page = NULL;
    	page->frame = NULL;
        palloc_free_page (frame->kva);
        free (frame);
        return false;
	}

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

				if (!spt_alloc_page_with_initializer (dst, page->uninit.type,
						page->va, page->writable, page->uninit.init, new_aux)) {
					file_close (new_aux->file);
					free (new_aux);
					goto err;
				}

				break;
			}

			case VM_ANON: {
				if (page->frame == NULL || page->frame->kva == NULL)
					goto err;

				if (!spt_alloc_page_with_initializer(dst, VM_ANON, page->va, page->writable, NULL, NULL))
					goto err;

				struct page *new_page = spt_find_page (dst, page->va);
				if (new_page == NULL)
					goto err;

				if (!vm_do_claim_page (new_page))
					goto err;

				memcpy (new_page->frame->kva, page->frame->kva, PGSIZE);
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
