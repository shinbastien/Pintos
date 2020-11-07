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

struct lock swap_lock;
static struct block *swap_block;
static struct bitmap *swap_table;

block_sector_t swap_out (struct fte *victim) {
    
    lock_acquire(&swap_lock);
    printf("swap_out\n");
    //block_sector size가 512B. 페이지 하나를 저장하기 위해서 8개가 필요
    block_sector_t free_index = bitmap_scan_and_flip(swap_table, 0, 8, 0); 

    if (free_index == BITMAP_ERROR) PANIC("NO free index in swap block");


    for (int i = 0; i < 8; i++){ 
        block_write(swap_block, free_index + i, (victim->frame)+ i * BLOCK_SECTOR_SIZE);
    }
    victim->spte->swap_location=free_index;
    pagedir_clear_page (victim->thread->pagedir,victim->spte->upage); 
    victim->spte->state=SWAP_DISK;
    lock_release(&swap_lock);
    return free_index;
}
void swap_in (struct fte* fte) {  
    lock_acquire(&swap_lock);
        printf("swap_in\n");

    int swap_index = fte->spte->swap_location;
    if (bitmap_test(swap_table, swap_index) == 0)
        ASSERT ("Trying to swap in a free block.");
   
    bitmap_flip(swap_table, swap_index); //bitmap

    for (int i = 0; i < 8; i++) {
        block_read(swap_block, swap_index + i, (uint8_t *)(fte->frame) + i * BLOCK_SECTOR_SIZE);
    }

    fte->spte->swap_location=NULL;
    fte->spte->state=MEMORY;

    lock_release(&swap_lock);
}
void
init_swap_table(void){
    swap_block = block_get_role(BLOCK_SWAP);
    swap_table =bitmap_create(block_size(swap_block));
    lock_init (&swap_lock);
      bitmap_set_all (swap_table, false);

}
bool 
load_from_swap(struct spte* spte){
    bool result=true;
    void* frame=frame_alloc(PAL_USER,spte);
            void* create_fte(frame,spte);
    swap_in((struct fte*)&frame);

    return result;
}