#include <stdlib.h>
#include <stdio.h>

#include "types.h"
#include "report.h"
#include "utilities.h"

//
// SubString
//

SubString *SubString_new(char *start, int length) {
	SubString *subString = safeMalloc(sizeof(SubString));
	subString->start = start;
	subString->length = length;
	
	return subString;
}

SubString *getSubStringFromString(char *string) {
	return SubString_new(string, (int)strlen(string));
}

int SubString_string_cmp(SubString *subString, char *string) {
	if (subString->length == strlen(string)) {
		return strncmp(subString->start, string, subString->length);
	} else {
		return 1;
	}
}

int SubString_SubString_cmp(SubString *subString1, SubString *subString2) {
	if (subString1->length == subString2->length) {
		return strncmp(subString1->start, subString2->start, subString1->length);
	} else {
		return 1;
	}
}

//
// linkedList
//

void *linkedList_addNode(linkedList_Node **head, unsigned long size) {
	linkedList_Node* newNode = safeMalloc(sizeof(linkedList_Node) + size);
	
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
	
	*head = NULL;
}

linkedList_Node *linkedList_getLast(linkedList_Node *head) {
	if (head == NULL) {
		abort();
//		return NULL;
	}
	
	linkedList_Node *current = head;
	while (1) {
		if (current->next == NULL) {
			return current;
		}
		current = current->next;
	}
}

linkedList_Node *linkedList_popLast(linkedList_Node **head) {
	if (head == NULL) {
		return NULL;
	}
	
	linkedList_Node *current = *head;
	
	if ((*head)->next == NULL) {
		*head = NULL;
		return current;
	}
	
	while (1) {
		linkedList_Node *next = current->next;
		// if next is the last node
		if (next->next == NULL) {
			current->next = NULL;
			return next;
		}
		current = next;
	}
}

//
// Dictionary
//

