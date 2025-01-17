#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "hash.h"
static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
    char *fn_copy2;
  tid_t tid;
  struct thread* cur;
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  /* for files real name(ex)echo) */
  fn_copy2 = palloc_get_page (0);

  if (fn_copy == NULL){
        // printf("error1\n");

    return TID_ERROR;
  }

  strlcpy (fn_copy, file_name, PGSIZE);
  strlcpy (fn_copy2, file_name, PGSIZE);


  char *next_ptr;
  char * realname;
  realname = strtok_r(fn_copy2," ", &next_ptr);
  if (filesys_open(realname)==NULL){
    // printf("error2\n");
    return -1;
  }
  /* Create a new thread to execute FILE_NAME. */
  cur=thread_current();

  tid = thread_create (realname, PRI_DEFAULT, start_process, fn_copy);
  // free(realname);
  sema_down(&cur->load_lock);

  if (tid == TID_ERROR){
    palloc_free_page (fn_copy); 
   }

  palloc_free_page (fn_copy2);
  struct thread* child_thread;
  struct list_elem* e;
  if(!list_empty(&thread_current()->child_wait_list)){
  for (e = list_begin(&thread_current()->child_wait_list); e != list_end(&thread_current()->child_wait_list); e = list_next(e)) {
      child_thread = list_entry(e, struct thread, child_list_elem);
      if (child_thread->exit == 1) {
        // printf("load fail\n");
        return process_wait(tid);
      }
    }
  }
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  // printf("start_process %d\n",thread_current()->tid);
  init_spt(&(thread_current()->spt));

  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  char *next_ptr;
  int argc = 0;
  char** argv[30];
  char * token;
  token = strtok_r(file_name," ", &next_ptr);
  argv[0] = token;
  while(token) 
  { 
    argc++;

    token = strtok_r(NULL, " ", &next_ptr); 
    argv[argc]=token;
  }

  success = load (argv[0], &if_.eip, &if_.esp);

  if(success){
    parse_stack(argv, argc, &if_.esp);
    palloc_free_page (file_name);

    sema_up(&(thread_current()->parent)->load_lock);

  }
  else {
    // list_remove(&(thread_current()->child_list_elem));
    printf("load_failed\n");
    thread_current()->exit = 1;
    /* If load failed, quit. */
    palloc_free_page (file_name);
        sema_up(&(thread_current()->parent)->load_lock);

    exit(-1);
    }
  
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  //function return 스택에 쌓아놓고 새로운 function call을 함
  
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
  struct thread *cur = thread_current();
  struct list wait_list = cur -> child_wait_list;
  struct list_elem* e;
  struct thread* child=NULL;

  if (child_tid!=NULL){
    if(!list_empty(&cur -> child_wait_list)){
      for (e = list_begin (&wait_list); e != list_end (&wait_list);e = list_next (e))
      {
        if((child=list_entry(e, struct thread, child_list_elem))->tid == child_tid){
          sema_down(&thread_current()->wait_lock);
          list_remove(e);
          int child_exit_status= child->child_status;

          palloc_free_page (child);
          return child_exit_status;
        }
      }
    }
  }
  return -1;   
}

