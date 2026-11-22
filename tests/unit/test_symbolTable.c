/*
 * Unit tests for symbolTable.c
 *
 * Each test case runs in its own forked child process with a timeout, so a
 * crash or an infinite loop in one case is reported as a failure of that case
 * instead of killing (or hanging) the whole suite.
 *
 * Run all:      ./test_symbolTable
 * Run one:      ./test_symbolTable shadowing_restored_on_exit
 */
#include "symbolTable.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_TIMEOUT_SECONDS 5

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            fprintf(stderr, "    %s:%d: CHECK failed: %s\n", __FILE__,      \
                    __LINE__, #cond);                                       \
            return 1;                                                       \
        }                                                                   \
    } while (0)

#define CHECK_FOUND(info) CHECK((info).scope != 0)
#define CHECK_NOT_FOUND(info) CHECK((info).scope == 0)

// ============================== Basic operations ==============================

static int test_create_and_delete_empty(void)
{
    SymbolTable *t = symbolTable_create();
    CHECK(t != NULL);
    symbolTable_delete(t);
    return 0;
}

static int test_lookup_missing_symbol(void)
{
    SymbolTable *t = symbolTable_create();
    CHECK_NOT_FOUND(symbolTable_lookup(t, "x"));
    symbolTable_delete(t);
    return 0;
}

static int test_declare_then_lookup(void)
{
    SymbolTable *t = symbolTable_create();
    CHECK(symbolTable_declare_symbol(t, "x", "int", -3, 7) ==
          SymbolTableStatus_OK);

    SymbolInfo info = symbolTable_lookup(t, "x");
    CHECK_FOUND(info);
    CHECK(info.lowerbound == -3);
    CHECK(info.upperbound == 7);
    CHECK(strcmp(info.type, "int") == 0);

    symbolTable_delete(t);
    return 0;
}

static int test_extreme_bounds_preserved(void)
{
    SymbolTable *t = symbolTable_create();
    CHECK(symbolTable_declare_symbol(t, "big", "long", INT64_MIN, INT64_MAX) ==
          SymbolTableStatus_OK);

    SymbolInfo info = symbolTable_lookup(t, "big");
    CHECK_FOUND(info);
    CHECK(info.lowerbound == INT64_MIN);
    CHECK(info.upperbound == INT64_MAX);

    symbolTable_delete(t);
    return 0;
}

static int test_multiple_symbols_independent(void)
{
    SymbolTable *t = symbolTable_create();
    CHECK(symbolTable_declare_symbol(t, "a", "int", 1, 1) ==
          SymbolTableStatus_OK);
    CHECK(symbolTable_declare_symbol(t, "b", "char", 2, 2) ==
          SymbolTableStatus_OK);

    SymbolInfo a = symbolTable_lookup(t, "a");
    SymbolInfo b = symbolTable_lookup(t, "b");
    CHECK_FOUND(a);
    CHECK_FOUND(b);
    CHECK(a.lowerbound == 1 && strcmp(a.type, "int") == 0);
    CHECK(b.lowerbound == 2 && strcmp(b.type, "char") == 0);
    CHECK_NOT_FOUND(symbolTable_lookup(t, "c"));

    symbolTable_delete(t);
    return 0;
}

// =============================== Redeclaration ================================

static int test_redeclare_same_scope_rejected(void)
{
    SymbolTable *t = symbolTable_create();
    CHECK(symbolTable_declare_symbol(t, "x", "int", 0, 0) ==
          SymbolTableStatus_OK);
    CHECK(symbolTable_declare_symbol(t, "x", "int", 5, 5) ==
          SymbolTableStatus_ALREADY_DECLARED);

    // The original declaration must be untouched.
    SymbolInfo info = symbolTable_lookup(t, "x");
    CHECK_FOUND(info);
    CHECK(info.lowerbound == 0);

    symbolTable_delete(t);
    return 0;
}

// ================================== Scopes ====================================

static int test_shadowing_restored_on_exit(void)
{
    SymbolTable *t = symbolTable_create();
    CHECK(symbolTable_declare_symbol(t, "x", "int", 0, 10) ==
          SymbolTableStatus_OK);
    SymbolInfo outer = symbolTable_lookup(t, "x");
    CHECK_FOUND(outer);

    CHECK(symbolTable_enter_scope(t) == SymbolTableStatus_OK);
    CHECK(symbolTable_declare_symbol(t, "x", "char", 100, 200) ==
          SymbolTableStatus_OK);

    SymbolInfo inner = symbolTable_lookup(t, "x");
    CHECK_FOUND(inner);
    CHECK(inner.scope > outer.scope);
    CHECK(inner.lowerbound == 100);
    CHECK(strcmp(inner.type, "char") == 0);

    symbolTable_exit_scope(t);

    SymbolInfo after = symbolTable_lookup(t, "x");
    CHECK_FOUND(after);
    CHECK(after.scope == outer.scope);
    CHECK(after.lowerbound == 0);
    CHECK(after.upperbound == 10);
    CHECK(strcmp(after.type, "int") == 0);

    symbolTable_delete(t);
    return 0;
}

