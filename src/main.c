#include <clang-c/Index.h>
#include <stdio.h>
#include <stdlib.h>

#include "traversal.h"

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("Input: file");
		exit(1);
	}
	CXIndex index = clang_createIndex(0, 0);

	CXTranslationUnit tu = clang_parseTranslationUnit(
		index, argv[1], NULL, 0, NULL, 0, CXTranslationUnit_None);

	if (tu == NULL) {
		fprintf(stderr, "Failed to parse translation unit\n");
		clang_disposeIndex(index);
		return 1;
	}

	find_strcpy(tu);

	// Cleanup
	clang_disposeTranslationUnit(tu);
	clang_disposeIndex(index);
	return 0;
}
