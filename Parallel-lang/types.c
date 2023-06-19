#include <stdlib.h>
#include <stdio.h>

#include "types.h"
#include "error.h"

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
// CharAccumulator
//

void CharAccumulator_initialize(CharAccumulator *charAccumulator) {
	charAccumulator->data = malloc(charAccumulator->maxSize);
	if (charAccumulator->data == NULL) {
		printf("malloc failed\n");
		abort();
	}
}

void CharAccumulator_appendChar(CharAccumulator *charAccumulator, char character) {
	if (charAccumulator->size + 1 >= charAccumulator->maxSize) {
		// more than double the size of the string
		charAccumulator->maxSize = (charAccumulator->maxSize + charAccumulator->size + 1 + 1) * 2;
		
		// reallocate
		char *oldData = charAccumulator->data;
		
		charAccumulator->data = malloc(charAccumulator->maxSize);
		if (charAccumulator->data == NULL) {
			printf("malloc failed\n");
			abort();
		}
		
		stpncpy(charAccumulator->data, oldData, charAccumulator->size);
		
		free(oldData);
	}
	
	charAccumulator->data[charAccumulator->size] = character;
	
	charAccumulator->size += 1;
	
	charAccumulator->data[charAccumulator->size] = 0;
}

void CharAccumulator_appendCharsCount(CharAccumulator *charAccumulator, char *chars, size_t count) {
	if (charAccumulator->size + count >= charAccumulator->maxSize) {
		// more than double the size of the string
		charAccumulator->maxSize = (charAccumulator->maxSize + charAccumulator->size + count + 1) * 2;
		
		// reallocate
		char *oldData = charAccumulator->data;
		
		charAccumulator->data = malloc(charAccumulator->maxSize);
		if (charAccumulator->data == NULL) {
			printf("malloc failed\n");
			abort();
		}
		
		stpncpy(charAccumulator->data, oldData, charAccumulator->size);
		
		free(oldData);
	}
	
	stpncpy(charAccumulator->data + charAccumulator->size, chars, count);
	
	charAccumulator->size += count;
	
	charAccumulator->data[charAccumulator->size] = 0;
}

//void CharAccumulator_appendUint(CharAccumulator *charAccumulator, const unsigned int number) {
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

void CharAccumulator_free(CharAccumulator *charAccumulator) {
	free(charAccumulator->data);
}
