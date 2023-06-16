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
// String
//

void String_initialize(String *string) {
	string->data = malloc(string->maxSize);
	if (string->data == NULL) {
		printf("malloc failed\n");
		abort();
	}
}

void String_appendCharsCount(String *string, char *chars, size_t count) {
	if (string->size + count >= string->maxSize) {
		// more than double the size of the string
		string->maxSize = (string->maxSize + string->size + count + 1) * 2;
		
		// reallocate
		char *oldData = string->data;
		
		string->data = malloc(string->maxSize);
		if (string->data == NULL) {
			printf("malloc failed\n");
			abort();
		}
		
		stpncpy(string->data, oldData, string->size);
		
		free(oldData);
	}
	
	stpncpy(string->data + string->size, chars, count);
	
	string->size += count;
	
	string->data[string->size] = 0;
}

//void String_appendUint(String *string, const unsigned int number) {
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

void String_free(String *string) {
	free(string->data);
}
