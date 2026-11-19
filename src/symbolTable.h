#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <linux/limits.h>
#include <stddef.h>
#include <util.h>

struct SymbolTableInput {
    char symbol[NAME_MAX + 1];
    void *stack;
    bool is_consumed;
};

/// A symbol table is
/// a hash table with key as a string (symbol) and
/// value as a stack of scopes
typedef struct {
    struct SymbolTableInput table[2048];
    size_t len;
} SymbolTable;

/// Create an empty symbol table,
SymbolTable *create_symbol_table(void);

#endif
