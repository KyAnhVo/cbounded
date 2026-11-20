#include "symbolTable.h"

#include "util.h"
#include <linux/limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/// hash then fold, FNV-1a.
uint64_t hash_str(char *symbol)
{
    uint64_t h = 14695981039346656037ULL;
    const uint64_t fnv_prime = 1099511628211ULL;
    for (int i = 0; i < NAME_MAX && symbol[i] != '\0'; i++) {
        h = (h ^ (uint8_t)symbol[i]) * fnv_prime;
    }
    return (h ^ (h >> 32)) & (SYMBOL_TABLE_SIZE - 1);
}

struct SymbolScope {
    uint8_t scope;
    char type[FILENAME_MAX + 1];
    int64_t lowerbound, upperbound;
    struct SymbolScope *next;
};

typedef struct {
    struct SymbolScope *head;
    uint8_t max_scope;
    uint8_t len;
} SymbolStack;

struct SymbolTableInput {
    char symbol[NAME_MAX + 1];
    SymbolStack stack;
    uint64_t hashed_val;
    bool used;
    bool deleted;
};

struct SymbolTable {
    SymbolStack table[SYMBOL_TABLE_SIZE];
    uint8_t size;
    uint8_t scope;
};

SymbolTable *symbolTable_create(void)
{
    SymbolTable *table = (SymbolTable *)xcalloc(
        sizeof(struct SymbolTable),
        "Memory issue, cannot allocate space for a new symbol table");
    table->scope = 1;
    return table;
}

void symbolTable_delete(SymbolTable *symbol_table)
{
    free(symbol_table);
}
