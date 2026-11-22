#include "symbolTable.h"

#include "util.h"
#include <linux/limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================== Hash function =======================================

/// hash then fold, FNV-1a.
uint64_t hash_str(const char *symbol)
{
    uint64_t h = 14695981039346656037ULL;
    const uint64_t fnv_prime = 1099511628211ULL;
    for (int i = 0; i < NAME_MAX && symbol[i] != '\0'; i++) {
        h = (h ^ (uint8_t)symbol[i]) * fnv_prime;
    }
    return (h ^ (h >> 32)) & (SYMBOL_TABLE_SIZE - 1);
}

// ============================= Structs ===================================

struct SymbolScope {
    uint8_t scope;
    char type[FILENAME_MAX + 1];
    int64_t lowerbound, upperbound;
    struct SymbolScope *next;
};

struct SymbolStack {
    struct SymbolScope *head;
    uint8_t max_scope;
    uint8_t len;
    char symbol[NAME_MAX + 1];
};

struct SymbolTableItem {
    char symbol[NAME_MAX + 1];
    struct SymbolStack stack;
    uint64_t hashed_val;
    bool used;
    bool deleted;
};

// =============================== Symbol table ==================================

struct SymbolTable {
    struct SymbolTableItem table[SYMBOL_TABLE_SIZE];
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

SymbolInfo symbolTable_lookup(SymbolTable *table, const char *symbol)
{
    SymbolInfo info = {
        .scope = 0, // Default, erroneous value
        .lowerbound = INT64_MIN, // default, minimum possible value
        .upperbound = INT64_MAX, // default, maximum possible value
        .type = { '\0' }, // Fill with 0 (for safety)
    };

    uint64_t h = hash_str(symbol);
    size_t start = h & (SYMBOL_TABLE_SIZE - 1);

    for (size_t step = 0; step < SYMBOL_TABLE_SIZE - 1; step++) {
        size_t i = (start + step) & (SYMBOL_TABLE_SIZE - 1);
        const struct SymbolTableItem *current = &(table->table[i]);

        // If the slot is not used (not taken), then it is either
        // deleted (used before) or not. If it is not deleted before,
        // we have never touched this or anything after (for this
        // particular hash value)
        if (!current->used) {
            if (current->deleted) {
                continue;
            }
            return info;
        }

        if (strcmp(current->symbol, symbol) == 0) {
            // Note that in this case, head must exist
            struct SymbolScope *head = current->stack.head;
            info.lowerbound = head->lowerbound;
            info.upperbound = head->upperbound;
            snprintf(info.type, sizeof(info.type), "%s", head->type);
            info.scope = head->scope;
            return info;
        }
    }

    return info; // All tombstones and/or items
}
