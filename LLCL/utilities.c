#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "utilities.h"
#include "sha1.h"

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

// out is hashSize bytes
void hashContents(char *in, size_t length, char *out) {
	SHA1_CTX context;
	sha1_init(&context);
	sha1_update(&context, (unsigned char *)in, length);
	sha1_final(&context, (unsigned char *)out);
}
