#ifndef types_h
#define types_h

#include <stdlib.h>

typedef struct {
	int line;
	int columnStart;
	int columnEnd;
} SourceLocation;

typedef enum {
	TokenType_word,
	TokenType_string
} TokenType;

typedef struct {
	TokenType type;
	SourceLocation location;
	char value[];
} token;

struct linkedList_Node {
	struct linkedList_Node *next;
	uint8_t data[];
};
typedef struct linkedList_Node linkedList_Node;

void *linkedList_addNode(linkedList_Node **head, unsigned long size);

void linkedList_freeList(linkedList_Node **head);

#endif /* types_h */