void *Dictionary_addNode(Dictionary **head, SubString *key, unsigned long size) {
	Dictionary* newNode = safeMalloc(sizeof(Dictionary) + size);
	newNode->key = key;
	
	if (*head == NULL) {
		*head = newNode;
	} else {
		Dictionary *current = *head;
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

void *Dictionary_getFromSubString(Dictionary *head, SubString *key) {
	if (head == NULL) return NULL;
	
	Dictionary *current = head;
	while (1) {
		if (SubString_SubString_cmp(current->key, key) == 0) {
			return current->data;
		}
		
		if (current->next == NULL) {
			return NULL;
		}
		
		current = current->next;
	}
}

//
// CharAccumulator
//

void CharAccumulator_initialize(CharAccumulator *accumulator) {
	if (accumulator->data != NULL) {
		free(accumulator->data);
	}
	accumulator->data = safeMalloc(accumulator->maxSize);
	
	accumulator->size = 0;
	
	// add a NULL bite
	*accumulator->data = 0;
}

void CharAccumulator_appendChar(CharAccumulator *accumulator, char character) {
	if (accumulator->size + 1 >= accumulator->maxSize) {
		// more than double the size of the string
		accumulator->maxSize = (accumulator->maxSize + accumulator->size + 1 + 1) * 2;
		
		// reallocate
		char *oldData = accumulator->data;
		
		accumulator->data = safeMalloc(accumulator->maxSize);
		
		stpncpy(accumulator->data, oldData, accumulator->size);
		
		free(oldData);
	}
	
	accumulator->data[accumulator->size] = character;
	
	accumulator->size += 1;
	
	accumulator->data[accumulator->size] = 0;
}

void CharAccumulator_appendCharsCount(CharAccumulator *accumulator, char *chars, size_t count) {
	if (accumulator->size + count >= accumulator->maxSize) {
		// more than double the size of the string
		accumulator->maxSize = (accumulator->maxSize + accumulator->size + count + 1) * 2;
		
		// reallocate
		char *oldData = accumulator->data;
		
		accumulator->data = safeMalloc(accumulator->maxSize);
		
		stpncpy(accumulator->data, oldData, accumulator->size);
		
		free(oldData);
	}
	
	stpncpy(accumulator->data + accumulator->size, chars, count);
	
	accumulator->size += count;
	
	accumulator->data[accumulator->size] = 0;
}

void CharAccumulator_appendChars(CharAccumulator *accumulator, char *chars) {
	CharAccumulator_appendCharsCount(accumulator, chars, strlen(chars));
}

void CharAccumulator_appendInt(CharAccumulator *accumulator, int64_t number) {
	if (number < 0) {
		printf("CharAccumulator_appendInt error");
		abort();
	}
	int64_t n = number;
	int count = 1;

	while (n > 9) {
		n /= 10;
		count++;
	}

	if (accumulator->size + count >= accumulator->maxSize) {
		// more than double the size of the string
		accumulator->maxSize = (accumulator->maxSize + accumulator->size + count + 1) * 2;
		
		// reallocate
		char *oldData = accumulator->data;
		
		accumulator->data = safeMalloc(accumulator->maxSize);
		
		stpncpy(accumulator->data, oldData, accumulator->size);
		
		free(oldData);
	}

	sprintf(accumulator->data + accumulator->size, "%lld", number);

	accumulator->size += count;
}

void CharAccumulator_free(CharAccumulator *accumulator) {
	free(accumulator->data);
}

CharAccumulator *CharAccumulator_new(void) {
	CharAccumulator *charAccumulator = safeMalloc(sizeof(CharAccumulator));
	*charAccumulator = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(charAccumulator);
	return charAccumulator;
}

int nextFileID = 1;

FileInformation *FileInformation_new(char *path, CharAccumulator *topLevelStructSource, CharAccumulator *topLevelConstantSource, CharAccumulator *topLevelFunctionSource, CharAccumulator *LLVMmetadataSource) {
	FileInformation *FI = safeMalloc(sizeof(FileInformation));
	
	*FI = (FileInformation){
		.ID = nextFileID,
		.topLevelStructSource = topLevelStructSource,
		.topLevelConstantSource = topLevelConstantSource,
		.topLevelFunctionSource = topLevelFunctionSource,
		.LLVMmetadataSource = LLVMmetadataSource,
		
		.stringCount = 0,
		.metadataCount = 0,
		
		// level is -1 so that it starts at 0 for the first iteration
		.level = -1,
		.context = {
			.currentSource = NULL,
			.path = path,
			.declaredInLLVM = NULL,
			
			.scopeObjects = {0}
		},
		
		.debugInformationCompileUnitID = 0,
		.debugInformationFileScopeID = 0
	};
	
	nextFileID++;

	return FI;
}

//
// lexer, parser and builder
//

int FileInformation_declaredInLLVM(FileInformation *FI, ScopeObject *pointer) {
	linkedList_Node *current = FI->context.declaredInLLVM;
	
	while (current != NULL) {
		if (*(ScopeObject **)current->data == pointer) {
			return 1;
		}
		
		current = current->next;
	}
	
	return 0;
}

void FileInformation_addToDeclaredInLLVM(FileInformation *FI, ScopeObject *pointer) {
	ScopeObject **newPointer = linkedList_addNode(&FI->context.declaredInLLVM, sizeof(void *));
	*newPointer = pointer;
}

SubString *ASTnode_getSubStringFromString(ASTnode *node, FileInformation *FI) {
	if (node->nodeType != ASTnodeType_string) {
		addStringToReportMsg("must be a string");
		compileError(FI, node->location);
	}
	
	return ((ASTnode_string *)node->value)->value;
}

int64_t ASTnode_getIntFromNumber(ASTnode *node, FileInformation *FI) {
	if (node->nodeType != ASTnodeType_number) {
		addStringToReportMsg("must be a number");
		compileError(FI, node->location);
	}
	
	return ((ASTnode_number *)node->value)->value;
}

//
// Facts
//

void Fact_newExpression(linkedList_Node **head, ASTnode_infixOperatorType operatorType, ASTnode *left, ASTnode *rightConstant) {
	Fact *factData = linkedList_addNode(head, sizeof(Fact) + sizeof(Fact_expression));
	factData->type = FactType_expression;
	((Fact_expression *)factData->value)->operatorType = ASTnode_infixOperatorType_equivalent;
	((Fact_expression *)factData->value)->left = left;
	((Fact_expression *)factData->value)->rightConstant = rightConstant;
}

//
// context
//

BuilderType BuilderType_new(ScopeObject *scopeObject, linkedList_Node *constraintNodes) {
	return (BuilderType){
		.scopeObject = scopeObject,
		.constraintNodes = constraintNodes,
		.factStack = {0},
	};
}

BuilderType getTypeFromScopeObject(ScopeObject *scopeObject) {
	return BuilderType_new(scopeObject, NULL);
}

ScopeObject ScopeObject_new(SubString *key, int compileTime, FileInformation *originFile, BuilderType type, ScopeObjectType scopeObjectType) {
	return (ScopeObject){
		.key = key,
		.compileTime = compileTime,
		.originFile = originFile,
		.type = type,
		.scopeObjectType = scopeObjectType,
	};
}

ScopeObject_struct ScopeObject_struct_new(char *LLVMname, linkedList_Node *members, int byteSize, int byteAlign) {
	return (ScopeObject_struct){
		.LLVMname = LLVMname,
		.members  = members,
		.byteSize = byteSize,
		.byteAlign = byteAlign,
	};
}

ScopeObject_value ScopeObject_value_new(int LLVMRegister) {
	return (ScopeObject_value){
		.LLVMRegister = LLVMRegister,
	};
}

// from the core file and has name
int ContextBinding_hasCoreName(ScopeObject *binding, char *name) {
	return binding->originFile == coreFilePointer && SubString_string_cmp(binding->key, name) == 0;
}

// note: originFile = coreFilePointer
void addScopeObject_simpleType(linkedList_Node **context, char *name, char *LLVMname, int byteSize, int byteAlign) {
	SubString *key = SubString_new(name, (int)strlen(name));
	ScopeObject *data = linkedList_addNode(context, sizeof(ScopeObject) + sizeof(ScopeObject_struct));
	
	*data = ScopeObject_new(key, 1, coreFilePointer, getTypeFromScopeObject(getScopeObjectFromString(coreFilePointer, "Type")), ScopeObjectType_struct);
	*(ScopeObject_struct *)data->value = ScopeObject_struct_new(LLVMname, NULL, byteSize, byteAlign);
}

ScopeObject *getScopeObjectFromString(FileInformation *FI, char *key) {
	int index = FI->level;
	while (index >= 0) {
		linkedList_Node *current = FI->context.scopeObjects[index];
		while (current != NULL) {
			ScopeObject *binding = (ScopeObject *)current->data;
			
			if (SubString_string_cmp(binding->key, key) == 0) {
				return binding;
			}
			
			current = current->next;
		}
		
		index--;
	}
	
	linkedList_Node *current = coreFilePointer->context.scopeObjects[0];
	while (current != NULL) {
		ScopeObject *binding = (ScopeObject *)current->data;
		
		if (SubString_string_cmp(binding->key, key) == 0) {
			return binding;
		}
		
		current = current->next;
	}

	return NULL;
}

ScopeObject *getScopeObjectFromSubString(FileInformation *FI, SubString *key) {
	int index = FI->level;
	while (index >= 0) {
		linkedList_Node *current = FI->context.scopeObjects[index];
		while (current != NULL) {
			ScopeObject *binding = (ScopeObject *)current->data;
			
			if (SubString_SubString_cmp(binding->key, key) == 0) {
				return binding;
			}
			
			current = current->next;
		}
		
		index--;
	}
	
	linkedList_Node *current = coreFilePointer->context.scopeObjects[0];
	while (current != NULL) {
		ScopeObject *binding = (ScopeObject *)current->data;
		
		if (SubString_SubString_cmp(binding->key, key) == 0) {
			return binding;
		}
		
		current = current->next;
	}
	
	return NULL;
}

//
// BuilderType
//

BuilderType *BuilderType_getNewFromString(FileInformation *FI, char *string) {
	ScopeObject *scopeObject = getScopeObjectFromString(FI, string);
	if (scopeObject == NULL) abort();
	
	BuilderType *typeData = safeMalloc(sizeof(BuilderType));
	*typeData = (BuilderType){
		.scopeObject = scopeObject,
		.constraintNodes = NULL,
		.factStack = {0}
	};
	
	return typeData;
}

/// returns the resolved value of `type` or NULL
ASTnode *BuilderType_getResolvedValue(BuilderType *type, FileInformation *FI) {
	int index = 0;
	while (index <= FI->level) {
		linkedList_Node *currentFact = type->factStack[index];
		
		if (currentFact == NULL) {
			index++;
			continue;
		}
		
		while (currentFact != NULL) {
			Fact *fact = (Fact *)currentFact->data;
			
			if (fact->type == FactType_expression) {
				Fact_expression *expressionFact = (Fact_expression *)fact->value;
				
				if (expressionFact->operatorType == ASTnode_infixOperatorType_equivalent) {
					if (expressionFact->left == NULL) {
						return expressionFact->rightConstant;
					} else {
						abort();
					}
				}
			} else {
				abort();
			}
			
			currentFact = currentFact->next;
		}
		
		index++;
	}
	
	return NULL;
}

int BuilderType_hasName(BuilderType *type, char *name) {
	return SubString_string_cmp(type->scopeObject->key, name) == 0;
}

// from the core file and has name
int BuilderType_hasCoreName(BuilderType *type, char *name) {
	return type->scopeObject->originFile == coreFilePointer && BuilderType_hasName(type, name);
}

int BuilderType_isSignedInt(BuilderType *type) {
	return BuilderType_hasCoreName(type, "Int8") ||
	BuilderType_hasCoreName(type, "Int16") ||
	BuilderType_hasCoreName(type, "Int32") ||
	BuilderType_hasCoreName(type, "Int64");
}

int BuilderType_isUnsignedInt(BuilderType *type) {
	return BuilderType_hasCoreName(type, "UInt8") ||
	BuilderType_hasCoreName(type, "UInt16") ||
	BuilderType_hasCoreName(type, "UInt32") ||
	BuilderType_hasCoreName(type, "UInt64");
}

int BuilderType_isInt(BuilderType *type) {
	return BuilderType_isSignedInt(type) || BuilderType_isUnsignedInt(type);
}

int BuilderType_isFloat(BuilderType *type) {
	return BuilderType_hasCoreName(type, "Float16") ||
	BuilderType_hasCoreName(type, "Float32") ||
	BuilderType_hasCoreName(type, "Float64");
}

int BuilderType_isNumber(BuilderType *type) {
	return BuilderType_isInt(type) || BuilderType_isFloat(type);
}

char *BuilderType_getLLVMname(BuilderType *type, FileInformation *FI) {
//	CharAccumulator *LLVMname = CharAccumulator_new();
//	CharAccumulator_appendChars(LLVMname, "[ ");
//	CharAccumulator_appendInt(LLVMname, ASTnode_getIntFromNumber(resolvedSizeValue, FI));
//	CharAccumulator_appendChars(LLVMname, " x ");
//	CharAccumulator_appendChars(LLVMname, BuilderType_getLLVMname(BuilderType_getStateFromString(type, "type"), FI));
//	CharAccumulator_appendChars(LLVMname, " ]");
//	return LLVMname->data;
	
	if (type->scopeObject->scopeObjectType == ScopeObjectType_struct) {
		return ((ScopeObject_struct *)type->scopeObject->value)->LLVMname;
	} else if (type->scopeObject->scopeObjectType == ScopeObjectType_function) {
		return ((ScopeObject_function *)type->scopeObject->value)->LLVMname;
	} else if (type->scopeObject->scopeObjectType == ScopeObjectType_value) {
//		return ((ScopeObject_value *)type->scopeObject->value)->LLVMname;
		abort();
	} else {
		abort();
	}
}

int BuilderType_getByteSize(BuilderType *type) {
	if (type->scopeObject->scopeObjectType != ScopeObjectType_struct) {
		return ((ScopeObject_struct *)type->scopeObject->value)->byteSize;
	} else {
		abort();
	}
}

int BuilderType_getByteAlign(BuilderType *type) {
	if (type->scopeObject->scopeObjectType != ScopeObjectType_struct) {
		return ((ScopeObject_struct *)type->scopeObject->value)->byteAlign;
	} else {
		abort();
	}
}

SubString *BuilderType_getName(BuilderType *type) {
	return type->scopeObject->key;
}
