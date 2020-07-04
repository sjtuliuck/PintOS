#include <stdio.h>
#include "page.h"
#include "frame.h"
#include "swap.h"
#include "../threads/thread.h"
#include "../userprog/pagedir.h"
#include "../userprog/syscall.h"
#include "../lib/stddef.h"
#include "../threads/malloc.h"
#include "../lib/debug.h"
#include "../threads/vaddr.h"

#define PAGE_PAL_FLAG			0
#define PAGE_INST_MARGIN		32
#define PAGE_STACK_SIZE			0x800000
#define PAGE_STACK_UNDERLINE	(PHYS_BASE - PAGE_STACK_SIZE)

bool page_hash_less(const struct hash_elem *lhs, const struct hash_elem *rhs, void *aux empty) {
	return hash_entry(lhs, struct page_table_elem, elem)->key < hash_entry(rhs, struct page_table_elem, elem)->key;
}					 


unsigned page_hash(const struct hash_elem *nelm, void* aux empty){
	struct page_table_elem *t = hash_entry(nelm, struct page_table_elem, elem);
	return hash_bytes(&(t->key), sizeof(t->key));
}

void page_destroy_frame_likes(struct hash_elem *nelm, void *aux empty) {
	struct page_table_elem *t = hash_entry(nelm, struct page_table_elem, elem);

	if(t->status == FRAME) {
	  pagedir_clear_page(thread_current()->pagedir, t->key);
		frame_free_frame(t->value);
	}
	else if(t->status == SWAP) {
		swap_free((index_t) t->value);
	}
	free(t);
}

static struct lock page_lock;

void page_lock_init() {
	lock_init(&page_lock);
}
page_table_t* page_create() {
	page_table_t *t = malloc(sizeof(page_table_t));

	if(t == NULL) 
		return NULL;
	else {
		if(page_init(t))
			return t;
		else {
			free(t);
			return NULL;
		}
	}
}
bool page_init(page_table_t *page_table) {
	return hash_init(page_table, page_hash, page_hash_less, NULL);
}

void page_destroy(page_table_t *page_table) {
	lock_acquire(&page_lock);
	hash_destroy(page_table, page_destroy_frame_likes);
	lock_release(&page_lock);
}

struct page_table_elem* page_find(page_table_t *page_table, void *upage) {
	struct hash_elem *nelm;
	struct page_table_elem tmp;

    ASSERT(page_table != NULL);
	tmp.key = upage;
	nelm = hash_find(page_table, &(tmp.elem));
	
	if(nelm != NULL) {
		return hash_entry(nelm, struct page_table_elem, elem);
	}
	else {
		return NULL;
	}
}

bool page_page_fault_handler(const void *vaddr, bool to_write, void *esp) {
	struct thread *nthread = thread_current();
	uint32_t *pagedir = nthread->pagedir;
	page_table_t *page_table = nthread->page_table;
	void *upage = pg_round_down(vaddr);
	lock_acquire(&page_lock);
	
	struct page_table_elem *t = page_find(page_table, upage);
	void *dest = NULL;
	
	ASSERT(is_user_vaddr(vaddr) || (!(t != NULL && t->status == FRAME)));

	if(to_write == true && t != NULL && t->writable == false)
	  return false;
		
	if(upage >= PAGE_STACK_UNDERLINE) {
		if(vaddr >= esp - PAGE_INST_MARGIN) {
			if(t == NULL) {
				dest = frame_get_frame(PAGE_PAL_FLAG, upage);
				if(dest == NULL) {
					frame_set_pinned_false(dest);
					lock_release(&page_lock);
					return false;
				}
				else {
					t = malloc(sizeof(*t));
					t->key = upage;
					t->value = dest;
					t->status = FRAME;
					t->writable = true;
					t->origin = NULL;
					hash_insert(page_table, &t->elem);
				}
			}
			else {
				switch(t->status) {
					case SWAP:
						dest = frame_get_frame(PAGE_PAL_FLAG, upage);
						if(dest == NULL) {
							frame_set_pinned_false(dest);
							lock_release(&page_lock);
							return false;
						}
						swap_load((index_t) t->value, dest);
						t->value = dest;
						t->status = FRAME;
						break;
					default:
						frame_set_pinned_false(dest);
						lock_release(&page_lock);
						return false;
				}
			}
		}
		else {
			frame_set_pinned_false(dest);
			lock_release(&page_lock);
			return false;		
		}
	}
	else {
		if(t == NULL) {
			frame_set_pinned_false(dest);
			lock_release(&page_lock);
			return false;		
		}
		else {
			if(t->status == SWAP){
				dest = frame_get_frame(PAGE_PAL_FLAG, upage);
				if(dest == NULL) {
					frame_set_pinned_false(dest);
					lock_release(&page_lock);
					return false;
				}
				swap_load((index_t)t->value, dest);
				t->value = dest;
				t->status = FRAME;
			} else {
				if(t->status == FILE){
					dest = frame_get_frame(PAGE_PAL_FLAG, upage);
					if(dest == NULL) {
						frame_set_pinned_false(dest);
						lock_release(&page_lock);
						return false;
					}
					mmap_read_file(t->value, upage, dest);
					t->value = dest;
					t->status = FRAME;
				} else {
					frame_set_pinned_false(dest);
					lock_release(&page_lock);
					return false;
				}
			}
		}
	}
	
	frame_set_pinned_false(dest);
	lock_release(&page_lock);
	ASSERT(pagedir_set_page(pagedir, t->key, t->value, t->writable));
	return true;
}

