#include "vm/page.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

unsigned 
hash_key_func (const struct hash_elem *e, void *aux){
    struct spte* spte = hash_entry(e,struct spte,elem);
    // return hash_bytes (&spte->upage, sizeof spte->upage);
    return hash_int(spte->upage);
}

bool 
hash_less_key_func (const struct hash_elem *a, const struct hash_elem *b, void *aux){
    struct spte* spte1 = hash_entry(a,struct spte,elem);
    struct spte* spte2 = hash_entry(b,struct spte,elem);
    if(spte1->upage<spte2->upage)
        return true;
    else
        return false;
}

void
init_spt(struct hash* spt){
  hash_init(spt, hash_key_func, hash_less_key_func, NULL); 
}

struct spte*
create_spte(void * upage, enum status state) {
    //언제 free해줘야하나
    struct spte *spte = (struct spte*)malloc(sizeof(struct spte));
    spte->upage=upage;
    spte->state=state;
    spte->swap_location = 0;
    return spte;
}

void
spte_update(struct spte* spte){
    spte->state = SWAP_DISK;

}
struct spte* 
get_spte(struct hash* spt,void* fault_addr){
    struct spte spte;
    spte.upage=pg_round_down(fault_addr);
    struct hash_elem* get = hash_find (spt,&spte.elem);

    if(get ==NULL){
        return NULL;
    }
    else{
        return hash_entry(get,struct spte, elem);
    }
}
    void spte_free(struct hash_elem *e, void *aux){
        free(hash_entry(e,struct spte, elem));
    }

void
destroy_spt(void){
    hash_destroy(&thread_current()->spt, spte_free); 
}

