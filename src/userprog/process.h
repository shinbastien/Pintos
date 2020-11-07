#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/interrupt.h"
// #include "threads/palloc.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void parse_stack(char** argv,int argc,void** esp);
// void *frame_alloc(enum palloc_flags);
// void *create_fte(void *frame);
// void init_frame_table(void);
bool stack_growth(void* fault_addr, struct intr_frame *f);

#endif /* userprog/process.h */
