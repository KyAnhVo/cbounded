#include "traversal.h"
#include <clang-c/CXSourceLocation.h>
#include <clang-c/Index.h>
#include <clang-c/CXString.h>
#include <stdio.h>

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
	CXString kind =
		clang_getCursorKindSpelling(clang_getCursorKind(cursor));
	printf("%s: %s\n", clang_getCString(kind), clang_getCString(name));
	clang_disposeString(name);
	clang_disposeString(kind);

	*((int *)data) += 1;
	clang_visitChildren(cursor, &traverse_tu_visitor, data);
	*((int *)data) -= 1;

	return CXChildVisit_Continue;
}
