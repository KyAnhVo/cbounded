# cbounded: C Buffer Overflow detector, specialized in libc writes, WITH CONTEXT

## Why context?

The assumption that all calls to "dangerous" stdlib functions are dangerous
is not true. Consider the following:

```c
char* cpy(char* src, size_t n) 
{
    char* dest = malloc(n + 1);
    strcpy(dest, src);
    return dest;
}
```

If `n >= strlen(src)` is maintained in our codebase, this function above will 
be correct but flagged as a bug in naive "is this a strcpy" buffer overflow detector.
Standard practice of replacing most `strcpy` with `strncpy` can be helpful in 
most cases, but doing that in huge codebases is difficult. That is why we have 
to take context into consideration.

## Method

**Note:** integer overflows are ignored for now for v0.1.

We consider each function to have its own precondition and postcondition.
- **Precondition:** what the function needs from its caller to run safely
- **Postcondition:** what the function changes, and what's guaranteed about
these changes in terms of its inputs

For example, consider our `cpy2` function:

```c
char* cpy2(char* src, size_t n) 
{
    char* dest = malloc(n * 2);
    strcpy(dest, src);
    return dest;
}
```

The precondition is as follows:
- `strlen(src) + 1 <= n * 2`

And the postcondition is:
- `strlen(dest) == strlen(src)`
- `capacity(dest) == n * 2`

**Note:** Notice that the postcondition presumes the precondition is correct.

Now, we also set up the preconditions and postconditions for our stdlib functions
that we want to analyze, and then raise an error when a call site violate a callee's
precondition.