static int test_outer_symbol_visible_in_inner_scope(void)
{
    SymbolTable *t = symbolTable_create();
    CHECK(symbolTable_declare_symbol(t, "g", "int", 1, 1) ==
          SymbolTableStatus_OK);

    CHECK(symbolTable_enter_scope(t) == SymbolTableStatus_OK);
    CHECK(symbolTable_enter_scope(t) == SymbolTableStatus_OK);
    CHECK_FOUND(symbolTable_lookup(t, "g"));
    symbolTable_exit_scope(t);
    symbolTable_exit_scope(t);

    CHECK_FOUND(symbolTable_lookup(t, "g"));
    symbolTable_delete(t);
    return 0;
}

static int test_inner_only_symbol_removed_on_exit(void)
{
    SymbolTable *t = symbolTable_create();
    CHECK(symbolTable_enter_scope(t) == SymbolTableStatus_OK);
    CHECK(symbolTable_declare_symbol(t, "tmp", "int", 0, 0) ==
          SymbolTableStatus_OK);
    CHECK_FOUND(symbolTable_lookup(t, "tmp"));

    // "tmp" is the only item in its bucket: exercises removing a bucket head
    // whose successor is NULL.
    symbolTable_exit_scope(t);
    CHECK_NOT_FOUND(symbolTable_lookup(t, "tmp"));

    symbolTable_delete(t);
    return 0;
}

static int test_same_name_redeclarable_after_scope_exit(void)
{
    SymbolTable *t = symbolTable_create();
    CHECK(symbolTable_enter_scope(t) == SymbolTableStatus_OK);
    CHECK(symbolTable_declare_symbol(t, "i", "int", 0, 0) ==
          SymbolTableStatus_OK);
    symbolTable_exit_scope(t);

    CHECK(symbolTable_enter_scope(t) == SymbolTableStatus_OK);
    CHECK(symbolTable_declare_symbol(t, "i", "int", 9, 9) ==
          SymbolTableStatus_OK);
    SymbolInfo info = symbolTable_lookup(t, "i");
    CHECK_FOUND(info);
    CHECK(info.lowerbound == 9);
    symbolTable_exit_scope(t);

    symbolTable_delete(t);
    return 0;
}

/// Two sibling blocks at the same depth: `{ int a; } { int b; }`.
/// Exiting the second must not touch what the first block declared.
static int test_sibling_scopes_independent(void)
{
    SymbolTable *t = symbolTable_create();

    CHECK(symbolTable_enter_scope(t) == SymbolTableStatus_OK);
    CHECK(symbolTable_declare_symbol(t, "a", "int", 0, 0) ==
          SymbolTableStatus_OK);
    symbolTable_exit_scope(t);

    CHECK(symbolTable_enter_scope(t) == SymbolTableStatus_OK);
    CHECK_NOT_FOUND(symbolTable_lookup(t, "a"));
    CHECK(symbolTable_declare_symbol(t, "b", "int", 0, 0) ==
          SymbolTableStatus_OK);
    symbolTable_exit_scope(t);

    CHECK_NOT_FOUND(symbolTable_lookup(t, "a"));
    CHECK_NOT_FOUND(symbolTable_lookup(t, "b"));

    symbolTable_delete(t);
    return 0;
}

static int test_exit_removes_all_symbols_of_scope(void)
{
    SymbolTable *t = symbolTable_create();
    CHECK(symbolTable_declare_symbol(t, "keep", "int", 0, 0) ==
          SymbolTableStatus_OK);

    CHECK(symbolTable_enter_scope(t) == SymbolTableStatus_OK);
    CHECK(symbolTable_declare_symbol(t, "p", "int", 0, 0) ==
          SymbolTableStatus_OK);
    CHECK(symbolTable_declare_symbol(t, "q", "int", 0, 0) ==
          SymbolTableStatus_OK);
    CHECK(symbolTable_declare_symbol(t, "r", "int", 0, 0) ==
          SymbolTableStatus_OK);
    symbolTable_exit_scope(t);

    CHECK_NOT_FOUND(symbolTable_lookup(t, "p"));
    CHECK_NOT_FOUND(symbolTable_lookup(t, "q"));
    CHECK_NOT_FOUND(symbolTable_lookup(t, "r"));
    CHECK_FOUND(symbolTable_lookup(t, "keep"));

    symbolTable_delete(t);
    return 0;
}

static int test_max_scope_depth(void)
{
    SymbolTable *t = symbolTable_create();

    int entered = 0;
    while (symbolTable_enter_scope(t) == SymbolTableStatus_OK) {
        entered++;
        CHECK(entered <= MAX_NESTED_SCOPE_COUNT); // guard against no limit
    }
    CHECK(symbolTable_enter_scope(t) ==
          SymbolTableStatus_MAX_SCOPE_REACHED);

    // Declaring at the deepest scope must work and stay in bounds.
    CHECK(symbolTable_declare_symbol(t, "deep", "int", 1, 1) ==
          SymbolTableStatus_OK);
    CHECK_FOUND(symbolTable_lookup(t, "deep"));

    for (int i = 0; i < entered; i++) {
        symbolTable_exit_scope(t);
    }
    CHECK_NOT_FOUND(symbolTable_lookup(t, "deep"));

    symbolTable_delete(t);
    return 0;
}

