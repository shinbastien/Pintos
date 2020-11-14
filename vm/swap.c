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
// #include "threads/init.c"
// #define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

struct lock swap_lock;
static struct block *swap_block;
static struct bitmap *swap_table;

block_sector_t swap_out (struct fte *victim) {
        // printf("swap out \n");
    // size_t a=   PGSIZE/BLOCK_SECTOR_SIZE;
    // printf("sector per page = %d\n",a);
    // printf("bitmap size = %d\n",block_size(swap_block)/a);
    // printf("swap 1 %d\n",thread_current()->tid);
    lock_acquire(&swap_lock);
        // printf("success\n");

        // printf("swap_out at %d %d\n",thread_current()->tid,victim->thread->tid);

    // printf("swap_out\n");
    //block_sector size가 512B. 페이지 하나를 저장하기 위해서 8개가 필요
    block_sector_t free_index = bitmap_scan_and_flip(swap_table, 0, 1, 0);
    if (free_index == BITMAP_ERROR) PANIC("NO free index in swap block");

        // printf("victim = %d\n",victim->frame);
        // printf("free = %d\n",free_index);

    for (int i = 0; i < 8; i++){ 
        block_write(swap_block, free_index*8 + i, (uint8_t *)(victim->frame)+ i * BLOCK_SECTOR_SIZE);
    }

    victim->spte->swap_location=free_index;
        // printf("success\n");

    pagedir_clear_page (victim->thread->pagedir,victim->spte->upage); 
    // dirty = pagedir_is_dirty (victim->thread->pagedir, victim->spte->upage);
        // printf("success\n");

    victim->spte->state=SWAP_DISK;
            // printf("success\n");

    lock_release(&swap_lock);
    // printf("swap r 1 %d\n",thread_current()->tid);

    return free_index;
}
void swap_in (block_sector_t swap_index,void* frame) {  
    // printf("swap 2 %d\n",thread_current()->tid);
    lock_acquire(&swap_lock);
    struct fte* fte = find_frame(frame);
            // printf("upage=%d\n",fte->spte->upage);
        // printf("swap_in start at %d %d\n",thread_current()->tid,fte->thread->tid);
        // ASSERT(fte != NULL);
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
            // printf("swap_in finish at %d %d\n",thread_current()->tid,fte->thread->tid);

    lock_release(&swap_lock);
        // printf("swap r 2 %d\n",thread_current()->tid);

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