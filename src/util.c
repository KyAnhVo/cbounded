#include "util.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

void *xcalloc(size_t size, const char *err_msg)
{
    void *ptr = calloc(1, size);
    if (!ptr) {
        fprintf(stderr, "%s\n", err_msg);
        exit(MEMORY_ERR);
    }
    return ptr;
}
