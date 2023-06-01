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

void String_appendCharsCount(String *string, char *chars, unsigned long count) {
	if (string->size + count >= string->maxSize) {
		// double the size of the string
		string->maxSize = string->maxSize * 2;
		
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

char *buildLLVM(linkedList_Node **current) {
	String LLVMsource = {100, 0, 0};
	String_initialize(&LLVMsource);
	
	while (1) {
		if (*current == NULL) {
			break;
		}
		
		ASTnode *node = ((ASTnode *)((*current)->data));
		
		printf("node type: %u\n", node->type);
		
		switch (node->type) {
			case ASTnodeType_function: {
				ASTnode_function *function = (ASTnode_function *)node->value;
				
				String_appendChars(&LLVMsource, "define i32 @");
				String_appendChars(&LLVMsource, function->name);
				String_appendChars(&LLVMsource, "() {\n\tret i32 0\n}");
				
				break;
			}
				
//			case ASTnodeType_type: {
//				break;
//			}
//
//			case ASTnodeType_number: {
//				break;
//			}
				
			default: {
				printf("unknown node type: %u\n", node->type);
				compileError(node->location);
				break;
			}
		}
		
		*current = (*current)->next;
	}
	
	return LLVMsource.data;
}
