#include <stdlib.h>
#include <stdio.h>

#include "types.h"
#include "error.h"

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
}

//
// SubString
//

int SubString_SubString_cmp(SubString *subString1, SubString *subString2) {
	if (subString1->length == subString2->length) {
		return strncmp(subString1->start, subString2->start, subString1->length);;
	} else {
		return 1;
	}
}

//
// CharAccumulator
//

void CharAccumulator_initialize(CharAccumulator *accumulator) {
	accumulator->data = malloc(accumulator->maxSize);
	if (accumulator->data == NULL) {
		printf("malloc failed\n");
		abort();
	}
}

void CharAccumulator_appendChar(CharAccumulator *accumulator, char character) {
	if (accumulator->size + 1 >= accumulator->maxSize) {
		// more than double the size of the string
		accumulator->maxSize = (accumulator->maxSize + accumulator->size + 1 + 1) * 2;
		
		// reallocate
		char *oldData = accumulator->data;
		
		accumulator->data = malloc(accumulator->maxSize);
		if (accumulator->data == NULL) {
			printf("malloc failed\n");
			abort();
		}
		
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
		
		accumulator->data = malloc(accumulator->maxSize);
		if (accumulator->data == NULL) {
			printf("malloc failed\n");
			abort();
		}
		
		stpncpy(accumulator->data, oldData, accumulator->size);
		
		free(oldData);
	}
	
	stpncpy(accumulator->data + accumulator->size, chars, count);
	
	accumulator->size += count;
	
	accumulator->data[accumulator->size] = 0;
}

//void CharAccumulator_appendUint(CharAccumulator *accumulator, const unsigned int number) {
//	int n = number;
//	int count = 1;
//
//	while (n > 9) {
//		n /= 10;
//		count++;
//	}
//
//	if (string->size + count >= string->maxSize) {
//		// double the size of the string
//		string->maxSize = string->maxSize * 2;
//
//		// reallocate
//		char *oldData = string->data;
//
//		string->data = malloc(string->maxSize);
//		if (string->data == NULL) {
//			printf("malloc failed\n");
//			abort();
//		}
//
//		stpncpy(string->data, oldData, string->size);
//
//		free(oldData);
//	}
//
//	sprintf(string->data + string->size, "%u", number);
//
//	string->size += count;
//}

void CharAccumulator_free(CharAccumulator *accumulator) {
	free(accumulator->data);
}
