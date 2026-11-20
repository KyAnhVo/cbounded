#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stddef.h>

#define bool uint8_t
#define true (bool)1
#define false (bool)0

#define MAX_NESTED_SCOPE_COUNT 127

#define SUCCESS 0
#define MEMORY_ERR 1

/// Allocates a zero-out'd memory chunk of size `size`,
/// or if not possible then print the error message to stderr and exit
/// MEMORY_ERR status code.
///
/// @param size The size of memory wanting to be allocated
/// @param err_msg The message to print out if cannot allocate memory chunk
void *xcalloc(size_t size, const char *);

#endif
