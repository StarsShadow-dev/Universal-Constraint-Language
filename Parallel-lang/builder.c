#include <stdio.h>
#include <string.h>

#include "builder.h"
#include "error.h"

typedef struct {
	int maxSize;
	int size;
	char *data;
} String;

void String_initialize(String *string) {
	string->data = malloc(string->maxSize);
	if (string->data == NULL) {
		abort();
	}
}

void String_appendCharsCount(String *string, char *chars, int count) {
	if (string->size + count >= string->maxSize) {
		// double the size of the string
		string->maxSize = string->maxSize * 2;
		
		printf("reallocating to %i\n", string->maxSize);
		
		// reallocate
		char *oldData = string->data;
		
		string->data = malloc(string->maxSize);
		if (string->data == NULL) {
			abort();
		}
		
		stpncpy(string->data, oldData, string->size);
		
		free(oldData);
	}
	
	stpncpy(string->data + string->size, chars, count);
	
	string->size += count;
	
	string->data[string->size] = 0;
}

#define String_appendChars(string, chars) String_appendCharsCount(string, chars, strlen(chars))

void String_free(String *string) {
	free(string->data);
}

void buildLLVM(void) {
	String source = {5, 0, 0};
	String_initialize(&source);
	
	String_appendChars(&source, "test");
	
	String_appendChars(&source, "test2");
	
	String_free(&source);
}
