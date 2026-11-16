#ifndef UTIL_H
#define UTIL_H

#include <linux/limits.h>

typedef struct {
	char variableName[NAME_MAX + 1];
	char type[NAME_MAX + 1];
} Condition;

#endif
