#ifndef utilities_h
#define utilities_h

#include <time.h>

#define hashSize 20

void *safeMalloc(size_t size);
struct timespec getTimespec(void);
uint64_t getMilliseconds(struct timespec start, struct timespec end);

void hashContents(char *in, size_t length, char *out);

#endif /* utilities_h */
