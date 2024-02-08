#ifndef types_h
#define types_h

#include <stdlib.h>
#include <string.h>

#define WORD_ALIGNED __attribute__ ((aligned(8)))

//
// SubString
//

typedef struct {
	char *start;
	int length;
} SubString;

SubString *SubString_new(char *start, int length);

SubString *getSubStringFromString(char *string);

/// returns 1, if the sub string and string have different lengths
int SubString_string_cmp(SubString *subString, char *string);

/// returns 1, if the sub strings have different lengths
int SubString_SubString_cmp(SubString *subString1, SubString *subString2);

#define SubString_print(subString) printf("%.*s", (subString)->length, (subString)->start)

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
// Dictionary
//

struct Dictionary {
	SubString *key;
	struct Dictionary *next;
	uint8_t data[];
};
typedef struct Dictionary Dictionary;

void *Dictionary_addNode(Dictionary **head, SubString *key, unsigned long size);

void *Dictionary_getFromSubString(Dictionary *head, SubString *key);
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
} FileContext;

typedef struct {
	int ID;
	CharAccumulator *topLevelStructSource;
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

FileInformation *FileInformation_new(char *path, CharAccumulator *topLevelStructSource, CharAccumulator *topLevelConstantSource, CharAccumulator *topLevelFunctionSource, CharAccumulator *LLVMmetadataSource);

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
	TokenType_selfReference,
	TokenType_floatingType // @
} TokenType;

typedef struct {
	TokenType type;
	SourceLocation location;
	SubString subString;
} Token;

typedef enum {
	ASTnodeType_constrainedType,
	ASTnodeType_struct,
	ASTnodeType_function,
	ASTnodeType_call,
	ASTnodeType_while,
	ASTnodeType_if,
	ASTnodeType_return,
	ASTnodeType_variableDefinition,
	ASTnodeType_variableAssignment,
	
	ASTnodeType_infixOperator,
	
	// TODO: replace with ASTnodeType_bool?
	ASTnodeType_bool,
	
	ASTnodeType_number,
	ASTnodeType_string,
	
	ASTnodeType_identifier,
	ASTnodeType_subscript,
	
	ASTnodeType_selfReference
} ASTnodeType;

typedef struct {
	ASTnodeType nodeType;
	SourceLocation location;
	uint8_t value[] WORD_ALIGNED;
} ASTnode;

typedef struct {
	linkedList_Node *type;
	linkedList_Node *constraints;
} ASTnode_constrainedType;

typedef struct {
	linkedList_Node *block;
} ASTnode_struct;

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
	ASTnode_infixOperatorType_assignment,
	ASTnode_infixOperatorType_equivalent,
	ASTnode_infixOperatorType_notEquivalent,
	ASTnode_infixOperatorType_greaterThan,
	ASTnode_infixOperatorType_lessThan,
	ASTnode_infixOperatorType_add,
	ASTnode_infixOperatorType_subtract,
	ASTnode_infixOperatorType_multiply,
	ASTnode_infixOperatorType_divide,
	ASTnode_infixOperatorType_modulo,
	ASTnode_infixOperatorType_and,
	ASTnode_infixOperatorType_or,
	ASTnode_infixOperatorType_memberAccess,
	
	// node: the right of operatorType_cast is a type
	ASTnode_infixOperatorType_cast
} ASTnode_infixOperatorType;

typedef struct {
	ASTnode_infixOperatorType operatorType;
	linkedList_Node *left;
	linkedList_Node *right;
} ASTnode_infixOperator;

typedef struct {
	int isTrue;
} ASTnode_bool;

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

typedef struct {
	linkedList_Node *left;
	linkedList_Node *right;
} ASTnode_subscript;

SubString *ASTnode_getSubStringFromString(ASTnode *node, FileInformation *FI);

int64_t ASTnode_getIntFromNumber(ASTnode *node, FileInformation *FI);

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
	ASTnode_infixOperatorType operatorType;
	ASTnode *left;
	ASTnode *rightConstant;
} Fact_expression;

typedef struct {
	ASTnode *condition;
	linkedList_Node *trueFacts;
	linkedList_Node *falseFacts;
} Fact_if;

void Fact_newExpression(linkedList_Node **head, ASTnode_infixOperatorType operatorType, ASTnode *left, ASTnode *rightConstant);

//
// context
//

typedef enum {
	ContextBindingType_struct,
	ContextBindingType_simpleType,
	ContextBindingType_function,
	ContextBindingType_variable,
	ContextBindingType_compileTimeSetting
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
	
	/// Dictionary of BuilderTypes
	Dictionary *states;
	/// [linkedList<Fact>]
	linkedList_Node *factStack[maxContextLevel];
} BuilderType;

typedef struct {
	char *LLVMname;
	linkedList_Node *memberBindings;
} ContextBinding_struct;

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
	int LLVMRegister;
	char *LLVMtype;
	BuilderType type;
} ContextBinding_variable;

typedef struct {
	SubString *value;
} ContextBinding_compileTimeSetting;

int FileInformation_declaredInLLVM(FileInformation *FI, ContextBinding *pointer);
void FileInformation_addToDeclaredInLLVM(FileInformation *FI, ContextBinding *pointer);

void addContextBinding_simpleType(linkedList_Node **context, char *name, char *LLVMtype, int byteSize, int byteAlign);
void addContextBinding_compileTimeSetting(linkedList_Node **context, char *name, char *value);
ContextBinding *getContextBindingFromString(FileInformation *FI, char *key);
ContextBinding *getContextBindingFromSubString(FileInformation *FI, SubString *key);

ASTnode *BuilderType_getResolvedValue(BuilderType *type, FileInformation *FI);

BuilderType *BuilderType_getStateFromSubString(BuilderType *type, SubString *key);
BuilderType *BuilderType_getStateFromString(BuilderType *type, char *key);

int BuilderType_hasName(BuilderType *type, char *name);
int BuilderType_hasCoreName(BuilderType *type, char *name);
int BuilderType_isSignedInt(BuilderType *type);
int BuilderType_isUnsignedInt(BuilderType *type);
int BuilderType_isInt(BuilderType *type);
int BuilderType_isFloat(BuilderType *type);
int BuilderType_isNumber(BuilderType *type);

char *BuilderType_getLLVMname(BuilderType *type, FileInformation *FI);

int BuilderType_getByteSize(BuilderType *type);
int BuilderType_getByteAlign(BuilderType *type);

#endif /* types_h */
