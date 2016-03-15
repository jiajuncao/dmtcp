#ifndef IBV_IBMALLOC_H
#define IBV_IBMALLOC_H

#include <stdio.h>

void *ibmalloc(size_t size);
void ibfree(void *ptr);

void ibmalloc_init();
void ibmalloc_fini();

#endif
