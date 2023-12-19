#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "utilities.h"

void *safeMalloc(size_t size) {
	void *pointer = malloc(size);
	if (pointer == NULL) {
		printf("safeMalloc: malloc failed\n");
		abort();
	}
	return pointer;
}

struct timespec getTimespec(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
//	timespec_get(&ts, TIME_UTC);
	return ts;
}

uint64_t getMilliseconds(struct timespec start, struct timespec end) {
	return (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
}
