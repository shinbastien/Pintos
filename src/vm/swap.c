#include "swap.h"
#include <bitmap.h>
#include "threads/synch.h"
#include "threads/interrupt.h"
#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
// #define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

struct lock swap_lock;
static struct block *swap_block;
static struct bitmap *swap_table;

block_sector_t swap_out (struct fte *victim) {
        // printf("swap out \n");

    
    lock_acquire(&swap_lock);
        // printf("swap_out\n");

    // printf("swap_out\n");
    //block_sector size가 512B. 페이지 하나를 저장하기 위해서 8개가 필요
    block_sector_t free_index = bitmap_scan_and_flip(swap_table, 0, 1, 0);
    if (free_index == BITMAP_ERROR) PANIC("NO free index in swap block");


    for (int i = 0; i < 8; i++){ 
        block_write(swap_block, free_index*8 + i, (victim->frame)+ i * BLOCK_SECTOR_SIZE);
    }
    victim->spte->swap_location=free_index;

    pagedir_clear_page (victim->thread->pagedir,victim->spte->upage); 
    // dirty = pagedir_is_dirty (victim->thread->pagedir, victim->spte->upage);

    victim->spte->state=SWAP_DISK;
    lock_release(&swap_lock);
    return free_index;
}
void swap_in (block_sector_t swap_index,void* frame) {  
    lock_acquire(&swap_lock);

    struct fte* fte = find_frame(frame);
            // printf("upage=%d\n",fte->spte->upage);

        ASSERT(fte->frame !=NULL);
    // int swap_index = fte->spte->swap_location;
    // printf("%d\n",fte->thread->tid);
    if (bitmap_test(swap_table, swap_index) == 0) {
        // printf("hi 정윤51 \n");
        ASSERT ("Trying to swap in a free block.");
    }
    bitmap_flip(swap_table, swap_index); //bitmap
    // printf("hi 정윤50 \n");
    for (int i = 0; i < 8; i++) {
        // bitmap_flip(swap_table, swap_index+i);
        block_read(swap_block, swap_index*8 + i, (uint8_t *)(fte->frame) + i * BLOCK_SECTOR_SIZE);
    }
    // printf("hi 정윤50 \n");
    fte->spte->swap_location=-1;
    fte->spte->state=MEMORY;
    lock_release(&swap_lock);
}
void
init_swap_table(void){
    swap_block = block_get_role(BLOCK_SWAP);
    swap_table =bitmap_create(block_size(swap_block)/8);
    lock_init (&swap_lock);
}
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
bool 
load_from_swap(struct spte* spte){
    bool result=true;
    void* frame=frame_alloc(PAL_USER,spte);
                //    printf("fdsadfs\n");
                //    printf("fdsadfs\n");
        // printf("upage=%d\n",spte->upage);

    if(!install_page (spte->upage, frame, spte->writable)){
        
        result = false;
        free_frame(frame);
        printf("load_from_swap error!\n");
        return result;
    }
                //    printf("fdsadfs\n");
    swap_in(spte->swap_location,frame);
                //    printf("fdsadfs\n");
    return result;
}