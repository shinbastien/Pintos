/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      /* jy 새로 넣은 코드 */
      list_insert_ordered(&sema->waiters, &thread_current()-> elem, list_elem_compare,0);
      // printf(list_size(&sema->waiters));

      /*list_push_back (&sema->waiters, &thread_current ()->elem);*/
      thread_block ();
    }

  sema->value--;
  intr_set_level (old_level);
}

void 
lock_down(struct lock *lock){
  enum intr_level old_level;
  struct semaphore* sema = &lock->semaphore;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      /* jy 새로 넣은 코드 */
      list_insert_ordered(&sema->waiters, &thread_current()-> elem, list_elem_compare,0);
      lock->lock_max = lock_max(lock);
      thread_block ();
    }

  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters)) {
    // list_sort (&sema->waiters, list_elem_compare,0);
    thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem));
                                
  }
  sema->value++;
 
    thread_yield();

  intr_set_level (old_level);

}
void
lock_up (struct lock* lock) 
{

  enum intr_level old_level;
  struct semaphore* sema=&lock->semaphore;
  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters)) {
    // list_sort (&sema->waiters, list_elem_compare,0);
    thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem));
          lock->lock_max = lock_max(lock);

                                
  }
  sema->value++;
 
    thread_yield();

  intr_set_level (old_level);

}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{

  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));
  struct thread* tholder = lock->holder;
  int current_priority=thread_get_priority();
  struct thread* current_thread =thread_current();
  // if(tholder!=NULL){
  //   int p1=thread_get_priority();
  //   // if(tholder->past_priority !=NULL){
  //   //   tholder->past_priority = tholder->priority;
  //   // }
  //   if(p1>tholder->priority){
  //     tholder->priority = p1;
  //   }
  // }
  /*gw:도네이션 코드**************/ 


  if(tholder!=NULL){
    enum intr_level old_level;
    old_level = intr_disable();

    current_thread->lock_of_holder = lock;
    // if(list_empty(&tholder->multiple_donation))
      // list_push_back(&tholder->multiple_donation,&current_thread->elem);
    // else
      // list_insert_ordered(&tholder->multiple_donation,&current_thread->elem,list_elem_compare,0);
    priority_donate(current_thread);
    // refresh();
    intr_set_level(old_level);

  }
  
  /*gw:nest donation*/ 
  /*gw:multiple donation*/

  
  /****************************/

  lock_down (lock);

  thread_current()->lock_of_holder =NULL;
  lock->holder = thread_current ();
  list_insert(list_begin (&thread_current()->lock_list),&(lock->lock_elem));

    // if  

}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));
  struct thread* tholder = lock->holder;
  // int past_priority = tholder->past_priority;
  // if(past_priority!=NULL){
  //   tholder->priority= tholder->past_priority;
  // }
  // tholder->priority= tholder->past_priority;

  
  // if(!list_empty(&tholder->multiple_donation)){
  //   if(check_multiple(tholder)){
  //     int biggest_priority= list_entry(list_pop_front(&tholder->multiple_donation),struct thread, elem)->priority;
  //     if (tholder->priority<biggest_priority){
  //       tholder->priority=biggest_priority;
  //     }
  //   }
  //   else{
  //     tholder->priority=tholder->past_priority;
  //   }
  // }
  // else{
  lock->holder = NULL;

  list_remove(&(lock->lock_elem));

  if(lock_list_max (&tholder->lock_list)>(tholder->past_priority))
    tholder->priority=lock_list_max (&tholder->lock_list);
  else
    tholder->priority=tholder->past_priority;
  // }



  lock_up (lock);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  // list_push_back (&cond->waiters, &waiter.elem);
  /* gw */
  // list_insert_ordered (&cond->waiters, &waiter.elem,semaphore_elem_compare,0);
  /* jy */
  list_insert_ordered(&cond->waiters, &waiter.elem, compare_waiter_max_by_elem, 0);

  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) {
    list_sort(&cond->waiters, compare_waiter_max_by_elem, 0);
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
  }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
// bool semaphore_elem_compare(const struct list_elem *a, const struct list_elem *b, void *aux){
//   struct semaphore_elem *semaphorea = list_entry(a,struct semaphore_elem, elem);
//   struct semaphore_elem *semaphoreb = list_entry(b,struct semaphore_elem, elem);
//   struct list lista= (semaphorea->semaphore).waiters;
//   struct list listb= (semaphoreb->semaphore).waiters;
//   printf("dx");
//   // printf("what%d",list_size(&lista));
//   // printf("the%d",list_size(&listb));
  
//   int maxa=list_entry(list_max (&lista, list_elem_compare, 0),struct thread, elem)->priority;
//   int maxb=list_entry(list_max (&listb, list_elem_compare, 0),struct thread,elem)->priority;
//   bool result= maxa>maxb;
//   return result;
// }


int find_sema_max (struct semaphore *sema) {
  int max_priority = list_entry(list_max(&sema->waiters, list_elem_compare, 0),struct thread, elem)->priority;
  return max_priority;
}

int waiter_max (struct semaphore_elem*waiter) {
 return find_sema_max(&waiter->semaphore);
} 
bool compare_waiter_max(struct semphore_elem *waiter1, struct semaphore_elem *waiter2) {
  bool result = waiter_max(waiter1) > waiter_max(waiter2);
  return result;
}
bool compare_waiter_max_by_elem(const struct list_elem *a,const struct list_elem *b,void *aux) {
  return compare_waiter_max(list_entry(a, struct semaphore_elem, elem), list_entry(b, struct semaphore_elem, elem));
}




/*gw donation을 위한 함수들*/
bool check_holder(struct thread* cur){
  return ((cur->lock_of_holder!=NULL)&&(cur->priority>((cur->lock_of_holder)->holder)->priority));
}
// bool check_multiple(struct thread* holder){
//   struct list multiple_donation=  holder->multiple_donation;
//   struct list_elem * cnt = list_begin(&multiple_donation);
//   struct lock* first_lock= list_entry(cnt,struct thread,elem)->lock_of_holder;
//   while (cnt != list_tail(&multiple_donation))
//     {
//       cnt = list_next(cnt);
//       struct lock* check_lock=list_entry(cnt,struct thread,elem)->lock_of_holder;
//       if(check_lock!=first_lock){
//         return true;
//       }
//     }
//   return false;
// }

// bool semaphore_elem_compare(const struct list_elem *a, const struct list_elem *b, void *aux){
//   struct semaphore_elem *semaphorea = list_entry(a,struct semaphore_elem, elem);
void priority_donate(struct thread* cur){
  struct lock* acquired_lock=cur->lock_of_holder;
  if(acquired_lock !=NULL){
    struct thread* holder=acquired_lock ->holder;
    if(holder !=NULL){
      if(holder->priority<cur->priority){
        holder->priority = cur->priority;
        priority_donate(holder);
      }
    }
  }
}

int lock_max(struct lock *lock){
  return find_sema_max(&lock->semaphore);
}
bool lock_elem_compare (struct lock *lock_a, struct lock *lock_b){
  return lock_max(lock_a) < lock_max(lock_b);  
}

bool lock_list_elem_compare (const struct list_elem *a,const struct list_elem *b,void *aux) {
  return lock_elem_compare(list_entry(a, struct lock, lock_elem), list_entry(b, struct lock,lock_elem));
}

int lock_list_max (struct list *lock_list){
  return list_entry(list_max(lock_list, lock_list_elem_compare, 0),struct lock,lock_elem)->lock_max;
}