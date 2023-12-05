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

void linkedList_join(linkedList_Node **head1, linkedList_Node **head2);

int linkedList_getCount(linkedList_Node **head);

void linkedList_freeList(linkedList_Node **head);

linkedList_Node *linkedList_getLast(linkedList_Node *head);

linkedList_Node *linkedList_popLast(linkedList_Node **head);

//
// SubString
//

typedef struct {
	char *start;
	int length;
} SubString;

SubString *SubString_new(char *start, int length);

/// returns 1, if the sub string and string have different lengths
int SubString_string_cmp(SubString *subString, char *string);

/// returns 1, if the sub strings have different lengths
int SubString_SubString_cmp(SubString *subString1, SubString *subString2);

#define SubString_print(subString) printf("%.*s", (subString)->length, (subString)->start)

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

void CharAccumulator_appendInt(CharAccumulator *accumulator, int64_t number);

void CharAccumulator_free(CharAccumulator *accumulator);

#define maxContextLevel 10

typedef struct {
	char *currentSource;
	char *path;
	linkedList_Node *declaredInLLVM;
	
	linkedList_Node *bindings[maxContextLevel];
	linkedList_Node *importedFiles;
} FileContext;

typedef struct {
	int ID;
	CharAccumulator *topLevelConstantSource;
	CharAccumulator *topLevelFunctionSource;
	CharAccumulator *LLVMmetadataSource;
	
	int stringCount;
	int metadataCount;
	
	int level;
	FileContext context;
	
	int debugInformationCompileUnitID;
	int debugInformationFileScopeID;
} FileInformation;

FileInformation *FileInformation_new(char *path, CharAccumulator *topLevelConstantSource, CharAccumulator *topLevelFunctionSource, CharAccumulator *LLVMmetadataSource);

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
	TokenType_number,
	TokenType_pound, // #
	TokenType_ellipsis, // ...
	TokenType_separator,
	TokenType_operator,
	TokenType_string,
	TokenType_selfReference
} TokenType;

typedef struct {
	TokenType type;
	SourceLocation location;
	SubString subString;
} Token;

typedef enum {
	ASTnodeType_queryLocation,
	ASTnodeType_import,
	ASTnodeType_constrainedType,
	ASTnodeType_struct,
	ASTnodeType_implement,
	ASTnodeType_function,
	ASTnodeType_call,
	ASTnodeType_macro,
	ASTnodeType_runMacro,
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
	
	ASTnodeType_identifier,
	
	ASTnodeType_selfReference
} ASTnodeType;

typedef struct {
	ASTnodeType nodeType;
	SourceLocation location;
	uint8_t value[] WORD_ALIGNED;
} ASTnode;

typedef struct {
	SubString *path;
} ASTnode_import;

typedef struct {
	linkedList_Node *type;
	linkedList_Node *constraints;
} ASTnode_constrainedType;

typedef struct {
	SubString *name;
	linkedList_Node *block;
} ASTnode_struct;

typedef struct {
	SubString *name;
	linkedList_Node *block;
} ASTnode_implement;

typedef struct {
	SubString *name;
	int external;
	ASTnode *returnType;
	linkedList_Node *argumentNames;
	linkedList_Node *argumentTypes;
	linkedList_Node *codeBlock;
} ASTnode_function;

// a function call
typedef struct {
	linkedList_Node *left;
	linkedList_Node *arguments;
} ASTnode_call;

typedef struct {
	SubString *name;
	linkedList_Node *codeBlock;
} ASTnode_macro;

typedef struct {
	linkedList_Node *left;
	linkedList_Node *arguments;
} ASTnode_runMacro;

typedef struct {
	linkedList_Node *expression;
	linkedList_Node *codeBlock;
} ASTnode_while;

typedef struct {
	linkedList_Node *expression;
	linkedList_Node *trueCodeBlock;
	linkedList_Node *falseCodeBlock;
} ASTnode_if;

typedef struct {
	linkedList_Node *expression;
} ASTnode_return;

typedef struct {
	SubString *name;
	ASTnode *type;
	linkedList_Node *expression;
} ASTnode_variableDefinition;

typedef enum {
	ASTnode_operatorType_assignment,
	ASTnode_operatorType_equivalent,
	ASTnode_operatorType_greaterThan,
	ASTnode_operatorType_lessThan,
	ASTnode_operatorType_add,
	ASTnode_operatorType_subtract,
	ASTnode_operatorType_multiply,
	ASTnode_operatorType_memberAccess,
	ASTnode_operatorType_scopeResolution
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
} ASTnode_identifier;

void getASTnodeDescription(FileInformation *FI, CharAccumulator *charAccumulator, ASTnode *node);

//
// facts
//

typedef enum {
	FactType_expression,
	FactType_if
} FactType;

typedef struct {
	FactType type;
	uint8_t value[] WORD_ALIGNED;
} Fact;

typedef struct {
	ASTnode_operatorType operatorType;
	ASTnode *left;
	ASTnode *rightConstant;
} Fact_expression;

typedef struct {
	ASTnode *condition;
	linkedList_Node *trueFacts;
	linkedList_Node *falseFacts;
} Fact_if;

//
// context
//

typedef enum {
	ContextBindingType_simpleType,
	ContextBindingType_function,
	ContextBindingType_macro,
	ContextBindingType_variable,
	ContextBindingType_compileTimeSetting,
	ContextBindingType_struct,
	ContextBindingType_namespace
} ContextBindingType;

typedef struct {
	FileInformation *originFile;
	SubString *key;
	ContextBindingType type;
	int byteSize;
	int byteAlign;
	
	uint8_t value[] WORD_ALIGNED;
} ContextBinding;

typedef struct {
	ContextBinding *binding;
	linkedList_Node *constraintNodes;
	// Fact
	linkedList_Node *factStack[maxContextLevel];
} BuilderType;

int BuilderType_hasName(BuilderType *type, char *name);
int BuilderType_isInt(BuilderType *type);
int BuilderType_isFloat(BuilderType *type);
int BuilderType_isNumber(BuilderType *type);

typedef struct {
	char *LLVMtype;
} ContextBinding_simpleType;

typedef struct {
	char *LLVMname;
	char *LLVMreturnType;
	/// BuilderType
	linkedList_Node *argumentNames;
	/// BuilderType
	linkedList_Node *argumentTypes;
	BuilderType returnType;
	
	// for LLVM registers
	int registerCount;
	
	int debugInformationScopeID;
} ContextBinding_function;

typedef struct {
	linkedList_Node *codeBlock;
} ContextBinding_macro;

typedef struct {
	int LLVMRegister;
	char *LLVMtype;
	BuilderType type;
} ContextBinding_variable;

typedef struct {
	SubString *value;
} ContextBinding_compileTimeSetting;

typedef struct {
	char *LLVMname;
	linkedList_Node *propertyBindings;
	// ContextBinding (should all be ContextBinding_function)
	linkedList_Node *methodBindings;
} ContextBinding_struct;

typedef struct {
	linkedList_Node *files;
} ContextBinding_namespace;

int FileInformation_declaredInLLVM(FileInformation *FI, ContextBinding *pointer);
void FileInformation_addToDeclaredInLLVM(FileInformation *FI, ContextBinding *pointer);

#endif /* types_h */
