#ifndef VM_FRAME_H
#define VM_FRAME_H
#include "threads/palloc.h"
#include "vm/page.h"
#include "threads/thread.h"
struct lock frame_table_lock;
static struct list frame_table;
static struct list_elem* next_fte_elem;
struct fte {
  void *frame;
  struct spte *spte;
  struct thread *thread;
  struct list_elem elem;
};

void *frame_alloc(enum palloc_flags,struct spte*);
struct fte *create_fte(void *frame,struct spte*);
void init_frame_table(void);
struct fte* find_victim (void);
struct fte* find_victim2 (void);
void frame_table_update(struct fte* fte  ,struct spte* spte,struct thread* cur);
void free_frame(void* frame);
struct fte* find_frame(void*frame);

#endif /* vm/frame.h */