struct file 
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  struct thread* parent = thread_current()->parent;
  uint32_t *pd;
  // struct list wait_list = (cur->parent)->child_wait_list;
  for(int i=0;i<128;i++){
    struct file* file_now = cur->fd_table[i];
    // if ((file_now->deny_write) == true)
    if(file_now!=NULL){
      file_close(file_now);
    }
  }

  destroy_spt(thread_current());

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
      sema_up(&parent->wait_lock);

}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;
  // printf("load %d\n",thread_current()->tid);

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  // file_deny_write(file);

  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp)){
    goto done;
  }

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
    // file_allow_write(file);
  file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);
  file_seek (file, ofs);

  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      // if (page_read_bytes == PGSIZE)
      //   printf("hi9 \n");
      // else if (page_zero_bytes == PGSIZE)
      //   printf("hi10 \n");
      // else
      //   printf("hi11 \n");
      /* Get a page of memory. */
      struct spte* spte = create_spte(upage,MEMORY);
      if(hash_insert (&(thread_current()->spt), &(spte->elem))!=NULL){
          printf("fadsdsfasddfsa\n");
      }
      spte->writable=writable;
      uint8_t *kpage = frame_alloc (PAL_USER,spte);

      // if (kpage == NULL)
      if (kpage == NULL)
        return false;

      /* Load this page. */
      // if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          // palloc_free_page (kpage);
          free_frame (kpage);

          return false; 
        }
      // memset (kpage + page_read_bytes, 0, page_zero_bytes);
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          free_frame (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

      // printf("setupstack = %d",pg_round_down(PHYS_BASE-PGSIZE));
      struct spte* spte = create_spte((uint8_t *)PHYS_BASE-PGSIZE,MEMORY);
      spte -> writable = true;
      // printf("hi 건우 \n");
      if(hash_insert (&(thread_current()->spt), &(spte->elem))!=NULL){
          // printf("fadsdsfasddfsa\n");
      }
      kpage = frame_alloc ((PAL_USER|PAL_ZERO),spte);

  if (kpage != NULL) 
    {
      success = install_page ((uint8_t *)PHYS_BASE-PGSIZE, kpage, true);
      if (success) {
        *esp =(uint8_t *) PHYS_BASE;
        // printf("esp = %d\n",*esp);
        thread_current()->esp =*esp;
        // spte->upage = *esp;
      }
      else{
        printf("here\n");
        free_frame(kpage);
      }
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
void
parse_stack(char** argv,int argc,void** esp){
  int len=0;
  for(int i =argc-1;0<=i;i--){
    *esp=*esp-(strlen(argv[i])+1);
    strlcpy(*esp,argv[i],strlen(argv[i])+1);    
    len+=strlen(argv[i])+1;
    argv[i]=*esp;
  }                                 
  /* insert the arguments in the stack the data is the char */
  //word-align 
  for(int i =0;i<4-len%4;i++){

    *esp=*esp-1;
    **(uint8_t**) esp=0;
  }                                 /* we match the stack pointer to be fit with the multiple of 4 */
  
  *esp=*esp-4;
  **(int**)esp=0;                   /* dummy argv[] for stack filled with 0 */

  for(int i =argc-1;0<=i;i--){

    *esp=*esp-4;
    **(int**)esp=argv[i];    
  }                                 /* insert the location address of the arguments in the stack */

  *esp=*esp-4;
  **(int**)esp=*esp+4;              /* the starting address of argv, the address of argv[0] */

  *esp=*esp-4;
  **(int**)esp=argc;                /* insert argc, the number of arguments */
  
  *esp=*esp-4;
  **(int**)esp=0;                   /* becomes the return address of the stack */
  thread_current()->esp = *esp;
}

// /*For project 3, VM */
// struct lock frame_table_lock;
// struct list frame_table;
// struct fte {
//   void *frame;
//   // struct sup_page_entry *spte;
//   struct thread *thread;
//   struct list_elem elem;
// };

// void*
// frame_alloc (enum palloc_flags flags){
//   if ((flags & PAL_USER) == 0 )
//     return NULL;
//   void *frame = palloc_get_page(flags);
//   // if (!frame){
//   //   lock_acquire(&frame_table_lock);
//   //   struct fte *fte = find_victim();
//   //   lock_release(&frame_table_lock);
//   //   frame = fte->frame;
//   //   // swap_out(frame);
//   //   // // swap_out된 frame의 spte, pte 업데이트
//   //   // spte_update(fte->spte)
//   //   // frame_table_update(fte, spte, thread_current());

//   //   } else {
//   create_fte(frame);
//   // }

//   return frame;
// }
// // struct fte* 
// // find_victim (){ 
// //   struct list_elem *evict_elem = list_pop_back(&frame_table);
// //   list_push_front(&frame_table, evict_elem)
// //   return list_entry (evict_elem, struct fte, elem);
// // }
// // void *
// // frame_table_update(struct fte fte  ,struct spte spte,struct thread* cur){

// // }
// void *
// create_fte(void * frame){
//   struct fte* fte=&frame;
//   fte->thread = thread_current();
//   fte->frame= frame;
//   list_push_back(&frame_table,&fte->elem);
  
// }
// void 
// init_frame_table(void){
//   list_init(&frame_table);
// }
bool
stack_growth(void* fault_addr){
  // printf("stack  growth\n");
  // void* upage = pg_round_down(fault_addr);
  bool success = false;
  // struct spte* spte= create_spte(upage,MEMORY);
  struct spte* spte= create_spte(thread_current()->esp-PGSIZE,MEMORY);
  // printf("spte = %d",pg_round_down(thread_current()->esp-PGSIZE));
  spte->writable = true;
  // printf("fault_addr:%d\n",thread_current()->esp);
        if(hash_insert (&(thread_current()->spt), &(spte->elem))!=NULL){
      };
  void* frame = frame_alloc((PAL_USER|PAL_ZERO),spte);

  // success = install_page (upage, frame, true);
  // success = install_page (((uint8_t *) PHYS_BASE) - 2*PGSIZE, frame, true);
  success = install_page (pg_round_down(thread_current()->esp-PGSIZE), frame, true);
  if(success){
    thread_current()->esp = pg_round_down(thread_current()->esp-PGSIZE);
    // if (fault_addr > thread_current()->esp)
    //   printf("hi \n");
      // printf("esp = %d\n",thread_current()->esp);

    // thread_current()->esp = upage;
  }
  else{
    printf("stack error\n");
    free_frame(frame);
  }
    // printf("esp = %d",thread_current()->esp );

  return success;
}