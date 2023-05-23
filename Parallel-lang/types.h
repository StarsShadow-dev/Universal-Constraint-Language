#ifndef types_h
#define types_h

struct linkedList_Node {
	struct linkedList_Node *next;
	char data[];
};

typedef struct linkedList_Node linkedList_Node;

void *linkedList_addNode(linkedList_Node **head, unsigned long size);

void linkedList_freeList(linkedList_Node **head);

#endif /* types_h */