// ============================ Hashing / collisions ============================

/// More symbols than buckets guarantees collisions (pigeonhole), so this
/// exercises walking bucket chains in lookup, declare and exit_scope.
#define MANY_SYMBOLS (SYMBOL_TABLE_SIZE + SYMBOL_TABLE_SIZE / 2)

static int test_many_symbols_with_collisions(void)
{
    SymbolTable *t = symbolTable_create();
    char name[32];

    CHECK(symbolTable_enter_scope(t) == SymbolTableStatus_OK);
    for (int i = 0; i < MANY_SYMBOLS; i++) {
        snprintf(name, sizeof(name), "v%d", i);
        CHECK(symbolTable_declare_symbol(t, name, "int", i, i) ==
              SymbolTableStatus_OK);
    }

    for (int i = 0; i < MANY_SYMBOLS; i++) {
        snprintf(name, sizeof(name), "v%d", i);
        SymbolInfo info = symbolTable_lookup(t, name);
        CHECK_FOUND(info);
        CHECK(info.lowerbound == i);
    }
    CHECK_NOT_FOUND(symbolTable_lookup(t, "not_declared"));

    symbolTable_exit_scope(t);
    for (int i = 0; i < MANY_SYMBOLS; i++) {
        snprintf(name, sizeof(name), "v%d", i);
        CHECK_NOT_FOUND(symbolTable_lookup(t, name));
    }

    symbolTable_delete(t);
    return 0;
}

// ================================== Cleanup ===================================

/// Deleting a table that still has live symbols in nested scopes must free
/// everything (run under ASan/LeakSanitizer to verify).
static int test_delete_with_live_symbols(void)
{
    SymbolTable *t = symbolTable_create();
    CHECK(symbolTable_declare_symbol(t, "x", "int", 0, 0) ==
          SymbolTableStatus_OK);
    CHECK(symbolTable_enter_scope(t) == SymbolTableStatus_OK);
    CHECK(symbolTable_declare_symbol(t, "x", "int", 1, 1) ==
          SymbolTableStatus_OK);
    CHECK(symbolTable_declare_symbol(t, "y", "int", 2, 2) ==
          SymbolTableStatus_OK);

    symbolTable_delete(t);
    return 0;
}

// ================================== Runner ====================================

struct TestCase {
    const char *name;
    int (*fn)(void);
};

#define TEST(fn_name) { #fn_name, test_##fn_name }

static const struct TestCase TESTS[] = {
    TEST(create_and_delete_empty),
    TEST(lookup_missing_symbol),
    TEST(declare_then_lookup),
    TEST(extreme_bounds_preserved),
    TEST(multiple_symbols_independent),
    TEST(redeclare_same_scope_rejected),
    TEST(shadowing_restored_on_exit),
    TEST(outer_symbol_visible_in_inner_scope),
    TEST(inner_only_symbol_removed_on_exit),
    TEST(same_name_redeclarable_after_scope_exit),
    TEST(sibling_scopes_independent),
    TEST(exit_removes_all_symbols_of_scope),
    TEST(max_scope_depth),
    TEST(many_symbols_with_collisions),
    TEST(delete_with_live_symbols),
};

#define TEST_COUNT (sizeof(TESTS) / sizeof(TESTS[0]))

/// Runs one test in a child process. Returns 0 on pass.
static int run_isolated(const struct TestCase *tc)
{
    fflush(stdout);
    fflush(stderr);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        alarm(TEST_TIMEOUT_SECONDS);
        // exit(), not _exit(): LeakSanitizer runs its check from an atexit
        // handler, so _exit() would silently skip leak detection.
        exit(tc->fn());
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == 0) {
            printf("[PASS] %s\n", tc->name);
            return 0;
        }
        printf("[FAIL] %s (exit status %d)\n", tc->name, WEXITSTATUS(status));
        return 1;
    }
    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig == SIGALRM) {
            printf("[FAIL] %s (timed out after %ds, infinite loop?)\n",
                   tc->name, TEST_TIMEOUT_SECONDS);
        } else {
            printf("[FAIL] %s (crashed: %s)\n", tc->name, strsignal(sig));
        }
        return 1;
    }
    printf("[FAIL] %s (unknown termination)\n", tc->name);
    return 1;
}

int main(int argc, char **argv)
{
    int failed = 0;
    int ran = 0;

    for (size_t i = 0; i < TEST_COUNT; i++) {
        if (argc > 1 && strcmp(argv[1], TESTS[i].name) != 0) {
            continue;
        }
        ran++;
        failed += run_isolated(&TESTS[i]);
    }

    if (ran == 0) {
        fprintf(stderr, "No test named '%s'\n", argc > 1 ? argv[1] : "");
        return 1;
    }

    printf("\n%d/%d passed\n", ran - failed, ran);
    return failed == 0 ? 0 : 1;
}
