/*
 * Test target for cbounded: a mix of strcpy() calls, some provably safe,
 * some overflowing, some that cannot be decided without more context.
 *
 * Each call site is tagged with one of:
 *   SAFE    - destination is always large enough
 *   UNSAFE  - destination can (or always will) overflow
 *   UNKNOWN - depends on the caller; not decidable locally
 *
 * Running the binary with no extra args only exercises SAFE paths.
 * Pass "unsafe" as argv[1] to also run the overflowing ones (UB!).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME_LEN 32

struct user {
	char name[NAME_LEN];
	char tag[4];
	int id;
};

/* ---------------------------------------------------------------- SAFE */

/* Literal (6 bytes incl. NUL) into 16-byte array. */
void safe_literal_small(void)
{
	char buf[16];
	strcpy(buf, "hello"); /* SAFE */
	printf("%s\n", buf);
}

/* Literal exactly fills the buffer, including the NUL. */
void safe_literal_exact_fit(void)
{
	char buf[6];
	strcpy(buf, "hello"); /* SAFE */
	printf("%s\n", buf);
}

/* Source array is smaller than destination array. */
void safe_array_to_bigger_array(void)
{
	char src[8] = "abc";
	char dst[16];
	strcpy(dst, src); /* SAFE */
	printf("%s\n", dst);
}

/* Same-size arrays: src[8] always holds at most 7 chars + NUL. */
void safe_same_size_arrays(void)
{
	char src[8] = "1234567";
	char dst[8];
	strcpy(dst, src); /* SAFE */
	printf("%s\n", dst);
}

/* Length is checked correctly before copying. */
void safe_guarded_by_strlen(const char *s)
{
	char dst[NAME_LEN];
	if (strlen(s) < sizeof(dst)) {
		strcpy(dst, s); /* SAFE */
		printf("%s\n", dst);
	}
}

/* Early-return guard instead of wrapping the copy. */
void safe_early_return_guard(const char *s)
{
	char dst[NAME_LEN];
	if (strlen(s) >= NAME_LEN)
		return;
	strcpy(dst, s); /* SAFE */
	printf("%s\n", dst);
}

/* Heap buffer sized from the source. */
char *safe_heap_dup(const char *s)
{
	char *p = malloc(strlen(s) + 1);
	if (!p)
		return NULL;
	strcpy(p, s); /* SAFE */
	return p;
}

/* Struct member with a literal that fits. */
void safe_struct_field(struct user *u)
{
	strcpy(u->name, "administrator"); /* SAFE */
	strcpy(u->tag, "ok"); /* SAFE */
}

/* Ternary picks between two literals, both fit. */
void safe_ternary_literals(int flag)
{
	char buf[8];
	strcpy(buf, flag ? "yes" : "no"); /* SAFE */
	printf("%s\n", buf);
}

/* Copy back into the same buffer type from a struct of equal size. */
void safe_struct_to_struct(struct user *dst, const struct user *src)
{
	strcpy(dst->name, src->name); /* SAFE */
}

/* -------------------------------------------------------------- UNSAFE */

/* Literal needs 6 bytes, buffer has 5 (classic off-by-one on NUL). */
void unsafe_literal_off_by_one(void)
{
	char buf[5];
	strcpy(buf, "hello"); /* UNSAFE */
	printf("%s\n", buf);
}

/* Literal is just way too long. */
void unsafe_literal_too_long(void)
{
	char buf[4];
	strcpy(buf, "this string is far too long"); /* UNSAFE */
	printf("%s\n", buf);
}

/* Source array larger than destination array. */
void unsafe_array_to_smaller_array(void)
{
	char src[64] = "a moderately long string that will not fit";
	char dst[8];
	strcpy(dst, src); /* UNSAFE */
	printf("%s\n", dst);
}

/* Untrusted parameter straight into a fixed buffer. */
void unsafe_unchecked_param(const char *s)
{
	char dst[NAME_LEN];
	strcpy(dst, s); /* UNSAFE */
	printf("%s\n", dst);
}

/* Off-by-one guard: <= allows strlen == sizeof, no room for NUL. */
void unsafe_guard_off_by_one(const char *s)
{
	char dst[NAME_LEN];
	if (strlen(s) <= sizeof(dst)) {
		strcpy(dst, s); /* UNSAFE */
		printf("%s\n", dst);
	}
}

/* Guard checks the wrong buffer's size. */
void unsafe_guard_wrong_buffer(const char *s)
{
	char big[128];
	char small[16];
	if (strlen(s) < sizeof(big)) {
		strcpy(small, s); /* UNSAFE */
		printf("%s\n", small);
	}
	(void)big;
}

