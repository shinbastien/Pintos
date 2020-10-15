#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
typedef int32_t off_t;

struct file 
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };

// static struct lock process_lock;
  struct semaphore file_lock;

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  sema_init(&file_lock,1);

}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  void *pointer = f->esp;
  
  check_user_sp(pointer);
  switch(*(int*)(pointer)){
    case SYS_HALT:
      halt();
      break;                        /* Halt the operating system. */
    case SYS_EXIT:                   /* Terminate this process. */
      check_user_sp(pointer+4);

      exit(*(int*)(pointer+4));

      break;
    case SYS_EXEC:                   /* Start another process. */
      check_user_sp(pointer+4);

      f->eax=exec(*(int*)(pointer+4));
      break;
    case SYS_WAIT:                   /* Wait for a child process to die. */
      check_user_sp(pointer+4);
      f->eax=wait((tid_t)*(uint32_t *)(f->esp + 4));

      break;
    case SYS_CREATE:                 /* Create a file. */
      // check_user_sp(const void* sp
        check_user_sp(pointer+8);

      f->eax=create(*(int*)(pointer+4),*(unsigned*)(pointer+8));
      break;
    case SYS_REMOVE:                 /* Delete a file. */
      check_user_sp(pointer+4);

      f->eax=remove(*(int*)(pointer+4));
      break;
    case SYS_OPEN:                   /* Open a file. */
      check_user_sp(pointer+4);

      f->eax=open (*(int*)(pointer+4));
      break;
    case SYS_FILESIZE:               /* Obtain a file's size. */
      check_user_sp(pointer+4);

      f->eax=filesize(*(int*)(pointer+4));
      break;
    case SYS_READ:                   /* Read from a file. */
      check_user_sp(pointer+12);

      f->eax=read(*(int*)(pointer+4),*(void**)(pointer+8),*(unsigned*)(pointer+12));
      break;
    case SYS_WRITE:                  /* Write to a file. */
      check_user_sp(pointer+12);

      f->eax=write(*(int*)(pointer+4),*(void**)(pointer+8),*(unsigned*)(pointer+12));
      break;
    case SYS_SEEK:                   /* Change position in a file. */
      check_user_sp(pointer+8);

      seek(*(int*)(pointer+4),*(unsigned*)(pointer+8));
      break;
    case SYS_TELL:                   /* Report current position in a file. */
      check_user_sp(pointer+4);

      tell(*(int*)(pointer+4));
      break;
    case SYS_CLOSE:
      check_user_sp(pointer+4);

      close(*(int*)(pointer+4)); 
      break;                      /* Close a file. */
  }
  // thread_exit ();
}
void halt (void){
  shutdown_power_off ();
}

void exit (int status){
  if(status>=0){
    printf ("%s: exit(%d)\n",thread_current()->name,status);
  }
  else{
    printf ("%s: exit(%d)\n",thread_current()->name,-1);
  }
  thread_current()->child_status=status;
  thread_exit ();
  

}
tid_t exec (const char *cmd_line){
  return process_execute(cmd_line);
}
//   lock_acquire(&process_lock)
int wait (tid_t pid){
      return process_wait(pid);
}
bool create (const char *file, unsigned initial_size){
  // check_user_sp(*file);
  // printf("%s\n",*file);
  // printf("fadsdsafdsdf\n");
  if(file==NULL||*file==NULL){
    exit(-1);
  }
  
  else if(strlen(file)==0||strlen(file)>14){
    return 0;
  }
  else{
    return filesys_create(file, initial_size);
  }
}
bool remove (const char *file){
  return filesys_remove(file);
}
int write (int fd, const void *buffer, unsigned size){
    struct thread *cur = thread_current();
          sema_down(&file_lock);

  if(fd ==1){
    putbuf(buffer,size);
                  sema_up(&file_lock);

    return size;
  }
  else{
        struct file* current_file =cur->fd_table[fd];
    if(current_file->deny_write){
                    sema_up(&file_lock);

       return 0;
    }

      int result= file_write (cur->fd_table[fd], buffer, size);
              sema_up(&file_lock);
     return result;
   }
                 sema_up(&file_lock);

  return -1;
}
int read (int fd, void *buffer, unsigned size){
    check_user_sp(buffer);

  uint8_t readbytes;
  struct thread *cur = thread_current();
        sema_down(&file_lock);

  if(fd==0){
    readbytes=input_getc();
            sema_up(&file_lock);

    return readbytes;
  }
  else{
    // printf("dd");
    int result = file_read(cur->fd_table[fd], buffer, size);
    sema_up(&file_lock);
    return result;
  }
              sema_up(&file_lock);

  return -1;
}

int open (const char *file){
  if(file==NULL){

    exit(-1);
  }
  
  else if(strlen(file)==0||strlen(file)>14){
    return -1;
  }
  sema_down(&file_lock);

  struct file* opened_file;
  struct thread *cur =thread_current();
  // int fd= cur->next_fd;
  opened_file = filesys_open (file);
  if(opened_file==NULL){
      sema_up(&file_lock);

    return -1;
  }
  if(!strcmp(file,cur->name))
    file_deny_write(opened_file);
  int fd;
  for(int i =2;i<128;i++){
    if(cur->fd_table[i]==NULL){
        cur->fd_table[i]=opened_file;
        fd=i;
          sema_up(&file_lock);

        return fd;
    }

  }
  sema_up(&file_lock);

  // cur->fd_table[fd]=opened_file;
  // cur->next_fd++;

  return fd;
}
int filesize (int fd){
  struct thread *cur =thread_current();
  return file_length(cur->fd_table[fd]);
}
void seek (int fd, unsigned position){
  struct thread *cur =thread_current();
  file_seek (cur->fd_table[fd], position);
}
unsigned tell (int fd){
  struct thread *cur =thread_current();
  file_tell (cur->fd_table[fd]);
}
void close (int fd){
  struct thread *cur =thread_current();
  struct file* file = cur->fd_table[fd];

  if(file!=NULL){
    file_allow_write(file);
    file_close(file);
    cur->fd_table[fd]=NULL;
  }
}


static inline void check_user_sp(const void* sp){
  if(!is_user_vaddr(sp)||sp<0x08048000){
    exit(-1);
  }
  
}

// void bring_sig_args (void *esp, int *arg, int numbers) {
//   for (i=1; i<numbers)
// }

