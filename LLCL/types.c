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
			
			.bindings = {0},
			.importedFiles = NULL,
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

int FileInformation_declaredInLLVM(FileInformation *FI, ContextBinding *pointer) {
	linkedList_Node *currentFunction = FI->context.declaredInLLVM;
	
	while (currentFunction != NULL) {
		if (*(ContextBinding **)currentFunction->data == pointer) {
			return 1;
		}
		
		currentFunction = currentFunction->next;
	}
	
	return 0;
}

void FileInformation_addToDeclaredInLLVM(FileInformation *FI, ContextBinding *pointer) {
	ContextBinding **bindingPointer = linkedList_addNode(&FI->context.declaredInLLVM, sizeof(void *));
	*bindingPointer = pointer;
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

// from the core file and has name
int ContextBinding_hasCoreName(ContextBinding *binding, char *name) {
	return binding->originFile == coreFilePointer && SubString_string_cmp(binding->key, name) == 0;
}

int ContextBinding_availableInOtherFile(ContextBinding *binding) {
	return binding->type != ContextBindingType_namespace;
}

// note: originFile = coreFilePointer
void addContextBinding_simpleType(linkedList_Node **context, char *name, char *LLVMtype, int byteSize, int byteAlign) {
	SubString *key = safeMalloc(sizeof(SubString));
	key->start = name;
	key->length = (int)strlen(name);
	
	ContextBinding *data = linkedList_addNode(context, sizeof(ContextBinding) + sizeof(ContextBinding_simpleType));
	
	data->originFile = coreFilePointer;
	data->key = key;
	data->type = ContextBindingType_simpleType;
	data->byteSize = byteSize;
	data->byteAlign = byteAlign;
	((ContextBinding_simpleType *)data->value)->LLVMtype = LLVMtype;
}

// note: originFile = coreFilePointer
void addContextBinding_compileTimeSetting(linkedList_Node **context, char *name, char *value) {
	SubString *key = safeMalloc(sizeof(SubString));
	key->start = name;
	key->length = (int)strlen(name);
	
	ContextBinding *data = linkedList_addNode(context, sizeof(ContextBinding) + sizeof(ContextBinding_compileTimeSetting));
	
	data->originFile = coreFilePointer;
	data->key = key;
	data->type = ContextBindingType_compileTimeSetting;
	data->byteSize = 0;
	data->byteAlign = 0;
	if (value != NULL) {
		((ContextBinding_compileTimeSetting *)data->value)->value = SubString_new(value, (int)strlen(value));
	} else {
		((ContextBinding_compileTimeSetting *)data->value)->value = NULL;
	}
}

ContextBinding *getContextBindingFromString(FileInformation *FI, char *key) {
	int index = FI->level;
	while (index >= 0) {
		linkedList_Node *current = FI->context.bindings[index];
		
		while (current != NULL) {
			ContextBinding *binding = ((ContextBinding *)current->data);
			
			if (SubString_string_cmp(binding->key, key) == 0) {
				return binding;
			}
			
			current = current->next;
		}
		
		index--;
	}
	
	linkedList_Node *currentFile = FI->context.importedFiles;
	while (currentFile != NULL) {
		FileInformation *fileInformation = *(FileInformation **)currentFile->data;
		
		linkedList_Node *current = fileInformation->context.bindings[0];
		
		while (current != NULL) {
			ContextBinding *binding = ((ContextBinding *)current->data);
			
			if (ContextBinding_availableInOtherFile(binding)) {
				if (SubString_string_cmp(binding->key, key) == 0) {
					return binding;
				}
			}
			
			current = current->next;
		}
		
		currentFile = currentFile->next;
	}

	return NULL;
}

ContextBinding *getContextBindingFromSubString(FileInformation *FI, SubString *key) {
	int index = FI->level;
	while (index >= 0) {
		linkedList_Node *current = FI->context.bindings[index];
		
		while (current != NULL) {
			ContextBinding *binding = ((ContextBinding *)current->data);
			
			if (SubString_SubString_cmp(binding->key, key) == 0) {
				return binding;
			}
			
			current = current->next;
		}
		
		index--;
	}
	
	linkedList_Node *currentFile = FI->context.importedFiles;
	while (currentFile != NULL) {
		FileInformation *fileInformation = *(FileInformation **)currentFile->data;
		
		linkedList_Node *current = fileInformation->context.bindings[0];
		
		while (current != NULL) {
			ContextBinding *binding = ((ContextBinding *)current->data);
			
			if (ContextBinding_availableInOtherFile(binding)) {
				if (SubString_SubString_cmp(binding->key, key) == 0) {
					return binding;
				}
			}
			
			current = current->next;
		}
		
		currentFile = currentFile->next;
	}
	
	return NULL;
}

//
// BuilderType
//

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

BuilderType *BuilderType_getStateFromSubString(BuilderType *type, SubString *key) {
	return Dictionary_getFromSubString(type->states, key);
}

BuilderType *BuilderType_getStateFromString(BuilderType *type, char *key) {
	return Dictionary_getFromSubString(type->states, getSubStringFromString(key));
}

int BuilderType_hasName(BuilderType *type, char *name) {
	return SubString_string_cmp(type->binding->key, name) == 0;
}

// from the core file and has name
int BuilderType_hasCoreName(BuilderType *type, char *name) {
	return type->binding->originFile == coreFilePointer && BuilderType_hasName(type, name);
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
	if (type->binding->type == ContextBindingType_simpleType) {
		if (ContextBinding_hasCoreName(type->binding, "_Vector")) {
			ASTnode *resolvedSizeValue = BuilderType_getResolvedValue(BuilderType_getStateFromString(type, "size"), FI);
			if (resolvedSizeValue == NULL) abort();
			
			CharAccumulator *LLVMname = CharAccumulator_new();
			CharAccumulator_appendChars(LLVMname, "[ ");
			CharAccumulator_appendInt(LLVMname, ASTnode_getIntFromNumber(resolvedSizeValue, FI));
			CharAccumulator_appendChars(LLVMname, " x ");
			CharAccumulator_appendChars(LLVMname, BuilderType_getLLVMname(BuilderType_getStateFromString(type, "type"), FI));
			CharAccumulator_appendChars(LLVMname, " ]");
			return LLVMname->data;
		} else {
			return ((ContextBinding_simpleType *)type->binding->value)->LLVMtype;
		}
	} else if (type->binding->type == ContextBindingType_function) {
		return ((ContextBinding_function *)type->binding->value)->LLVMname;
	} else if (type->binding->type == ContextBindingType_struct) {
		return ((ContextBinding_struct *)type->binding->value)->LLVMname;
	} else {
		abort();
	}
}

int BuilderType_getByteSize(BuilderType *type) {
	if (ContextBinding_hasCoreName(type->binding, "_Vector")) {
		return BuilderType_getStateFromString(type, "type")->binding->byteSize;
	} else {
		return type->binding->byteSize;
	}
}

int BuilderType_getByteAlign(BuilderType *type) {
	if (ContextBinding_hasCoreName(type->binding, "_Vector")) {
		return BuilderType_getStateFromString(type, "type")->binding->byteAlign;
	} else {
		return type->binding->byteAlign;
	}
}
