#include "symbolTable.h"

#include "util.h"
#include <assert.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================== Hash function =======================================

/// hash then fold, FNV-1a.
static uint64_t hash_str(const char *symbol)
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
    char type[NAME_MAX + 1];
    int64_t lowerbound, upperbound;
    struct SymbolScope *next;
    struct SymbolTableItem *last_declared_same_scope;
};

static void symbolScope_free(struct SymbolScope *scope)
{
    free(scope);
}

static void symbolScope_free_recursive(struct SymbolScope *scope)
{
    if (scope != NULL) {
        symbolScope_free_recursive(scope->next);
    }
    symbolScope_free(scope);
}

struct SymbolStack {
    struct SymbolScope *head;
};

static void symbolStack_free(struct SymbolStack *stack)
{
    if (stack != NULL) {
        symbolScope_free_recursive(stack->head);
    }
    free(stack);
}

static struct SymbolScope *symbolStack_pop(struct SymbolStack *stack)
{
    if (stack == NULL) {
        return NULL;
    }
    struct SymbolScope *ret = stack->head;
    stack->head = ret->next;
    ret->next = NULL;
    return ret;
}

struct SymbolTableItem {
    char symbol[NAME_MAX + 1];
    struct SymbolStack *stack;
    size_t index;

    // essentially treating the chain as a doubly linked list
    struct SymbolTableItem *next, *last;
};

static struct SymbolTableItem *
symbolTableItem_free(struct SymbolTableItem *item)
{
    struct SymbolTableItem *next = item->next, *last = item->last;
    if (next != NULL) {
        next->last = last;
    }
    if (last != NULL) {
        last->next = next;
    }

    symbolStack_free(item->stack);
    free(item);

    // this is arguably really handy. Consider the case that we want to delete a
    // head of the item list. We delete it, and get the next item returned to be
    // the head immediately. Not book-keeping necessary.
    return next;
}

static void symbolTableItem_free_recursive(struct SymbolTableItem *item)
{
    if (item == NULL) {
        return;
    }
    symbolTableItem_free_recursive(item->next);
    symbolStack_free(item->stack);
    free(item);
}

// =============================== Symbol table ==================================

struct SymbolTable {
    struct SymbolTableItem *table[SYMBOL_TABLE_SIZE];
    uint8_t scope;

    // Note: this array's 0th element is always NULL. Do not need to account
    // for add/minus one or whatnot, at ith scope just use arr[i].
    struct SymbolTableItem *scope_first_declared[MAX_NESTED_SCOPE_COUNT + 1];
};

SymbolTable *symbolTable_create(void)
{
    SymbolTable *table = (SymbolTable *)xcalloc(
        sizeof(struct SymbolTable),
        "Memo ry issue, cannot allocate space for a new symbol table");

    table->scope = 1;
    return table;
}

void symbolTable_delete(SymbolTable *symbol_table)
{
    // iterate over every field and delete them
    for (size_t i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        symbolTableItem_free_recursive(symbol_table->table[i]);
    }
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
    size_t index = h & (SYMBOL_TABLE_SIZE - 1);

    struct SymbolTableItem *curr;
    for (curr = table->table[index]; curr; curr = curr->next) {
        if (strcmp(symbol, curr->symbol) != 0) {
            continue;
        }
        struct SymbolScope *var = curr->stack->head;
        info.scope = var->scope;
        info.lowerbound = var->lowerbound;
        info.upperbound = var->upperbound;
        snprintf(info.type, sizeof(info.type), "%s", var->type);
        break;
    }

    return info; // All tombstones and/or items
}

enum SymbolTableOpStatus symbolTable_enter_scope(SymbolTable *table)
{
    if (table->scope == MAX_NESTED_SCOPE_COUNT) {
        return SymbolTableStatus_MAX_SCOPE_REACHED;
    }
    table->scope += 1;
    return SymbolTableStatus_OK;
}

void symbolTable_exit_scope(SymbolTable *table)
{
    // scope cannot be 0 essentially
    assert(table->scope > 1);

    struct SymbolTableItem *curr = table->scope_first_declared[table->scope];
    while (curr) {
        // remove the declaration

        struct SymbolScope *scope = symbolStack_pop(curr->stack);
        struct SymbolTableItem *next = scope->last_declared_same_scope;

        symbolScope_free(scope);

        // If stack is empty, remove this symbol

        if (curr->stack->head == NULL) {
            size_t index = curr->index;
            bool is_head = table->table[index] == curr;
            struct SymbolTableItem *head = symbolTableItem_free(curr);
            if (is_head) {
                table->table[index] = head;
            }
        }

        curr = next;
    }

    table->scope_first_declared[table->scope] = NULL;
    table->scope -= 1;
}

enum SymbolTableOpStatus symbolTable_declare_symbol(SymbolTable *table,
                                                    const char *symbol,
                                                    const char *type,
                                                    int64_t lowerbound,
                                                    int64_t upperbound)
{
    uint64_t h = hash_str(symbol);
    size_t ind = h & (SYMBOL_TABLE_SIZE - 1);

    // Find the corresponding SymbolTableItem

    struct SymbolTableItem *last_item = NULL, *item = table->table[ind];
    while (item != NULL) {
        if (strcmp(item->symbol, symbol) != 0) {
            last_item = item;
            item = item->next;
        } else {
            break;
        }
    }
    if (item == NULL) {
        // Here, we did not find our symbol. Add another symbol.

        item = xcalloc(
            sizeof(struct SymbolTableItem),
            "SymbolTable::symbolTable_declare_symbol: malloc item failed");

        // pointer shenanigans

        item->next = NULL;
        item->last = last_item;
        if (last_item != NULL) {
            last_item->next = item;
        }

        // book-keeping

        snprintf(item->symbol, sizeof(item->symbol), "%s", symbol);
        item->index = ind;
        item->stack = xcalloc(
            sizeof(struct SymbolStack),
            "SymbolTable::symbolTable_declare_symbol: initialize stack failed");

        if (last_item == NULL) { // this implies the ll is empty
            table->table[ind] = item;
        }
    }

    // Get the head and ensure that multiple declaration same scope is not
    // violated

    struct SymbolStack *stack = item->stack;
    if (stack->head != NULL && stack->head->scope == table->scope) {
        return SymbolTableStatus_ALREADY_DECLARED;
    }

    // Else, add the decl and do book keepings.

    struct SymbolScope *decl = xcalloc(
        sizeof(struct SymbolScope),
        "SymbolTable::symbolTable_declare_symbol: initialize declaration failed");
    decl->lowerbound = lowerbound;
    decl->upperbound = upperbound;
    snprintf(decl->type, sizeof(decl->type), "%s", type);
    decl->scope = table->scope;

    decl->next = stack->head;
    stack->head = decl;

    // push the decl on top of the symbol_decl_this_scope kinda thing

    decl->last_declared_same_scope = table->scope_first_declared[table->scope];
    table->scope_first_declared[table->scope] = item;

    return SymbolTableStatus_OK;
}
