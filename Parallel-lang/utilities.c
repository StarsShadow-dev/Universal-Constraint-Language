#include <stdlib.h>
#include <stdio.h>

#include "utilities.h"

void *safeMalloc(size_t size) {
	void *pointer = malloc(size);
	if (pointer == NULL) {
		printf("safeMalloc: malloc failed");
		abort();
	}
	return pointer;
}
