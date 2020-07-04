#ifndef MYPINTOS_SWAP_H
#define MYPINTOS_SWAP_H

#include "../devices/block.h"

typedef  block_sector_t index_t;

void swap_init();

index_t swap_store(void *kpage);

void swap_load(index_t index, void *kpage);

void swap_free(index_t index);


#endif 