/* Heap buffer forgets the +1 for the NUL terminator. */
char *unsafe_heap_missing_nul(const char *s)
{
	char *p = malloc(strlen(s));
	if (!p)
		return NULL;
	strcpy(p, s); /* UNSAFE */
	return p;
}

/* sizeof(pointer) mistaken for buffer size. */
void unsafe_sizeof_pointer(const char *s)
{
	char *p = malloc(64);
	if (!p)
		return;
	if (strlen(s) < sizeof(p)) /* sizeof(p) is 8, check is fine... */
		strcpy(p, s); /* SAFE (accidentally, check is too strict) */
	strcpy(p, s); /* UNSAFE: no check at all on this path */
	free(p);
}

/* Struct member too small for the literal. */
void unsafe_struct_field(struct user *u)
{
	strcpy(u->tag, "admin"); /* UNSAFE: tag is 4 bytes */
}

/* Bigger field copied into smaller field. */
void unsafe_field_to_field(struct user *u)
{
	strcpy(u->tag, u->name); /* UNSAFE */
}

/* Ternary where one branch overflows. */
void unsafe_ternary_literals(int flag)
{
	char buf[4];
	strcpy(buf, flag ? "ok" : "definitely not ok"); /* UNSAFE */
	printf("%s\n", buf);
}

/* Copying into the middle of a buffer shrinks the available space. */
void unsafe_offset_dest(void)
{
	char buf[8];
	strcpy(buf + 4, "hello"); /* UNSAFE: only 4 bytes left */
	printf("%s\n", buf + 4);
}

/* Data read from stdin may be up to 255 chars. */
void unsafe_from_fgets(void)
{
	char line[256];
	char name[16];
	if (fgets(line, sizeof(line), stdin)) {
		strcpy(name, line); /* UNSAFE */
		printf("%s", name);
	}
}

/* Concatenation in a loop overruns the accumulator. */
void unsafe_loop_append(void)
{
	char acc[16] = "";
	for (int i = 0; i < 10; i++)
		strcpy(acc + strlen(acc), "chunk"); /* UNSAFE */
	printf("%s\n", acc);
}

/* ------------------------------------------------------------- UNKNOWN */

/* Destination size is not known inside this function. */
void unknown_dest_param(char *dst, const char *src)
{
	strcpy(dst, src); /* UNKNOWN */
}

/* Destination size known, source length is not; caller decides. */
void unknown_wrapper(char *out)
{
	strcpy(out, "wrapper-default"); /* UNKNOWN: 16 bytes needed */
}

/* Callers of unknown_wrapper: one fine, one not. */
void call_unknown_wrapper_safe(void)
{
	char buf[32];
	unknown_wrapper(buf); /* SAFE via call */
	printf("%s\n", buf);
}

void call_unknown_wrapper_unsafe(void)
{
	char buf[8];
	unknown_wrapper(buf); /* UNSAFE via call */
	printf("%s\n", buf);
}

/* ---------------------------------------------------------------- main */

int main(int argc, char **argv)
{
	struct user u = { .name = "guest", .tag = "x", .id = 1 };
	struct user v = { 0 };
	char scratch[64];
	char *dup;

	safe_literal_small();
	safe_literal_exact_fit();
	safe_array_to_bigger_array();
	safe_same_size_arrays();
	safe_guarded_by_strlen("short");
	safe_early_return_guard("also short");
	dup = safe_heap_dup("duplicated");
	if (dup) {
		printf("%s\n", dup);
		free(dup);
	}
	safe_struct_field(&u);
	safe_ternary_literals(argc > 1);
	safe_struct_to_struct(&v, &u);
	unknown_dest_param(scratch, "fits in scratch");
	printf("%s\n", scratch);
	call_unknown_wrapper_safe();

	if (argc > 1 && strcmp(argv[1], "unsafe") == 0) {
		const char *input = argc > 2 ? argv[2] :
			"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

		unsafe_literal_off_by_one();
		unsafe_literal_too_long();
		unsafe_array_to_smaller_array();
		unsafe_unchecked_param(input);
		unsafe_guard_off_by_one(input);
		unsafe_guard_wrong_buffer(input);
		dup = unsafe_heap_missing_nul(input);
		free(dup);
		unsafe_sizeof_pointer(input);
		unsafe_struct_field(&u);
		unsafe_field_to_field(&u);
		unsafe_ternary_literals(0);
		unsafe_offset_dest();
		unsafe_from_fgets();
		unsafe_loop_append();
		call_unknown_wrapper_unsafe();
	}

	return 0;
}