bool page_set_frame(void *upage, void *kpage, bool wb) {
	struct thread *nthread = thread_current();
	page_table_t *page_table = nthread->page_table;
	uint32_t *pagedir = nthread->pagedir;

	bool success = true;
	lock_acquire(&page_lock);
	
	struct page_table_elem *t = page_find(page_table, upage);
	if(t == NULL) {
		t = malloc(sizeof(*t));
		t->key = upage;
		t->value = kpage;
		t->status = FRAME;
		t->origin = NULL;
		t->writable = wb;
		hash_insert(page_table, &t->elem);
	}
	else {
		success = false;
	}
	
	lock_release(&page_lock);
	
	if(success) {
		ASSERT(pagedir_set_page(pagedir, t->key, t->value, t->writable));
	}
	return success;
}

bool page_available_upage(page_table_t *page_table, void *upage) {
	return upage < PAGE_STACK_UNDERLINE && page_find(page_table, upage) == NULL;
}

bool page_accessible_upage(page_table_t *page_table, void *upage) {
	return upage < PAGE_STACK_UNDERLINE && page_find(page_table, upage) != NULL;
}

bool page_install_file(page_table_t *page_table, struct mmap_handler *mh, void *key) {
	struct thread *nthread = thread_current();
	bool success = false;
	lock_acquire(&page_lock);
	if(page_available_upage(page_table, key)) {
		struct page_table_elem *nelm = malloc(sizeof(*nelm));
		nelm->key = key;
		nelm->value = nelm->origin = mh;
		nelm->status = FILE;
		nelm->writable = mh->writable;
		hash_insert(page_table, &nelm->elem);
		success = true;
	}
	lock_release(&page_lock);
	return success;
}

bool page_unmap(page_table_t *page_table, void *upage) {
	struct thread *nthread = thread_current();
	bool success = false;
	lock_acquire(&page_lock);
	
	if(page_accessible_upage(page_table, upage)) {
		struct page_table_elem *t = page_find(page_table, upage);
		if(t == NULL){
			ASSERT(false);
		}
		if((t->status) == FILE){
			hash_delete(page_table, &(t->elem));
			free(t);
			success = true;
		}
		if((t->status) == FRAME){
		    if(pagedir_is_dirty(nthread->pagedir, t->key)) {
		    	mmap_write_file(t->origin, t->key, t->value);
		    }
		    pagedir_clear_page(nthread->pagedir, t->key);
		    hash_delete(page_table, &(t->elem));
     		frame_free_frame(t->value);
		    free(t);
			success = true;
		}
	}
	lock_release(&page_lock);
	return success;
}

bool page_status_eviction(struct thread *nthread, void *upage, void *index, bool to_swap) {
	struct page_table_elem *t = page_find(nthread->page_table, upage);
    bool success = true;
    
    if(t == NULL){
	    puts("NULL");
	    return false;
	}
	
	if( t->status != FRAME){
		printf("%s\n", t->status == FILE ? "flie" : "swap");
		return false;
	}
    
	if(!to_swap) {
		ASSERT( t->origin != NULL );
		t->value = t->origin;
		t->status = FILE;
	}
	else {
		t->value = index;
		t->status = SWAP;
	}
	pagedir_clear_page(nthread->pagedir, upage);
	return true;
	
}





struct page_table_elem* page_find_with_lock(page_table_t *page_table, void *upage){
	lock_acquire(&page_lock);
	struct page_table_elem* tmp = page_find(page_table, upage);
	lock_release(&page_lock);
	return tmp;
}
