#ifndef TRAVERSAL_H
#define TRAVERSAL_H
#include <clang-c/Index.h>

/// Traverses and prints out the function/variable
/// names of a translation unit.
void traverse_tu(CXTranslationUnit);

/// traverses and prints all occurrences of a strcpy
/// being called in a translation unit
void find_strcpy(CXTranslationUnit);

#endif
