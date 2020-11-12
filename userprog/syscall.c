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

struct semaphore file_lock;

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  /* Initialize semaphore for file system synchronization */
  sema_init(&file_lock,1);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* Arguments are in stack of interrupt frame*/
  void *pointer = f->esp;
  // check_user_sp(pointer);
  // thread_current()->esp = pointer;
  switch(*(int*)(pointer)){
    case SYS_HALT:
      halt();
      break;                        /* Halt the operating system. */
    case SYS_EXIT:                   /* Terminate this process. */

      check_user_sp(pointer+4);
      exit(*(int*)(pointer+4));

      break;
    case SYS_EXEC:                   /* Start another process. */
  // sema_down(&file_lock);

      check_user_sp(pointer+4);
      f->eax=exec(*(int*)(pointer+4));
              // sema_up(&file_lock);

      break;
    case SYS_WAIT:                   /* Wait for a child process to die. */

      check_user_sp(pointer+4);
      f->eax=wait((tid_t)*(uint32_t *)(f->esp + 4));

      break;
    case SYS_CREATE:                 /* Create a file. */

      check_user_sp(pointer+8);
      // sema_down(&file_lock);
          // sema_down(&file_lock);

      f->eax=create(*(int*)(pointer+4),*(unsigned*)(pointer+8));
      // sema_up(&file_lock);
    // sema_up(&file_lock);

      break;
    case SYS_REMOVE:                 /* Delete a file. */

      check_user_sp(pointer+4);
      // sema_down(&file_lock);
      f->eax=remove(*(int*)(pointer+4));
      // sema_up(&file_lock);
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
      // sema_down(&file_lock);

      check_user_sp(pointer+4);
      close(*(int*)(pointer+4)); 
            // sema_up(&file_lock);

      break;                      /* Close a file. */
    default:
      exit(-1);
      break;
  }
  // Original code call thread_exit() every time
  // thread_exit ();
}
void halt (void){
  shutdown_power_off ();
}

void exit (int status){
  if(status>=0){
    printf ("%s: exit(%d)\n",thread_current()->name,status);
  }
  /* If status is less than 0, exit returns -1 */
  else{
    printf ("%s: exit(%d)\n",thread_current()->name,-1);
  }
  thread_current()->child_status=status;
  thread_exit ();
}

/* we use tid as pid */
tid_t exec (const char *cmd_line){

  
  return process_execute(cmd_line);
}

int wait (tid_t pid){

      return process_wait(pid);
}

bool create (const char *file, unsigned initial_size){
  /* Check the validity of file */

  if(file==NULL||*file==NULL){

    exit(-1);
  }
  else if(strlen(file)==0||strlen(file)>14){

    return 0;
  }
  /* If file is valid, create file with initial size */
  else{
    // printf("hi \n");
    // printf("create_success %d\n",thread_current()->tid);
    sema_down(&file_lock);
    bool success = filesys_create(file, initial_size);
    sema_up(&file_lock);
    return success;
  }
}

bool remove (const char *file){
  if(file==NULL||*file==NULL){
    exit(-1);
  }
  sema_down(&file_lock);
  bool success = filesys_remove(file);
  sema_up(&file_lock);
  return success;
}

int write (int fd, const void *buffer, unsigned size){

  /* check the validity of buffer pointer */
  check_user_sp(buffer);

  struct thread *cur = thread_current();

  /* If buffer is valid, lock write for file synchronization */ 
  sema_down(&file_lock);

  /* Fd 1 means standard output(ex)printf) */

  if(fd ==1){

    putbuf(buffer,size);
    sema_up(&file_lock);

    return size;
  }


  else{
    struct file* current_file =cur->fd_table[fd];

    /* If file is running process, deny write */
    if(current_file->deny_write){
      sema_up(&file_lock);
      return 0;
    }

    // printf("write! \n");
    int result= file_write (cur->fd_table[fd], buffer, size);
    sema_up(&file_lock);
    return result;
   }
}

int read (int fd, void *buffer, unsigned size){
  /* check the validity of buffer pointer */
  check_user_sp(buffer);

  uint8_t readbytes;
  struct thread *cur = thread_current();
  sema_down(&file_lock);
  
  /* Fd 1 means standard input(keyboard)  */
  if(fd==0){
    readbytes=input_getc();
    sema_up(&file_lock);

    return readbytes;
  }
  else{
    int result = file_read(cur->fd_table[fd], buffer, size);
    sema_up(&file_lock);

    return result;
  }
}

int open (const char *file){
  /* check the validity of file */
  if(file==NULL){

    exit(-1);
  }
  /* Length of file name should be 1 to 13 */
  else if(strlen(file)==0||strlen(file)>14){
    return -1;
  }
  /* lock for file synchronization */
  // sema_down(&file_lock);
  // printf("filename=%s\n",file);
  // printf("thread_current tid %d\n",thread_current()->tid);
  struct file* opened_file;
  struct thread *cur =thread_current();
  int fd;
  sema_down(&file_lock);
  opened_file = filesys_open (file);
        // printf("open %s\n",file);

  if(opened_file==NULL){
    sema_up(&file_lock);

    return -1;
  }

  /* if file is current process, deny write */
  if(!strcmp(file,cur->name))
    file_deny_write(opened_file);
  /* number of opened files should be less than 128 */
  /* check vacant room of fd_table */
  for(int i =2;i<130;i++){
    if(cur->fd_table[i]==NULL){
        cur->fd_table[i]=opened_file;
        fd=i;
        sema_up(&file_lock);

        return fd;
    }
  }

  sema_up(&file_lock);
  return -1;
}

int filesize (int fd){
  struct thread *cur =thread_current();
  return file_length(cur->fd_table[fd]);
}

void seek (int fd, unsigned position){
  struct thread *cur =thread_current();
  sema_down(&file_lock);
  file_seek (cur->fd_table[fd], position);
  sema_up(&file_lock);
}

unsigned tell (int fd){
  struct thread *cur =thread_current();
  file_tell (cur->fd_table[fd]);
}

void close (int fd){
  struct thread *cur =thread_current();
  struct file* file = cur->fd_table[fd];
  sema_down(&file_lock);
  if(file!=NULL){
        // printf("close_success %d\n",thread_current()->tid);

    file_allow_write(file);
    file_close(file);
    cur->fd_table[fd]=NULL;
    sema_up(&file_lock);
    return;
  }
  sema_up(&file_lock);
}


/* check the validity of user stack pointer */
static inline void check_user_sp(const void* sp){
  if(!is_user_vaddr(sp)||sp<0x08048000){
    exit(-1);
  }
}

