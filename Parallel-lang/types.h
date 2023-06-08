#ifndef types_h
#define types_h

#include <stdlib.h>
#include <string.h>

//
// linkedList
//

struct linkedList_Node {
	struct linkedList_Node *next;
	uint8_t data[];
};
typedef struct linkedList_Node linkedList_Node;

void *linkedList_addNode(linkedList_Node **head, unsigned long size);

void linkedList_freeList(linkedList_Node **head);

//
// lexer, parser and builder
//

typedef struct {
	int line;
	int columnStart;
	int columnEnd;
} SourceLocation;

typedef enum {
	TokenType_word,
	TokenType_number,
	TokenType_string,
	TokenType_separator
} TokenType;

typedef struct {
	TokenType type;
	SourceLocation location;
	char value[];
} Token;

typedef enum {
	ASTnodeType_type,
	ASTnodeType_function,
	ASTnodeType_return,
	ASTnodeType_number
} ASTnodeType;

typedef struct {
	char *name;
} ASTnode_type;

typedef struct {
	char *name;
	linkedList_Node *returnType;
	linkedList_Node *arguments;
	linkedList_Node *codeBlock;
} ASTnode_function;

typedef struct {
	linkedList_Node *expression;
} ASTnode_return;

typedef struct {
	char *string;
	int64_t value;
} ASTnode_number;

typedef struct {
	ASTnodeType type;
	SourceLocation location;
	char value[];
} ASTnode;

//
// variables
//

typedef enum {
	VariableType_type
} VariableType;

typedef struct {
	char *LLVMname;
} Variable_type;

typedef struct {
	char *key;
	VariableType type;
	char value[];
} Variable;

//
// String
//

typedef struct {
	int maxSize;
	int size;
	char *data;
} String;

void String_initialize(String *string);

void String_appendCharsCount(String *string, char *chars, unsigned long count);

#define String_appendChars(string, chars) String_appendCharsCount(string, chars, strlen(chars))

void String_free(String *string);

#endif /* types_h */
