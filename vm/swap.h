#include "devices/block.h"
#include "vm/frame.h"
#include "vm/page.h"
block_sector_t swap_out (struct fte*victim_frame);
void swap_in (block_sector_t swap_location,void *frame);
void init_swap_block(void);
void init_swap_table(void);
bool load_from_swap(struct spte* spte);
