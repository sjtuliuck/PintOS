#include <lib/debug.h>
#include <threads/pte.h>
#include <threads/malloc.h>
#include "swap.h"
#include "../lib/kernel/hash.h"

const int PdB = PGSIZE / BLOCK_SECTOR_SIZE;


void alert(bool ntag){
	if(ntag){
		printf("fuck off");
		ASSERT(true);
	}
}
struct basic_swap{
    index_t nindex;
    struct list_elem list;
};

static struct list free_list;
struct block* swap_block;
index_t top = 0;

index_t get_free_swap_slot(){
  index_t res = (index_t)-1;
  if (list_empty(&free_list)){
    if (top + PdB >= block_size(swap_block))
    	continue;
	res = top;
	top += PdB;
  }
  else{
    struct basic_swap* entry = list_entry(list_front(&free_list), struct basic_swap, list);
    list_remove(&entry->list);
    res = entry->nindex;
    free(entry);
  }
  return res;
}

void swap_init(){
  alert(block_get_role(BLOCK_SWAP) != NULL);
  swap_block = block_get_role(BLOCK_SWAP);
  list_init(&free_list);
}


index_t swap_store(void *np){
  alert(is_kernel_vaddr(np));
  index_t nindex = get_free_swap_slot();
  if (nindex == (index_t)-1)
    return nindex;
  for (int i = 0; i < PdB; i++){
    block_write(swap_block, nindex + i, np + i * BLOCK_SECTOR_SIZE);
  }
  return nindex;
}

void swap_load(index_t nindex, void *np){
  alert(nindex != (index_t)-1);
  alert(is_kernel_vaddr(np));
  alert(nindex % PdB == 0);

  for (int i = 0; i < PdB; i++){
    block_read(swap_block, nindex + i, np + i * BLOCK_SECTOR_SIZE);
  }
  swap_free(nindex);
}

void swap_free(index_t nindex){
  alert(nindex % PdB == 0);

  if (top == nindex + PdB)
    top = nindex;
  else{
    struct basic_swap* entry = malloc(sizeof(struct basic_swap));
    entry->nindex = nindex;
    list_push_back(&free_list, &entry->list);
  }
}



