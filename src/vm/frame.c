#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
// #include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/synch.h"

#include "userprog/process.h"

/*For project 3, VM */

void*
frame_alloc (enum palloc_flags flags,struct spte *spte){
  if ((flags & PAL_USER) == 0 )
    return NULL;
  void *frame = palloc_get_page(flags);
  if (!frame){
    lock_acquire(&frame_table_lock);
    struct fte *fte = find_victim();
    lock_release(&frame_table_lock);
    frame = fte->frame;
    block_sector_t sector= swap_out(fte);
    // swap_out된 frame의 spte, pte 업데이트
    // spte_update(fte->spte);
    frame_table_update(fte, spte, thread_current());

    } else {
  create_fte(frame,spte);
  }

  return frame;
}

struct fte* 
find_victim (void){ 
  struct fte* evict;
  struct list_elem* e;
  printf("3333333\n");

  for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)) {
  printf("34444443\n");

    evict = list_entry(e,struct fte, elem);
    if(evict !=NULL){
    if (pagedir_is_accessed (evict->thread->pagedir,evict->spte->upage)){
      pagedir_set_accessed (evict->thread->pagedir,evict->spte->upage,0);
    }
    else {
      break;
    }
    }
  }
  return evict;
}
void *
frame_table_update(struct fte* fte  ,struct spte* spte,struct thread* cur){
  fte->spte = spte;
  fte->thread = cur; 
}



void *
create_fte(void * frame,struct spte *spte){
  struct fte* fte=malloc(sizeof(struct fte));
  fte->thread = thread_current();
  fte->frame= frame;
  fte->spte=spte;
  list_push_back(&frame_table,&fte->elem);
}
void 
init_frame_table(void){
  list_init(&frame_table);
  lock_init(&frame_table_lock);
}
