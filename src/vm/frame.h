#ifndef MYPINTOS_FRAME_H
#define MYPINTOS_FRAME_H

#include "../lib/stdbool.h"
#include "../threads/palloc.h"

struct frame_item{
    void *frame;
    void *upage;
    struct thread* t;
    bool pinned;
    struct hash_elem hash_elem;
    struct list_elem list_elem;
};

void *frame_lookup(void *frame);

void  frame_init();

void* frame_get_frame(enum palloc_flags flag, void *upage);

void  frame_free_frame(void *frame);

bool  frame_get_pinned(void* frame);

bool frame_set_pinned_false(void* frame);


#endif 
