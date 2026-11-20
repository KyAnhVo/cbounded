#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <linux/limits.h>
#include <stddef.h>
#include <stdio.h>
#include <util.h>

#define SYMBOL_TABLE_SIZE 2048

/// A symbol table is a hash table with key as a string (symbol) and
/// value as a stack of scopes
typedef struct SymbolTable SymbolTable;

/// Symbol info is given whenever a symbol table is created.
typedef struct {
    uint8_t scope;
    char type[FILENAME_MAX + 1];
    int64_t lowerbound, upperbound;
} SymbolInfo;

/// Delete a symbol table
void symbolTable_delete(SymbolTable *symbol_table);

/// Creates an empty symbol table
SymbolTable *symbolTable_create(void);

/// Get the stack of the current symbol, if available then return the ptr. Else
///
/// @param table The symbol table we are working with
/// @param symbol The symbol we want to delete// return null.
SymbolInfo symbolTable_lookup(SymbolTable *, char *);

/// Adds a symbol into the symbol table.
///
/// @param table The symbol table working with
/// @param symbol The symbol/name
/// @param scope The scope it is in
/// @param type The type of the symbol
/// @param lowerbound The lowerbound of the symbol's value
/// @param upperbound The upperbound of the symbol's value
SymbolInfo symbolTable_add_symbol(SymbolTable *, char *, uint8_t, char *,
                                  int64_t, int64_t);

/// Process a symbol table when enter scope
///
/// @param table The symbol table working with
void symbolTable_enter_scope(SymbolTable *);

/// Process a symbol table when exit scope
///
/// @param table The symbol table working with
void symbolTable_exit_scope(SymbolTable *);

#endif
