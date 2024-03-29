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

SourceLocation SourceLocation_new(FileInformation *originFile, int line, int columnStart, int columnEnd) {
	return (SourceLocation){originFile, line, columnStart, columnEnd};
}

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

void Fact_newExpression(linkedList_Node **head, InfixOperatorType operatorType, ASTnode *left, ASTnode *rightConstant) {
	Fact *factData = linkedList_addNode(head, sizeof(Fact) + sizeof(Fact_expression));
	factData->type = FactType_expression;
	((Fact_expression *)factData->value)->operatorType = InfixOperatorType_equivalent;
	((Fact_expression *)factData->value)->left = left;
	((Fact_expression *)factData->value)->rightConstant = rightConstant;
}

//
// context
//

ScopeObject ScopeObject_new(SubString *name, SourceLocation location, Bool compileTime, Bool canInferType, linkedList_Node *constraintNodes, ScopeObject *type, ScopeObjectKind scopeObjectType) {
	return (ScopeObject){
		.name = name,
		.location = location,
		.compileTime = compileTime,
		.canInferType = canInferType,
		.type = type,
		.scopeObjectKind = scopeObjectType
	};
}

ScopeObject_alias ScopeObject_alias_new(SubString *key, ScopeObject *value) {
	return (ScopeObject_alias){
		.key = key,
		.value = value
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

void ScopeLevel_addScopeObject(ScopeLevel level, ScopeObject *scopeObject) {
	ScopeObject **newScopeObjectPtr = linkedList_addNode(level, sizeof(ScopeObject *));
	*newScopeObjectPtr = scopeObject;
}

ScopeObject *addAlias(linkedList_Node **list, SubString *key, ScopeObject *type, ScopeObject *value) {
	ScopeObject *alias = linkedList_addNode(list, sizeof(ScopeObject) + sizeof(ScopeObject_alias));
	*alias = ScopeObject_new(NULL, SourceLocation_new(coreFilePointer, 0, 0, 0), false, false, NULL, type, ScopeObjectKind_alias);
	*(ScopeObject_alias *)alias->value = ScopeObject_alias_new(key, value);
	
	return alias;
}

// returns the type that all types are
ScopeObject *getTypeType(void) {
	return scopeObject_getAsAlias(getScopeObjectAliasFromString(coreFilePointer, "Type"))->value;
}

void addBuiltinType(ScopeLevel level, char *name, char *LLVMname, int byteSize, int byteAlign) {
	ScopeObject *structScopeObject = safeMalloc(sizeof(ScopeObject) + sizeof(ScopeObject_struct));
	*structScopeObject = ScopeObject_new(getSubStringFromString(name), SourceLocation_new(coreFilePointer, 0, 0, 0), true, false, NULL, getTypeType(), ScopeObjectKind_struct);
	*(ScopeObject_struct *)structScopeObject->value = ScopeObject_struct_new(LLVMname, NULL, byteSize, byteAlign);
	
	addAlias(level, getSubStringFromString(name), getTypeType(), structScopeObject);
}

void addBuiltinFunction(ScopeLevel level, char *name, char *description) {
	ScopeObject *structScopeObject = safeMalloc(sizeof(ScopeObject) + sizeof(ScopeObject_builtinFunction));
	*structScopeObject = ScopeObject_new(getSubStringFromString(name), SourceLocation_new(coreFilePointer, 0, 0, 0), true, false, NULL, getTypeType(), ScopeObjectKind_builtinFunction);
	
	addAlias(level, getSubStringFromString(name), getTypeType(), structScopeObject);
}

ScopeObject_value ScopeObject_value_new(int LLVMRegister) {
	return (ScopeObject_value){
		.LLVMRegister = LLVMRegister,
		.factStack = {0},
	};
}

ScopeObject_value *ScopeObject_getAsValue(ScopeObject *scopeObject) {
	if (scopeObject->scopeObjectKind != ScopeObjectKind_value) abort();
	return (ScopeObject_value *)scopeObject->value;
}

ScopeObject *addScopeObjectValue(ScopeLevel level, ScopeObject *type) {
	ScopeObject *newScopeObject = safeMalloc(sizeof(ScopeObject) + sizeof(ScopeObject_value));
	*newScopeObject = ScopeObject_new(NULL, SourceLocation_new(coreFilePointer, 0, 0, 0), false, false, NULL, type, ScopeObjectKind_value);
	*(ScopeObject_value *)newScopeObject->value = ScopeObject_value_new(0);
	ScopeLevel_addScopeObject(level, newScopeObject);
	return newScopeObject;
}

ScopeObject_constrainedType ScopeObject_constrainedType_new(linkedList_Node *constraintNodes) {
	return (ScopeObject_constrainedType){
		.constraintNodes = constraintNodes,
	};
}

ScopeObject_constrainedType *ScopeObject_getAsConstrainedType(ScopeObject *scopeObject) {
	if (scopeObject->scopeObjectKind != ScopeObjectKind_constrainedType) abort();
	return (ScopeObject_constrainedType *)scopeObject->value;
}

ScopeObject *addScopeObjectConstrainedType(ScopeLevel level, linkedList_Node *constraintNodes) {
	ScopeObject *newScopeObject = safeMalloc(sizeof(ScopeObject) + sizeof(ScopeObject_constrainedType));
	*newScopeObject = ScopeObject_new(NULL, SourceLocation_new(coreFilePointer, 0, 0, 0), false, false, NULL, getTypeType(), ScopeObjectKind_constrainedType);
	*(ScopeObject_constrainedType *)newScopeObject->value = ScopeObject_constrainedType_new(constraintNodes);
	ScopeLevel_addScopeObject(level, newScopeObject);
	return newScopeObject;
}

void addScopeObjectFromString(FileInformation *FI, ScopeLevel level, char *string, ASTnode *node) {
	ScopeObject *scopeObject = getScopeObjectAliasFromString(FI, string);
	if (scopeObject == NULL) abort();
	
	ScopeObject *newScopeObject = addScopeObjectValue(level, scopeObject);
		
	Fact_newExpression(&ScopeObject_getAsValue(newScopeObject)->factStack[0], InfixOperatorType_equivalent, NULL, node);
}

ScopeObject *getScopeObjectAliasFromString(FileInformation *FI, char *key) {
	int index = FI->level;
	while (index >= 0) {
		linkedList_Node *current = FI->context.scopeObjects[index];
		while (current != NULL) {
			ScopeObject *scopeObject = (ScopeObject *)current->data;
			ScopeObject_alias *alias = scopeObject_getAsAlias(scopeObject);
			
			if (SubString_string_cmp(alias->key, key) == 0) {
				return scopeObject;
			}
			
			current = current->next;
		}
		
		index--;
	}
	
	linkedList_Node *current = coreFilePointer->context.scopeObjects[0];
	while (current != NULL) {
		ScopeObject *scopeObject = (ScopeObject *)current->data;
		ScopeObject_alias *alias = scopeObject_getAsAlias(scopeObject);
		
		if (SubString_string_cmp(alias->key, key) == 0) {
			return scopeObject;
		}
		
		current = current->next;
	}

	return NULL;
}

ScopeObject *getScopeObjectAliasFromSubString(FileInformation *FI, SubString *key) {
	int index = FI->level;
	while (index >= 0) {
		linkedList_Node *current = FI->context.scopeObjects[index];
		while (current != NULL) {
			ScopeObject *scopeObject = (ScopeObject *)current->data;
			ScopeObject_alias *alias = scopeObject_getAsAlias(scopeObject);
			
			if (SubString_SubString_cmp(alias->key, key) == 0) {
				return scopeObject;
			}
			
			current = current->next;
		}
		
		index--;
	}
	
	linkedList_Node *current = coreFilePointer->context.scopeObjects[0];
	while (current != NULL) {
		ScopeObject *scopeObject = (ScopeObject *)current->data;
		ScopeObject_alias *alias = scopeObject_getAsAlias(scopeObject);
		
		if (SubString_SubString_cmp(alias->key, key) == 0) {
			return scopeObject;
		}
		
		current = current->next;
	}
	
	return NULL;
}

/// returns the resolved value of `type` or NULL
ASTnode *ScopeObjectValue_getResolvedValue(ScopeObject *scopeObject, FileInformation *FI) {
	int index = 0;
	while (index <= FI->level) {
		linkedList_Node *currentFact = ScopeObject_getAsValue(scopeObject)->factStack[index];
		
		if (currentFact == NULL) {
			index++;
			continue;
		}
		
		while (currentFact != NULL) {
			Fact *fact = (Fact *)currentFact->data;
			
			if (fact->type == FactType_expression) {
				Fact_expression *expressionFact = (Fact_expression *)fact->value;
				
				if (expressionFact->operatorType == InfixOperatorType_equivalent) {
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

ScopeObject_alias *scopeObject_getAsAlias(ScopeObject *scopeObject) {
	if (scopeObject->scopeObjectKind != ScopeObjectKind_alias) abort();
	return (ScopeObject_alias *)scopeObject->value;
}

ScopeObject *ScopeObjectAlias_unalias(ScopeObject *scopeObject) {
	return scopeObject_getAsAlias(scopeObject)->value;
}

int ScopeObject_hasName(ScopeObject *scopeObject, char *name) {
	if (scopeObject->name == NULL) {
		printf("ScopeObject_hasName: no name\n");
		abort();
	}
	return SubString_string_cmp(scopeObject->name, name) == 0;
}

// from the core file and has name
int ScopeObject_hasCoreName(ScopeObject *scopeObject, char *name) {
	return scopeObject->location.originFile == coreFilePointer && ScopeObject_hasName(scopeObject, name);
}

int ScopeObject_isSignedInt(ScopeObject *scopeObject) {
	return ScopeObject_hasCoreName(scopeObject, "Int8") ||
	ScopeObject_hasCoreName(scopeObject, "Int16") ||
	ScopeObject_hasCoreName(scopeObject, "Int32") ||
	ScopeObject_hasCoreName(scopeObject, "Int64");
}

int ScopeObject_isUnsignedInt(ScopeObject *scopeObject) {
	return ScopeObject_hasCoreName(scopeObject, "UInt8") ||
	ScopeObject_hasCoreName(scopeObject, "UInt16") ||
	ScopeObject_hasCoreName(scopeObject, "UInt32") ||
	ScopeObject_hasCoreName(scopeObject, "UInt64");
}

int ScopeObject_isInt(ScopeObject *scopeObject) {
	return ScopeObject_isSignedInt(scopeObject) || ScopeObject_isUnsignedInt(scopeObject);
}

int ScopeObject_isFloat(ScopeObject *scopeObject) {
	return ScopeObject_hasCoreName(scopeObject, "Float16") ||
	ScopeObject_hasCoreName(scopeObject, "Float32") ||
	ScopeObject_hasCoreName(scopeObject, "Float64");
}

int ScopeObject_isNumber(ScopeObject *scopeObject) {
	return ScopeObject_isInt(scopeObject) || ScopeObject_isFloat(scopeObject);
}

char *ScopeObjectAlias_getLLVMname(ScopeObject *scopeObject, FileInformation *FI) {
//	CharAccumulator *LLVMname = CharAccumulator_new();
//	CharAccumulator_appendChars(LLVMname, "[ ");
//	CharAccumulator_appendInt(LLVMname, ASTnode_getIntFromNumber(resolvedSizeValue, FI));
//	CharAccumulator_appendChars(LLVMname, " x ");
//	CharAccumulator_appendChars(LLVMname, BuilderType_getLLVMname(BuilderType_getStateFromString(type, "type"), FI));
//	CharAccumulator_appendChars(LLVMname, " ]");
//	return LLVMname->data;
	
	ScopeObject_alias *alias = scopeObject_getAsAlias(scopeObject);
	
	if (alias->value->scopeObjectKind == ScopeObjectKind_struct) {
		return ((ScopeObject_struct *)alias->value)->LLVMname;
	} else if (alias->value->scopeObjectKind == ScopeObjectKind_function) {
		return ((ScopeObject_function *)alias->value)->LLVMname;
	} else {
		abort();
	}
}

int ScopeObjectAlias_getByteSize(ScopeObject *scopeObject) {
	ScopeObject_alias *alias = scopeObject_getAsAlias(scopeObject);
	
	if (alias->value->scopeObjectKind != ScopeObjectKind_struct) {
		return ((ScopeObject_struct *)alias->value->value)->byteSize;
	} else {
		abort();
	}
}

int ScopeObjectAlias_getByteAlign(ScopeObject *scopeObject) {
	ScopeObject_alias *alias = scopeObject_getAsAlias(scopeObject);
	
	if (alias->value->scopeObjectKind == ScopeObjectKind_struct) {
		return ((ScopeObject_struct *)alias->value->value)->byteAlign;
	} else {
		abort();
	}
}

SubString *ScopeObjectAlias_getName(ScopeObject *scopeObject) {
	ScopeObject_alias *alias = scopeObject_getAsAlias(scopeObject);
	return alias->key;
}
