#ifndef utilities_h
#define utilities_h

#include <time.h>

void *safeMalloc(size_t size);
struct timespec getTimespec(void);
uint64_t getMilliseconds(struct timespec start, struct timespec end);

#endif /* utilities_h */
