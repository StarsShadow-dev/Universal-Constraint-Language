#include "types.h"
#include <stdlib.h>
#include <stdio.h>

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

void linkedList_freeList(linkedList_Node **head) {
	linkedList_Node *current = *head;
	while (current != NULL) {
		linkedList_Node *next = current->next;
		free(current);
		current = next;
	}
}
