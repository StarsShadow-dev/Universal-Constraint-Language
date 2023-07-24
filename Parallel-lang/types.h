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
// SubString
//

typedef struct {
	char *start;
	int length;
} SubString;

/// returns 1, if the sub string and string have different lengths
int SubString_string_cmp(SubString *subString, char *string);

/// returns 1, if the sub strings have different lengths
int SubString_SubString_cmp(SubString *subString1, SubString *subString2);

#define SubString_print(subString) printf("%.*s", (subString)->length, (subString)->start)

//
// variables
//

// "VariableType_variable" and "Variable_variable" I should probably name this something else
typedef enum {
	VariableType_simpleType,
	VariableType_function,
	VariableType_variable,
	VariableType_struct
} VariableType;

typedef struct {
	SubString *key;
	VariableType type;
	int byteSize;
	int byteAlign;
	
	uint8_t value[] WORD_ALIGNED;
} Variable;

typedef struct {
	char *LLVMtype;
} Variable_simpleType;

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
	int LLVMRegister;
	char *LLVMtype;
	linkedList_Node *type;
} Variable_variable;

typedef struct {
	char *LLVMname;
	/// SubString
	linkedList_Node *memberNames;
	// for properties and methods
	/// Variable
	linkedList_Node *memberVariables;
} Variable_struct;

//
// lexer, parser and builder
//

// Sub strings are stored as part of lexer tokens.
// Pointers to those sub strings are used in builder AST nodes.

typedef struct {
	int line;
	int columnStart;
	int columnEnd;
} SourceLocation;

typedef enum {
	TokenType_word,
	TokenType_separator,
	TokenType_operator,
	TokenType_number,
	TokenType_string
} TokenType;

typedef struct {
	TokenType type;
	SourceLocation location;
	SubString subString;
} Token;

typedef enum {
	ASTnodeType_type,
	ASTnodeType_struct,
	ASTnodeType_new,
	ASTnodeType_function,
	ASTnodeType_call,
	ASTnodeType_while,
	ASTnodeType_if,
	ASTnodeType_return,
	ASTnodeType_variableDefinition,
	ASTnodeType_variableAssignment,
	ASTnodeType_operator,
	
	// I do not really need an ASTnode_true or ASTnode_false struct because I would not be storing anything in them
	ASTnodeType_true,
	ASTnodeType_false,
	
	ASTnodeType_number,
	ASTnodeType_string,
	
	ASTnodeType_variable
} ASTnodeType;

typedef struct {
	ASTnodeType nodeType;
	SourceLocation location;
	uint8_t value[] WORD_ALIGNED;
} ASTnode;

typedef struct {
	SubString *name;
} ASTnode_type;

typedef struct {
	SubString *name;
	linkedList_Node *block;
} ASTnode_struct;

typedef struct {
	SubString *name;
	linkedList_Node *arguments;
} ASTnode_new;

typedef struct {
	SubString *name;
	int external;
	linkedList_Node *returnType;
	linkedList_Node *argumentNames;
	linkedList_Node *argumentTypes;
	linkedList_Node *codeBlock;
} ASTnode_function;

// a function call
typedef struct {
	SubString *name;
	linkedList_Node *arguments;
} ASTnode_call;

typedef struct {
	linkedList_Node *expression;
	linkedList_Node *codeBlock;
} ASTnode_while;

typedef struct {
	linkedList_Node *expression;
	linkedList_Node *codeBlock;
} ASTnode_if;

typedef struct {
	linkedList_Node *expression;
} ASTnode_return;

typedef struct {
	SubString *name;
	linkedList_Node *type;
	linkedList_Node *expression;
} ASTnode_variableDefinition;

typedef enum {
	ASTnode_operatorType_assignment,
	ASTnode_operatorType_equivalent,
	ASTnode_operatorType_greaterThan,
	ASTnode_operatorType_lessThan,
	ASTnode_operatorType_add,
	ASTnode_operatorType_subtract,
	ASTnode_operatorType_memberAccess
} ASTnode_operatorType;

typedef struct {
	ASTnode_operatorType operatorType;
	linkedList_Node *left;
	linkedList_Node *right;
} ASTnode_operator;

typedef struct {
	SubString *string;
	int64_t value;
} ASTnode_number;

typedef struct {
	SubString *value;
} ASTnode_string;

typedef struct {
	SubString *name;
} ASTnode_variable;

//
// CharAccumulator
//

typedef struct {
	size_t maxSize;
	size_t size;
	char *data;
} CharAccumulator;

void CharAccumulator_initialize(CharAccumulator *accumulator);

void CharAccumulator_appendChar(CharAccumulator *accumulator, char character);

void CharAccumulator_appendCharsCount(CharAccumulator *accumulator, char *chars, unsigned long count);

void CharAccumulator_appendChars(CharAccumulator *accumulator, char *chars);

#define CharAccumulator_appendSubString(accumulator, subString) CharAccumulator_appendCharsCount(accumulator, (subString)->start, (subString)->length)

void CharAccumulator_appendUint(CharAccumulator *accumulator, const unsigned int number);

void CharAccumulator_free(CharAccumulator *accumulator);

#endif /* types_h */
