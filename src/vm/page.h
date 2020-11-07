#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <devices/block.h>

enum status {
  SWAP_DISK, MEMORY, EXEC_FILE
};


struct spte {  
    struct hash_elem elem;
	
    enum status state;  // SWAP_DISK, EXEC_FILE, MEMORY
    void *upage; // virtual address of an user page

    // // for lazy loading
    // struct file *file;  
    // size_t offset;	//읽어야 되는 file의 시작 위치
    // size_t read_bytes;  // 읽어 들여야 하는 byte 수 
    // size_t zero_bytes;  // 0으로 채워야 되는 byte 수
  
    // // swap_out된 경우, swap disk에서의 page의 위치
    block_sector_t swap_location;
};

void spte_update(struct spte* spte);
unsigned hash_key_func (const struct hash_elem *e, void *aux);
bool hash_less_key_func (const struct hash_elem *a, const struct hash_elem *b, void *aux);
void init_spt(struct hash* spt);
struct spte* get_spte(struct hash* spt,void * fault_addr);
void destroy_spt(void);
void spte_free(struct hash_elem *e, void *aux);
#endif /* vm/page.h */