#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <devices/block.h>
#include "threads/thread.h"

enum status {
  SWAP_DISK, MEMORY
};
struct spte {  
    struct hash_elem elem;
	
    enum status state; // SWAP_DISK, MEMORY
    void *upage; // virtual address of an user page
    bool writable; // check whether the frame is writable or not
    block_sector_t swap_location; //index for swap in/out
};

// void spte_update(struct spte* spte);
unsigned hash_key_func (const struct hash_elem *e, void *aux);
bool hash_less_key_func (const struct hash_elem *a, const struct hash_elem *b, void *aux);
void init_spt(struct hash* spt);
struct spte* get_spte(struct hash* spt,void * fault_addr);
void destroy_spt(struct thread* thread);
void spte_free(struct hash_elem *e, void *aux);
#endif /* vm/page.h */