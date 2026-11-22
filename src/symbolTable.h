#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <linux/limits.h>
#include <stddef.h>
#include <util.h>

#define SYMBOL_TABLE_SIZE 2048

/// A symbol table is a hash table with key as a string (symbol) and
/// value as a stack of scopes
typedef struct SymbolTable SymbolTable;

/// Symbol info is given whenever a symbol table is created.
typedef struct {
    uint8_t scope;
    char type[NAME_MAX + 1];
    int64_t lowerbound, upperbound;
} SymbolInfo;

/// Status of insertion operation
enum SymbolTableOpStatus {
    SymbolTableStatus_OK,
    SymbolTableStatus_ALREADY_DECLARED,
    SymbolTableStatus_MAX_SCOPE_REACHED,
};

/// Delete a symbol table
void symbolTable_delete(SymbolTable *symbol_table);

/// Creates an empty symbol table
SymbolTable *symbolTable_create(void);

/// Get the current info of the current symbol
///
/// @param table The symbol table we are working with
/// @param symbol The symbol we want to look for
/// @return the symbol we are looking for. If the symbol we
///     are looking for does not exist, `SymbolInfo.scope == 0`.
SymbolInfo symbolTable_lookup(SymbolTable *, const char *);

/// Declare a symbol
///
/// @param table The symbol table working with
/// @param symbol The symbol/name
/// @param type The type of the symbol
/// @param lowerbound The lowerbound of the symbol's value
/// @param upperbound The upperbound of the symbol's value
enum SymbolTableOpStatus symbolTable_declare_symbol(SymbolTable *, const char *,
                                                    const char *, int64_t,
                                                    int64_t);

/// Process a symbol table when enter scope
///
/// @param table The symbol table working with
enum SymbolTableOpStatus symbolTable_enter_scope(SymbolTable *);

/// Process a symbol table when exit scope
///
/// @param table The symbol table working with
void symbolTable_exit_scope(SymbolTable *);

#endif
