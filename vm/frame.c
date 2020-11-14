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
    // printf("1 \n");
    lock_acquire(&frame_table_lock);

  void *frame = palloc_get_page(flags);
    // printf("here 1? \n");
      lock_release(&frame_table_lock);

  if (!frame){
    // printf("here?");
    // printf("2 \n");
    lock_acquire(&frame_table_lock);
    struct fte *fte = find_victim();
        lock_release(&frame_table_lock);

    frame = fte->frame;
    swap_out(fte);

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
    // evict = list_entry(list_pop_back(&frame_table),struct fte, elem);
    if (next_fte_elem == NULL){
      e = list_begin(&frame_table);
      
    }
    else{
      e = next_fte_elem;

    }
  while (true) {
    
    evict = list_entry(e,struct fte, elem);
    if (!pagedir_is_accessed (evict->thread->pagedir,evict->spte->upage)){
      // if(pagedir_is_dirty(evict->thread->pagedir,evict->spte->upage)){
      //   printf("dirty\n");
      // }
      if (list_next(e) == list_tail(&frame_table)){
        next_fte_elem = list_begin(&frame_table);

      }
      else{
        
        next_fte_elem = list_next(e);
                  // printf("tid=%d\n",list_entry(next_fte_elem,struct fte, elem)->thread->tid);



      }
      break;

    }
    else {

            pagedir_set_accessed (evict->thread->pagedir,evict->spte->upage,0);
      continue;
    }
    if (list_next(e) == list_tail(&frame_table)){
      e = list_begin(&frame_table);
    }
    else{
      e = list_next(e);
    }
    
  }

  return evict;
}

struct fte* 
find_victim2 (void){ 
  struct fte* evict;
  struct list_elem* e;
  for(e=list_begin(&frame_table);e!=list_end(&frame_table);e=list_next(e)){
    evict = list_entry(e,struct fte, elem);
    if (pagedir_is_accessed (evict->thread->pagedir,evict->spte->upage)){
      pagedir_set_accessed(evict->thread->pagedir, evict->spte->upage, 0);
    }
    else
      break;
  }
  return evict;
}

void 
frame_table_update(struct fte* fte  ,struct spte* spte,struct thread* cur){
  fte->spte = spte;
  fte->thread = cur; 
}



struct fte*
create_fte(void * frame,struct spte *spte){
  struct fte* fte=malloc(sizeof(struct fte));
  fte->thread = thread_current();

  fte->frame= frame;
  fte->spte=spte;
  // printf("3 \n");
  lock_acquire(&frame_table_lock);

  list_push_back(&frame_table,&fte->elem);
  lock_release(&frame_table_lock);

  return fte;
}
void 
init_frame_table(void){
  list_init(&frame_table);
  lock_init(&frame_table_lock);
  // next_fte_elem = NULL;
}


void
free_frame(void* frame){
  printf("free\n");
  struct fte* free_frame;
  struct list_elem* e;
    // printf("4 \n");
    lock_acquire(&frame_table_lock);

  for(e=list_begin(&frame_table);e!=list_end(&frame_table);e=list_next(e)){
    if((free_frame=list_entry(e,struct fte, elem))->frame==frame){
      list_remove(e);
      hash_delete(&free_frame->thread->spt,&free_frame->spte->elem);
      free(free_frame->spte);
      free(free_frame);
        lock_release(&frame_table_lock);

      break;
    }
  }

  palloc_free_page(frame);
    lock_release(&frame_table_lock);

}
struct fte*
find_frame(void* frame){

  struct fte* fte;
  struct list_elem* e;

  for(e=list_begin(&frame_table);e!=list_end(&frame_table);e=list_next(e)){
    if((fte=list_entry(e,struct fte, elem))->frame==frame){
      return fte;
    }
  }
  // printf("NOframe!\n");
  return NULL;

}