#include "traversal.h"

#include <clang-c/CXFile.h>
#include <clang-c/CXSourceLocation.h>
#include <clang-c/Index.h>
#include <clang-c/CXString.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>

// ----------------------------------------------------------------------------
// ---------------------- Traverse and print tree nodes -----------------------
// ----------------------------------------------------------------------------

enum CXChildVisitResult traverse_tu_visitor(CXCursor cursor, CXCursor parent,
                                            CXClientData data);

void traverse_tu(CXTranslationUnit tu)
{
    CXCursor cursor = clang_getTranslationUnitCursor(tu);
    int tab = 0;
    clang_visitChildren(cursor, &traverse_tu_visitor, (CXClientData)&tab);
}

enum CXChildVisitResult traverse_tu_visitor(CXCursor cursor, CXCursor parent,
                                            CXClientData data)
{
    (void)parent;
    if (!clang_Location_isFromMainFile(clang_getCursorLocation(cursor))) {
        return CXChildVisit_Continue;
    }

    int tab = *((int *)data);
    for (int i = 0; i < tab; i++) {
        printf("|   ");
    }

    CXString name = clang_getCursorDisplayName(cursor);
    CXString kind = clang_getCursorKindSpelling(clang_getCursorKind(cursor));
    printf("%s: %s\n", clang_getCString(kind), clang_getCString(name));
    clang_disposeString(name);
    clang_disposeString(kind);

    *((int *)data) += 1;
    clang_visitChildren(cursor, &traverse_tu_visitor, data);
    *((int *)data) -= 1;

    return CXChildVisit_Continue;
}

// ----------------------------------------------------------------------------
// ---------- Traverse the tree and find all instance of  ---------------
// ----------------------------------------------------------------------------

struct FindStrcpyFunction {
    char Name[NAME_MAX + 1];
};

enum CXChildVisitResult find_strcpy_visitor(CXCursor cursor, CXCursor parent,
                                            CXClientData data);

void find_strcpy(CXTranslationUnit tu)
{
    CXCursor cursor = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(cursor, &find_strcpy_visitor, (CXClientData)NULL);
}

void find_strcpy_visitor_file_scope(CXCursor cursor)
{
    if (clang_getCursorKind(cursor) == CXCursor_FunctionDecl &&
        clang_isCursorDefinition(cursor)) {
        struct FindStrcpyFunction data;
        CXString name_cxstring = clang_getCursorSpelling(cursor);
        const char *name = clang_getCString(name_cxstring);
        strncpy(data.Name, name, NAME_MAX + 1);
        clang_disposeString(name_cxstring);
        clang_visitChildren(cursor, &find_strcpy_visitor, (void *)&data);
    }
}

enum CXChildVisitResult find_strcpy_visitor(CXCursor cursor, CXCursor parent,
                                            CXClientData data)
{
    // We do not care about parent, and files that are not from current file.
    (void)parent;
    if (!clang_Location_isFromMainFile(clang_getCursorLocation(cursor))) {
        return CXChildVisit_Continue;
    }

    // File scope
    if (data == NULL) {
        find_strcpy_visitor_file_scope(cursor);
        return CXChildVisit_Continue;
    }

    // inside function
    struct FindStrcpyFunction *caller = (struct FindStrcpyFunction *)data;

    if (clang_getCursorKind(cursor) == CXCursor_CallExpr) {
        CXCursor f = clang_getCursorReferenced(cursor);
        CXString name_f = clang_getCursorSpelling(f);
        const char *name_cstr_f = clang_getCString(name_f);
        if (strcmp(name_cstr_f, "strcpy")) {
            CXSourceLocation location = clang_getCursorLocation(cursor);

            CXFile file;
            unsigned int line, column;
            clang_getSpellingLocation(location, &file, &line, &column, NULL);
            CXString filename = clang_getFileName(file);
            const char *filename_cstr = clang_getCString(filename);

            printf("%s::%s\n\tline %i, column %i\n", filename_cstr,
                   caller->Name, line, column);

            clang_disposeString(filename);
        }
        clang_disposeString(name_f);
    }

    return CXChildVisit_Recurse;
}
