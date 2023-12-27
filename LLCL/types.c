#include <stdlib.h>
#include <stdio.h>

#include "types.h"
#include "report.h"
#include "utilities.h"

//
// linkedList
//

void *linkedList_addNode(linkedList_Node **head, unsigned long size) {
	linkedList_Node* newNode = calloc(1, sizeof(linkedList_Node) + size);
	if (newNode == NULL) {
		printf("calloc failed to get space for a linked list\n");
		abort();
	}
	
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
// SubString
//

SubString *SubString_new(char *start, int length) {
	SubString *subString = safeMalloc(sizeof(SubString));
	subString->start = start;
	subString->length = length;
	
	return subString;
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

//
// Facts
//

void Fact_newExpression(linkedList_Node **head, ASTnode_operatorType operatorType, ASTnode *left, ASTnode *rightConstant) {
	Fact *factData = linkedList_addNode(head, sizeof(Fact) + sizeof(Fact_expression));
	factData->type = FactType_expression;
	((Fact_expression *)factData->value)->operatorType = ASTnode_operatorType_equivalent;
	((Fact_expression *)factData->value)->left = left;
	((Fact_expression *)factData->value)->rightConstant = rightConstant;
}

//
// context
//

char *ContextBinding_getLLVMname(ContextBinding *binding) {
	if (binding->type == ContextBindingType_simpleType) {
		return ((ContextBinding_simpleType *)binding->value)->LLVMtype;
	} else if (binding->type == ContextBindingType_function) {
		return ((ContextBinding_function *)binding->value)->LLVMname;
	} else if (binding->type == ContextBindingType_struct) {
		return ((ContextBinding_struct *)binding->value)->LLVMname;
	}
	
	abort();
}

int ContextBinding_availableInOtherFile(ContextBinding *binding) {
	return binding->type != ContextBindingType_namespace;
}

int BuilderType_hasName(BuilderType *type, char *name) {
	return SubString_string_cmp(type->binding->key, name) == 0;
}

int BuilderType_isSignedInt(BuilderType *type) {
	return BuilderType_hasName(type, "Int8") ||
	BuilderType_hasName(type, "Int16") ||
	BuilderType_hasName(type, "Int32") ||
	BuilderType_hasName(type, "Int64");
}

int BuilderType_isUnsignedInt(BuilderType *type) {
	return BuilderType_hasName(type, "UInt8") ||
	BuilderType_hasName(type, "UInt16") ||
	BuilderType_hasName(type, "UInt32") ||
	BuilderType_hasName(type, "UInt64");
}

int BuilderType_isInt(BuilderType *type) {
	return BuilderType_isSignedInt(type) || BuilderType_isUnsignedInt(type);
}

int BuilderType_isFloat(BuilderType *type) {
	return BuilderType_hasName(type, "Float16") ||
	BuilderType_hasName(type, "Float32") ||
	BuilderType_hasName(type, "Float64");
}

int BuilderType_isNumber(BuilderType *type) {
	return BuilderType_isInt(type) || BuilderType_isFloat(type);
}
