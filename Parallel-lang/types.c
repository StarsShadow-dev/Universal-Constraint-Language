#include <stdlib.h>
#include <stdio.h>

#include "types.h"
#include "report.h"
#include "utilities.h"

//
// linkedList
//

void *linkedList_addNode(linkedList_Node **head, unsigned long size) {
	linkedList_Node* newNode = calloc(1, sizeof(linkedList_Node) + size);
	if (newNode == NULL) {
		printf("calloc failed to get space for a linked list\n");
		abort();
	}
	
	if (*head == NULL) {
		*head = newNode;
	} else {
		linkedList_Node *current = *head;
		while (1) {
			if (current->next == NULL) {
				current->next = newNode;
				break;
			}
			current = current->next;
		}
	}
	
	return newNode->data;
}

/// THIS FUNCTION HAS NOT BEEN THOROUGHLY TESTED
void linkedList_join(linkedList_Node **head1, linkedList_Node **head2) {
	if (head2 == NULL) return;
	
	if (*head1 == NULL) {
		*head1 = *head2;
	} else {
		linkedList_Node *current = *head1;
		while (1) {
			if (current->next == NULL) {
				current->next = *head2;
				return;
			}
			current = current->next;
		}
	}
}

int linkedList_getCount(linkedList_Node **head) {
	int count = 0;
	
	linkedList_Node *current = *head;
	while (current != NULL) {
		count++;
		current = current->next;
	}
	
	return count;
}

void linkedList_freeList(linkedList_Node **head) {
	if (head == NULL) {
		return;
	}
	
	linkedList_Node *current = *head;
	while (current != NULL) {
		linkedList_Node *next = current->next;
		free(current);
		current = next;
	}
	
	*head = NULL;
}

//
// SubString
//

int SubString_string_cmp(SubString *subString, char *string) {
	if (subString->length == strlen(string)) {
		return strncmp(subString->start, string, subString->length);
	} else {
		return 1;
	}
}

int SubString_SubString_cmp(SubString *subString1, SubString *subString2) {
	if (subString1->length == subString2->length) {
		return strncmp(subString1->start, subString2->start, subString1->length);
	} else {
		return 1;
	}
}

//
// CharAccumulator
//

void CharAccumulator_initialize(CharAccumulator *accumulator) {
	if (accumulator->data != NULL) {
		free(accumulator->data);
	}
	accumulator->data = safeMalloc(accumulator->maxSize);
	
	accumulator->size = 0;
	
	// add a NULL bite
	*accumulator->data = 0;
}

void CharAccumulator_appendChar(CharAccumulator *accumulator, char character) {
	if (accumulator->size + 1 >= accumulator->maxSize) {
		// more than double the size of the string
		accumulator->maxSize = (accumulator->maxSize + accumulator->size + 1 + 1) * 2;
		
		// reallocate
		char *oldData = accumulator->data;
		
		accumulator->data = safeMalloc(accumulator->maxSize);
		
		stpncpy(accumulator->data, oldData, accumulator->size);
		
		free(oldData);
	}
	
	accumulator->data[accumulator->size] = character;
	
	accumulator->size += 1;
	
	accumulator->data[accumulator->size] = 0;
}

void CharAccumulator_appendCharsCount(CharAccumulator *accumulator, char *chars, size_t count) {
	if (accumulator->size + count >= accumulator->maxSize) {
		// more than double the size of the string
		accumulator->maxSize = (accumulator->maxSize + accumulator->size + count + 1) * 2;
		
		// reallocate
		char *oldData = accumulator->data;
		
		accumulator->data = safeMalloc(accumulator->maxSize);
		
		stpncpy(accumulator->data, oldData, accumulator->size);
		
		free(oldData);
	}
	
	stpncpy(accumulator->data + accumulator->size, chars, count);
	
	accumulator->size += count;
	
	accumulator->data[accumulator->size] = 0;
}

void CharAccumulator_appendChars(CharAccumulator *accumulator, char *chars) {
	CharAccumulator_appendCharsCount(accumulator, chars, strlen(chars));
}

void CharAccumulator_appendInt(CharAccumulator *accumulator, int64_t number) {
	if (number < 0) {
		printf("CharAccumulator_appendInt error");
		abort();
	}
	int64_t n = number;
	int count = 1;

	while (n > 9) {
		n /= 10;
		count++;
	}

	if (accumulator->size + count >= accumulator->maxSize) {
		// more than double the size of the string
		accumulator->maxSize = (accumulator->maxSize + accumulator->size + count + 1) * 2;
		
		// reallocate
		char *oldData = accumulator->data;
		
		accumulator->data = safeMalloc(accumulator->maxSize);
		
		stpncpy(accumulator->data, oldData, accumulator->size);
		
		free(oldData);
	}

	sprintf(accumulator->data + accumulator->size, "%lld", number);

	accumulator->size += count;
}

void CharAccumulator_free(CharAccumulator *accumulator) {
	free(accumulator->data);
}


ModuleInformation *ModuleInformation_new(char *path, CharAccumulator *topLevelConstantSource, CharAccumulator *topLevelFunctionSource, CharAccumulator *LLVMmetadataSource) {
	ModuleInformation *MI = safeMalloc(sizeof(ModuleInformation));
	
	*MI = (ModuleInformation){
		.name = NULL,
		.path = path,
		.topLevelConstantSource = topLevelConstantSource,
		.topLevelFunctionSource = topLevelFunctionSource,
		.LLVMmetadataSource = LLVMmetadataSource,
		
		.stringCount = 0,
		.metadataCount = 0,
		
		// level is -1 so that it starts at 0 for the first iteration
		.level = -1,
		.context = {
			.currentSource = NULL,
			.currentFilePath = NULL,
			
			.bindings = {0},
			.importedModules = NULL,
		},
		
		.debugInformationCompileUnitID = 0,
		.debugInformationFileScopeID = 0
	};

	return MI;
}
