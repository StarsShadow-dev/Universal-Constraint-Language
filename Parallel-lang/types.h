#ifndef types_h
#define types_h

#include <stdlib.h>
#include <string.h>

#define WORD_ALIGNED __attribute__ ((aligned(8)))

//
// linkedList
//

struct linkedList_Node {
	struct linkedList_Node *next;
	uint8_t data[];
};
typedef struct linkedList_Node linkedList_Node;

void *linkedList_addNode(linkedList_Node **head, unsigned long size);

/// THIS FUNCTION HAS NOT BEEN THOROUGHLY TESTED
void linkedList_join(linkedList_Node **head1, linkedList_Node **head2);

int linkedList_getCount(linkedList_Node **head);

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
	TokenType_separator,
	TokenType_number,
	TokenType_string
} TokenType;

typedef struct {
	TokenType type;
	SourceLocation location;
	char value[];
} Token;

typedef enum {
	ASTnodeType_type,
	ASTnodeType_function,
	ASTnodeType_call,
	ASTnodeType_return,
	ASTnodeType_number,
	ASTnodeType_string
} ASTnodeType;

typedef struct {
	char *name;
} ASTnode_type;

typedef struct {
	char *name;
	int external;
	linkedList_Node *returnType;
	linkedList_Node *argumentNames;
	linkedList_Node *argumentTypes;
	linkedList_Node *codeBlock;
} ASTnode_function;

// a function call
typedef struct {
	char *name;
	linkedList_Node *arguments;
} ASTnode_call;

typedef struct {
	linkedList_Node *expression;
} ASTnode_return;

typedef struct {
	char *string;
	int64_t value;
} ASTnode_number;

typedef struct {
	char *string;
} ASTnode_string;

typedef struct {
	ASTnodeType type;
	SourceLocation location;
	uint8_t value[] WORD_ALIGNED;
} ASTnode;

//
// variables
//

typedef enum {
	VariableType_type,
	VariableType_function
} VariableType;

typedef struct {
	char *LLVMname;
} Variable_type;

typedef struct {
	char *LLVMname;
	char *LLVMreturnType;
	linkedList_Node *argumentTypes;
	linkedList_Node *returnType;
	
	int hasReturned;
	// for LLVM registers
	int registerCount;
} Variable_function;

typedef struct {
	char *key;
	VariableType type;
	uint8_t value[] WORD_ALIGNED;
} Variable;

//
// String
//

typedef struct {
	size_t maxSize;
	size_t size;
	char *data;
} String;

void String_initialize(String *string);

void String_appendCharsCount(String *string, char *chars, unsigned long count);

#define String_appendChars(string, chars) String_appendCharsCount(string, chars, strlen(chars))

//void String_appendUint(String *string, const unsigned int number);

void String_free(String *string);

#endif /* types_h */
