/*
 This file has the buildLLVM function and a bunch of functions that the BuildLLVM function calls.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <libgen.h>
#include <dirent.h>

#include "compiler.h"
#include "globals.h"
#include "builder.h"
#include "report.h"
#include "utilities.h"
#include "printer.h"

void addDILocation(CharAccumulator *source, int ID, SourceLocation location) {
	CharAccumulator_appendChars(source, ", !dbg !DILocation(line: ");
	CharAccumulator_appendInt(source, location.line);
	CharAccumulator_appendChars(source, ", column: ");
	CharAccumulator_appendInt(source, location.columnStart + 1);
	CharAccumulator_appendChars(source, ", scope: !");
	CharAccumulator_appendInt(source, ID);
	CharAccumulator_appendChars(source, ")");
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

void addContextBinding_macro(FileInformation *FI, char *name) {
	SubString *key = safeMalloc(sizeof(SubString));
	key->start = name;
	key->length = (int)strlen(name);
	
	ContextBinding *macroData = linkedList_addNode(&FI->context.bindings[FI->level], sizeof(ContextBinding) + sizeof(ContextBinding_macro));
	
	macroData->originFile = FI;
	macroData->key = key;
	macroData->type = ContextBindingType_macro;
	// I do not think I need to set byteSize or byteAlign to anything specific
	macroData->byteSize = 0;
	macroData->byteAlign = 0;

	((ContextBinding_macro *)macroData->value)->codeBlock = NULL;
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

/// if the node is a memberAccess operator the function calls itself until it gets to the end of the memberAccess operators
ContextBinding *getContextBindingFromIdentifierNode(FileInformation *FI, ASTnode *node) {
	if (node->nodeType == ASTnodeType_identifier) {
		ASTnode_identifier *data = (ASTnode_identifier *)node->value;
		
		return getContextBindingFromSubString(FI, data->name);
	} else if (node->nodeType == ASTnodeType_operator) {
		ASTnode_operator *data = (ASTnode_operator *)node->value;
		
		if (data->operatorType != ASTnode_operatorType_memberAccess) abort();
		
		return getContextBindingFromIdentifierNode(FI, (ASTnode *)data->left->data);
	} else {
		abort();
	}
	
	return NULL;
}

// in the future this could do more than return type->binding->key
SubString *getTypeAsSubString(BuilderType *type) {
	return type->binding->key;
}

void addTypeFromString(FileInformation *FI, linkedList_Node **list, char *string, ASTnode *node) {
	ContextBinding *binding = getContextBindingFromString(FI, string);
	if (binding == NULL) abort();

	BuilderType *typeData = linkedList_addNode(list, sizeof(BuilderType));
	*typeData = (BuilderType){
		.binding = binding,
		.constraintNodes = NULL,
		.factStack = {0}
	};
	
	int level = 0;
	if (FI->level > 0) level = FI->level - 1;
	
	Fact_newExpression(&typeData->factStack[level], ASTnode_operatorType_equivalent, NULL, node);
}

void addTypeFromBuilderType(FileInformation *FI, linkedList_Node **list, BuilderType *type) {
	BuilderType *data = linkedList_addNode(list, sizeof(BuilderType));
	*data = (BuilderType){
		.binding = type->binding,
		.constraintNodes = type->constraintNodes,
		.factStack = {0}
	};
}

void addTypeFromBinding(FileInformation *FI, linkedList_Node **list, ContextBinding *binding) {
	BuilderType *data = linkedList_addNode(list, sizeof(BuilderType));
	*data = (BuilderType){
		.binding = binding,
		.constraintNodes = NULL,
		.factStack = {0}
	};
}

linkedList_Node *typeToList(BuilderType type) {
	linkedList_Node *list = NULL;
	
	BuilderType *data = linkedList_addNode(&list, sizeof(BuilderType));
	data->binding = type.binding;
	
	return list;
}

void expectUnusedName(FileInformation *FI, SubString *name, SourceLocation location) {
	ContextBinding *binding = getContextBindingFromSubString(FI, name);
	if (binding != NULL) {
		addStringToReportMsg("the name '");
		addSubStringToReportMsg(name);
		addStringToReportMsg("' is defined multiple times");
		
		addStringToReportIndicator("'");
		addSubStringToReportIndicator(name);
		addStringToReportIndicator("' redefined here");
		compileError(FI, location);
	}
}

// node is an ASTnode_constrainedType
BuilderType *getType(FileInformation *FI, ASTnode *node) {
	if (node->nodeType != ASTnodeType_constrainedType) abort();
	ASTnode_constrainedType *data = (ASTnode_constrainedType *)node->value;
	
	linkedList_Node *returnTypeList = NULL;
	buildLLVM(FI, NULL, NULL, NULL, NULL, &returnTypeList, data->type, 0, 0, 0);
	if (returnTypeList == NULL) abort();
	
	if (
		((BuilderType *)returnTypeList->data)->binding->type != ContextBindingType_simpleType &&
		((BuilderType *)returnTypeList->data)->binding->type != ContextBindingType_struct
	) {
		addStringToReportMsg("expected type");
		
		addStringToReportIndicator("'");
		addSubStringToReportIndicator(((BuilderType *)returnTypeList->data)->binding->key);
		addStringToReportIndicator("' is not a type");
		compileError(FI, node->location);
	}
	
	((BuilderType *)returnTypeList->data)->constraintNodes = data->constraints;
	
	return (BuilderType *)returnTypeList->data;
}

void applyFacts(FileInformation *FI, ASTnode_operator *operator) {
	if (operator->operatorType != ASTnode_operatorType_equivalent) return;
	if (((ASTnode *)operator->left->data)->nodeType != ASTnodeType_identifier) return;
	if (((ASTnode *)operator->right->data)->nodeType != ASTnodeType_number) return;
	
	ContextBinding *variableBinding = getContextBindingFromIdentifierNode(FI, (ASTnode *)operator->left->data);
	
	ContextBinding_variable *variable = (ContextBinding_variable *)variableBinding->value;
	
	Fact *fact = linkedList_addNode(&variable->type.factStack[FI->level], sizeof(Fact) + sizeof(Fact_expression));
	
	fact->type = FactType_expression;
	((Fact_expression *)fact->value)->operatorType = operator->operatorType;
	((Fact_expression *)fact->value)->left = NULL;
	((Fact_expression *)fact->value)->rightConstant = (ASTnode *)operator->right->data;
}

ASTnode *expectArgumentOnMacro(FileInformation *FI, linkedList_Node **currentType, linkedList_Node **currentArgument, char* typeName, int mustBeResolved) {
	BuilderType *type = (BuilderType *)(*currentType)->data;
	ASTnode *argument = (ASTnode *)(*currentArgument)->data;
	
	// make sure that type has the right name
	if (!BuilderType_hasName(type, typeName)) {
		addStringToReportMsg("unexpected type");
		
		addStringToReportIndicator("expected type '");
		addStringToReportIndicator(typeName);
		addStringToReportIndicator("' but got type '");
		addSubStringToReportIndicator(type->binding->key);
		addStringToReportIndicator("'");
		
		compileError(FI, argument->location);
	}
	
	*currentType = (*currentType)->next;
	*currentArgument = (*currentArgument)->next;
	
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
				
				if (expressionFact->operatorType == ASTnode_operatorType_equivalent) {
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
	
	if (mustBeResolved) {
		addStringToReportMsg("type must be resolved");
		
		addStringToReportIndicator("the value of this argument is not entirely known");
		
		compileError(FI, argument->location);
	} else {
		return NULL;
	}
}

SubString *getSubStringFromStringNode(FileInformation *FI, ASTnode *node) {
	if (node->nodeType != ASTnodeType_string) {
		addStringToReportMsg("must be a string");
		
		// this might not show up in the right spot
		compileError(FI, node->location);
	}
	
	return ((ASTnode_string *)node->value)->value;
}

char *getLLVMtypeFromBinding(FileInformation *FI, ContextBinding *binding) {
	if (binding->type == ContextBindingType_simpleType) {
		return ((ContextBinding_simpleType *)binding->value)->LLVMtype;
	} else if (binding->type == ContextBindingType_struct) {
		return ((ContextBinding_struct *)binding->value)->LLVMname;
	}
	
	abort();
}

BuilderType getTypeFromBinding(ContextBinding *binding) {
	return (BuilderType){binding};
}

int addOperatorResultToType(FileInformation *FI, BuilderType *type, ASTnode_operatorType operatorType, ASTnode *leftNode, ASTnode *rightNode) {
	if (FI->level <= 0) abort();
	if (leftNode->nodeType != rightNode->nodeType) return 0;
	ASTnodeType sharedNodeType = leftNode->nodeType;
	
	switch (sharedNodeType) {
		case ASTnodeType_number: {
			ASTnode_number *left = (ASTnode_number *)leftNode->value;
			ASTnode_number *right = (ASTnode_number *)rightNode->value;
			
			if (
				operatorType == ASTnode_operatorType_equivalent &&
				left->value == right->value
			) {
				ASTnode *node = safeMalloc(sizeof(ASTnode) + sizeof(ASTnode_bool));
				node->nodeType = ASTnodeType_bool;
				node->location = (SourceLocation){0};
				((ASTnode_bool *)node->value)->isTrue = 1;
				
				Fact_newExpression(&type->factStack[FI->level - 1], ASTnode_operatorType_equivalent, NULL, node);
				return 1;
			}
			return 0;
		}
		
		default: {
			return 0;
		}
	}
}

int addTypeResultAfterOperationToList(FileInformation *FI, linkedList_Node **list, char *name, ASTnode_operatorType operatorType, BuilderType *left, BuilderType *right) {
	int resultIsTrue = 0;
	
	BuilderType *type = linkedList_addNode(list, sizeof(BuilderType));
	*type = (BuilderType){
		.binding = getContextBindingFromString(FI, name),
		.constraintNodes = NULL,
		.factStack = {0}
	};
	
	int leftIndex = FI->level;
	while (leftIndex > 0) {
		linkedList_Node *leftCurrentFact = left->factStack[leftIndex];
		
		while (leftCurrentFact != NULL) {
			Fact *leftFact = (Fact *)leftCurrentFact->data;
			
			if (leftFact->type == FactType_expression) {
				Fact_expression *leftExpressionFact = (Fact_expression *)leftFact->value;
				
				int rightIndex = FI->level;
				while (rightIndex > 0) {
					linkedList_Node *rightCurrentFact = right->factStack[rightIndex];
					
					while (rightCurrentFact != NULL) {
						Fact *rightFact = (Fact *)rightCurrentFact->data;
						
						if (rightFact->type == FactType_expression) {
							Fact_expression *rightExpressionFact = (Fact_expression *)rightFact->value;
							
							if (
								leftExpressionFact->operatorType == ASTnode_operatorType_equivalent &&
								rightExpressionFact->operatorType == ASTnode_operatorType_equivalent
							) {
								int isTrue = addOperatorResultToType(FI, type, operatorType, leftExpressionFact->rightConstant, rightExpressionFact->rightConstant);
								if (isTrue) resultIsTrue = 1;
							}
						}
						
						rightCurrentFact = rightCurrentFact->next;
					}
					
					rightIndex--;
				}
			}
			
			leftCurrentFact = leftCurrentFact->next;
		}
		
		leftIndex--;
	}
	
	return resultIsTrue;
}

void expectType(FileInformation *FI, ContextBinding_variable *self, BuilderType *expectedType, BuilderType *actualType, SourceLocation location) {
	if (expectedType->binding != actualType->binding) {
		addStringToReportMsg("unexpected type");
		
		addStringToReportIndicator("expected type '");
		addSubStringToReportIndicator(expectedType->binding->key);
		addStringToReportIndicator("' but got type '");
		addSubStringToReportIndicator(actualType->binding->key);
		addStringToReportIndicator("'");
		compileError(FI, location);
	}
	
	if (expectedType->constraintNodes != NULL) {
		ASTnode *constraintExpectedNode = (ASTnode *)expectedType->constraintNodes->data;
		if (constraintExpectedNode->nodeType != ASTnodeType_operator) abort();
		ASTnode_operator *expectedData = (ASTnode_operator *)constraintExpectedNode->value;
		
		ASTnode *leftNode = (ASTnode *)expectedData->left->data;
		ASTnode *rightNode = (ASTnode *)expectedData->right->data;
		
		BuilderType *left;
		if (leftNode->nodeType == ASTnodeType_selfReference) {
			left = actualType;
		} else {
			linkedList_Node *leftTypes = NULL;
			buildLLVM(FI, NULL, NULL, NULL, NULL, &leftTypes, expectedData->left, 0, 0, 0);
			left = (BuilderType *)leftTypes->data;
		}
		
		BuilderType *right;
		if (rightNode->nodeType == ASTnodeType_selfReference) {
			right = actualType;
		} else {
			linkedList_Node *rightTypes = NULL;
			buildLLVM(FI, NULL, NULL, NULL, NULL, &rightTypes, expectedData->right, 0, 0, 0);
			right = (BuilderType *)rightTypes->data;
		}
		
		linkedList_Node *resultTypes = NULL;
		if (!addTypeResultAfterOperationToList(FI, &resultTypes, "Bool", expectedData->operatorType, left, right)) {
			addStringToReportMsg("constraint not met");
			compileError(FI, location);
		}
	}
}

ContextBinding *addFunctionToList(char *LLVMname, FileInformation *FI, linkedList_Node **list, ASTnode *node) {
	ASTnode_function *data = (ASTnode_function *)node->value;
	
	// make sure that the return type actually exists
	BuilderType* returnType = getType(FI, data->returnType);
	
	linkedList_Node *argumentTypes = NULL;
	
	// make sure that the types of all arguments actually exist
	linkedList_Node *currentArgument = data->argumentTypes;
	while (currentArgument != NULL) {
		BuilderType *currentArgumentType = getType(FI, (ASTnode *)currentArgument->data);
		
		BuilderType *data = linkedList_addNode(&argumentTypes, sizeof(BuilderType));
		*data = *currentArgumentType;
		
		currentArgument = currentArgument->next;
	}
	
	char *LLVMreturnType = getLLVMtypeFromBinding(FI, returnType->binding);
	
	ContextBinding *functionData = linkedList_addNode(list, sizeof(ContextBinding) + sizeof(ContextBinding_function));
	
	functionData->originFile = FI;
	functionData->key = data->name;
	functionData->type = ContextBindingType_function;
	// I do not think I need to set byteSize or byteAlign to anything specific
	functionData->byteSize = 0;
	functionData->byteAlign = 0;
	
	((ContextBinding_function *)functionData->value)->LLVMname = LLVMname;
	((ContextBinding_function *)functionData->value)->LLVMreturnType = LLVMreturnType;
	((ContextBinding_function *)functionData->value)->argumentNames = data->argumentNames;
	((ContextBinding_function *)functionData->value)->argumentTypes = argumentTypes;
	((ContextBinding_function *)functionData->value)->returnType = *returnType;
	((ContextBinding_function *)functionData->value)->registerCount = 0;
	
	if (compilerOptions.includeDebugInformation) {
		CharAccumulator_appendChars(FI->LLVMmetadataSource, "\n!");
		CharAccumulator_appendInt(FI->LLVMmetadataSource, FI->metadataCount);
		CharAccumulator_appendChars(FI->LLVMmetadataSource, " = distinct !DISubprogram(");
		CharAccumulator_appendChars(FI->LLVMmetadataSource, "name: \"");
		CharAccumulator_appendChars(FI->LLVMmetadataSource, LLVMname);
		CharAccumulator_appendChars(FI->LLVMmetadataSource, "\", scope: !");
		CharAccumulator_appendInt(FI->LLVMmetadataSource, FI->debugInformationFileScopeID);
		CharAccumulator_appendChars(FI->LLVMmetadataSource, ", file: !");
		CharAccumulator_appendInt(FI->LLVMmetadataSource, FI->debugInformationFileScopeID);
		// so apparently Homebrew crashes without a helpful error message if "type" is not here
		CharAccumulator_appendChars(FI->LLVMmetadataSource, ", type: !DISubroutineType(types: !{}), unit: !");
		CharAccumulator_appendInt(FI->LLVMmetadataSource, FI->debugInformationCompileUnitID);
		CharAccumulator_appendChars(FI->LLVMmetadataSource, ")");
		
		((ContextBinding_function *)functionData->value)->debugInformationScopeID = FI->metadataCount;
		
		FI->metadataCount++;
	}
	
	return functionData;
}

void generateStruct(FileInformation *FI, ContextBinding *structBinding, ASTnode *node, int defineNew) {
	ContextBinding_struct *structToGenerate = (ContextBinding_struct *)structBinding->value;
	
	CharAccumulator_appendChars(FI->topLevelStructSource, "\n\n");
	CharAccumulator_appendChars(FI->topLevelStructSource, structToGenerate->LLVMname);
	CharAccumulator_appendChars(FI->topLevelStructSource, " = type { ");
	
	int addComma = 0;
	
	if (defineNew) {
		linkedList_Node *currentPropertyNode = ((ASTnode_struct *)node->value)->block;
		while (currentPropertyNode != NULL) {
			ASTnode *propertyNode = (ASTnode *)currentPropertyNode->data;
			if (propertyNode->nodeType == ASTnodeType_queryLocation) {
				printf("[");
				printKeyword(13, "var", "");
				printBindings(FI);
				printf("]");
				exit(0);
			}
			if (propertyNode->nodeType != ASTnodeType_variableDefinition) {
				addStringToReportMsg("only variable definitions are allowed in a struct");
				compileError(FI, propertyNode->location);
			}
			ASTnode_variableDefinition *propertyData = (ASTnode_variableDefinition *)propertyNode->value;
			
			// make sure that there is not already a property in this struct with the same name
			linkedList_Node *currentPropertyBinding = structToGenerate->propertyBindings;
			while (currentPropertyBinding != NULL) {
				ContextBinding *propertyBinding = (ContextBinding *)currentPropertyBinding->data;
				
				if (SubString_SubString_cmp(propertyBinding->key, propertyData->name) == 0) {
					addStringToReportMsg("the name '");
					addSubStringToReportMsg(propertyData->name);
					addStringToReportMsg("' is defined multiple times inside a struct");
					
					addStringToReportIndicator("'");
					addSubStringToReportIndicator(propertyData->name);
					addStringToReportIndicator("' redefined here");
					compileError(FI,  propertyNode->location);
				}
				
				currentPropertyBinding = currentPropertyBinding->next;
			}
			
			// make sure the type actually exists
			BuilderType* type = getType(FI, propertyData->type);
			
			char *LLVMtype = getLLVMtypeFromBinding(FI, type->binding);
			
			// if there is a pointer anywhere in the struct then the struct should be aligned by pointer_byteSize
			if (strcmp(LLVMtype, "ptr") == 0) {
				structBinding->byteAlign = pointer_byteSize;
			}
			
			structBinding->byteSize += type->binding->byteSize;
			
			if (addComma) {
				CharAccumulator_appendChars(FI->topLevelStructSource, ", ");
			}
			CharAccumulator_appendChars(FI->topLevelStructSource, LLVMtype);
			
			ContextBinding *variableBinding = linkedList_addNode(&structToGenerate->propertyBindings, sizeof(ContextBinding) + sizeof(ContextBinding_variable));
			
			variableBinding->originFile = FI;
			variableBinding->key = propertyData->name;
			variableBinding->type = ContextBindingType_variable;
			variableBinding->byteSize = type->binding->byteSize;
			variableBinding->byteAlign = type->binding->byteSize;
			
			((ContextBinding_variable *)variableBinding->value)->LLVMRegister = 0;
			((ContextBinding_variable *)variableBinding->value)->LLVMtype = LLVMtype;
			((ContextBinding_variable *)variableBinding->value)->type = *type;
			
			addComma = 1;
			
			currentPropertyNode = currentPropertyNode->next;
		}
	}
	
	else {
		linkedList_Node *currentPropertyBinding = structToGenerate->propertyBindings;
		while (currentPropertyBinding != NULL) {
			ContextBinding *propertyBinding = (ContextBinding *)currentPropertyBinding->data;
			if (propertyBinding->type != ContextBindingType_variable) abort();
			ContextBinding_variable *variable = (ContextBinding_variable *)propertyBinding->value;
			
			if (addComma) {
				CharAccumulator_appendChars(FI->topLevelStructSource, ", ");
			}
			CharAccumulator_appendChars(FI->topLevelStructSource, variable->LLVMtype);
			
			addComma = 1;
			
			currentPropertyBinding = currentPropertyBinding->next;
		}
		
		FileInformation_addToDeclaredInLLVM(FI, structBinding);
	}
	
	CharAccumulator_appendChars(FI->topLevelStructSource, " }");
}

void generateType(FileInformation *FI, CharAccumulator *source, BuilderType *type) {
	if (type->binding->type == ContextBindingType_struct) {
		if (FI != type->binding->originFile && !FileInformation_declaredInLLVM(FI, type->binding)) {
			generateStruct(FI, type->binding, NULL, 0);
		}
	}
	CharAccumulator_appendChars(source, getLLVMtypeFromBinding(FI, type->binding));
}

void generateFunction(FileInformation *FI, CharAccumulator *outerSource, ContextBinding *functionBinding, ASTnode *node, int defineNew) {
	ContextBinding_function *function = (ContextBinding_function *)functionBinding->value;
	
	if (defineNew) {
		ASTnode_function *data = (ASTnode_function *)node->value;
		
		if (data->external) {
			CharAccumulator_appendChars(outerSource, "\n\ndeclare ");
		} else {
			CharAccumulator_appendChars(outerSource, "\n\ndefine ");
		}
	} else {
		CharAccumulator_appendChars(outerSource, "\n\ndeclare ");
	}
	
	CharAccumulator_appendChars(outerSource, function->LLVMreturnType);
	CharAccumulator_appendChars(outerSource, " @\"");
	CharAccumulator_appendChars(outerSource, function->LLVMname);
	CharAccumulator_appendChars(outerSource, "\"(");
	
	CharAccumulator functionSource = {100, 0, 0};
	CharAccumulator_initialize(&functionSource);
	
	linkedList_Node *currentArgumentType = function->argumentTypes;
	linkedList_Node *currentArgumentName = function->argumentNames;
	if (currentArgumentType != NULL) {
		int argumentCount =  linkedList_getCount(&function->argumentTypes);
		while (1) {
			ContextBinding *argumentTypeBinding = ((BuilderType *)currentArgumentType->data)->binding;
			
			char *currentArgumentLLVMtype = getLLVMtypeFromBinding(FI, ((BuilderType *)currentArgumentType->data)->binding);
			CharAccumulator_appendChars(outerSource, currentArgumentLLVMtype);
			
			if (defineNew) {
				CharAccumulator_appendChars(outerSource, " %");
				CharAccumulator_appendInt(outerSource, function->registerCount);
				
				CharAccumulator_appendChars(&functionSource, "\n\t%");
				CharAccumulator_appendInt(&functionSource, function->registerCount + argumentCount + 1);
				CharAccumulator_appendChars(&functionSource, " = alloca ");
				CharAccumulator_appendChars(&functionSource, currentArgumentLLVMtype);
				CharAccumulator_appendChars(&functionSource, ", align ");
				CharAccumulator_appendInt(&functionSource, argumentTypeBinding->byteAlign);
				
				CharAccumulator_appendChars(&functionSource, "\n\tstore ");
				CharAccumulator_appendChars(&functionSource, currentArgumentLLVMtype);
				CharAccumulator_appendChars(&functionSource, " %");
				CharAccumulator_appendInt(&functionSource, function->registerCount);
				CharAccumulator_appendChars(&functionSource, ", ptr %");
				CharAccumulator_appendInt(&functionSource, function->registerCount + argumentCount + 1);
				CharAccumulator_appendChars(&functionSource, ", align ");
				CharAccumulator_appendInt(&functionSource, argumentTypeBinding->byteAlign);
				
				// TODO: hack with FI->level to include bindings at FI->level + 1 in expectUnusedName (for arguments with the same name)
				FI->level++;
				expectUnusedName(FI, (SubString *)currentArgumentName->data, node->location);
				FI->level--;
				
				BuilderType type = *(BuilderType *)currentArgumentType->data;
				
				ContextBinding *argumentVariableData = linkedList_addNode(&FI->context.bindings[FI->level + 1], sizeof(ContextBinding) + sizeof(ContextBinding_variable));
				
				argumentVariableData->originFile = FI;
				argumentVariableData->key = (SubString *)currentArgumentName->data;
				argumentVariableData->type = ContextBindingType_variable;
				argumentVariableData->byteSize = argumentTypeBinding->byteSize;
				argumentVariableData->byteAlign = argumentTypeBinding->byteAlign;
				
				((ContextBinding_variable *)argumentVariableData->value)->LLVMRegister = function->registerCount + argumentCount + 1;
				((ContextBinding_variable *)argumentVariableData->value)->LLVMtype = currentArgumentLLVMtype;
				((ContextBinding_variable *)argumentVariableData->value)->type = type;
				
				function->registerCount++;
			}
			
			if (currentArgumentType->next == NULL) {
				break;
			}
			
			CharAccumulator_appendChars(outerSource, ", ");
			currentArgumentType = currentArgumentType->next;
			currentArgumentName = currentArgumentName->next;
		}
		
		if (defineNew) function->registerCount += argumentCount;
	}
	
	CharAccumulator_appendChars(outerSource, ")");
	
	if (defineNew) {
		ASTnode_function *data = (ASTnode_function *)node->value;
		
		if (!data->external) {
			function->registerCount++;
			
			if (compilerOptions.includeDebugInformation) {
				CharAccumulator_appendChars(outerSource, " !dbg !");
				CharAccumulator_appendInt(outerSource, function->debugInformationScopeID);
			}
			CharAccumulator_appendChars(outerSource, " {");
			CharAccumulator_appendChars(outerSource, functionSource.data);
			int functionHasReturned = buildLLVM(FI, function, outerSource, NULL, NULL, NULL, data->codeBlock, 0, 0, 0);
			
			if (!functionHasReturned) {
				addStringToReportMsg("function did not return");
				
				addStringToReportIndicator("the compiler cannot guarantee that function '");
				addSubStringToReportIndicator(data->name);
				addStringToReportIndicator("' returns");
				compileError(FI, node->location);
			}
			
			CharAccumulator_appendChars(outerSource, "\n}");
		}
	} else {
		FileInformation_addToDeclaredInLLVM(FI, functionBinding);
	}
	
	CharAccumulator_free(&functionSource);
}

FileInformation *importFile(FileInformation *currentFI, CharAccumulator *outerSource, char *path) {
	char* fullpath = realpath(path, NULL);
	
	linkedList_Node *currentFile = alreadyCompiledFiles;
	while (currentFile != NULL) {
		FileInformation *fileInformation = *(FileInformation **)currentFile->data;
		if (strcmp(fileInformation->context.path, fullpath) == 0) {
			if (compilerOptions.verbose) {
				printf("file already compiled: %s\n", fullpath);
			}
			return fileInformation;
		}
		currentFile = currentFile->next;
	}
	
	CharAccumulator *topLevelStructSource = safeMalloc(sizeof(CharAccumulator));
	(*topLevelStructSource) = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(topLevelStructSource);
	
	CharAccumulator *topLevelConstantSource = safeMalloc(sizeof(CharAccumulator));
	(*topLevelConstantSource) = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(topLevelConstantSource);
	
	CharAccumulator *topLevelFunctionSource = safeMalloc(sizeof(CharAccumulator));
	(*topLevelFunctionSource) = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(topLevelFunctionSource);
	
	CharAccumulator *LLVMmetadataSource = safeMalloc(sizeof(CharAccumulator));
	(*LLVMmetadataSource) = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(LLVMmetadataSource);
	
	FileInformation *newFI = FileInformation_new(fullpath, topLevelStructSource, topLevelConstantSource, topLevelFunctionSource, LLVMmetadataSource);
	compileFile(newFI);
	
	return newFI;
}

int buildLLVM(FileInformation *FI, ContextBinding_function *outerFunction, CharAccumulator *outerSource, CharAccumulator *innerSource, linkedList_Node *expectedTypes, linkedList_Node **types, linkedList_Node *current, int loadVariables, int withTypes, int withCommas) {
	FI->level++;
	if (FI->level > maxContextLevel) {
		printf("level (%i) > maxContextLevel (%i)\n", FI->level, maxContextLevel);
		abort();
	}
	
	linkedList_Node **currentExpectedType = NULL;
	if (expectedTypes != NULL) {
		currentExpectedType = &expectedTypes;
	}
	
	int hasReturned = 0;
	
	//
	// main loop
	//
	
	linkedList_Node *mainLoopCurrent = current;
	while (mainLoopCurrent != NULL) {
		ASTnode *node = ((ASTnode *)mainLoopCurrent->data);
		
		switch (node->nodeType) {
			case ASTnodeType_queryLocation: {
				printf("[");
				printKeywords(FI);
				printBindings(FI);
				printf("]");
				exit(0);
				break;
			}
			
			case ASTnodeType_import: {
				if (FI->level != 0) {
					addStringToReportMsg("import statements are only allowed at top level");
					compileError(FI, node->location);
				}
				
				ASTnode_import *data = (ASTnode_import *)node->value;
				
				if (data->path->length == 0) {
					addStringToReportMsg("empty import path");
					addStringToReportIndicator("import statements require a path");
					compileError(FI, node->location);
				}
				
				// TODO: hack to make "~" work on macOS
				char *path;
				if (data->path->start[0] == '~') {
					char *homePath = getenv("HOME");
					int pathSize = (int)strlen(homePath) + data->path->length;
					path = safeMalloc(pathSize);
					snprintf(path, pathSize, "%.*s%s", (int)strlen(homePath), homePath, data->path->start + 1);
				} else {
					char *dirPath = dirname(FI->context.path);
					int pathSize = (int)strlen(dirPath) + 1 + data->path->length + 1;
					path = safeMalloc(pathSize);
					snprintf(path, pathSize, "%.*s/%s", (int)strlen(dirPath), dirPath, data->path->start);
				}
				
				char *suffix = ".llcl";
				if (strncmp(data->path->start + data->path->length - strlen(suffix), suffix, strlen(suffix)) == 0) {
					// if the path ends with ".llcl" import the file and add it to FI->context.importedFiles
					FileInformation **filePointerData = linkedList_addNode(&FI->context.importedFiles, sizeof(void *));
					*filePointerData = importFile(FI, outerSource, path);
				} else {
					// is 1024 enough?
					char filePath[1024] = {0};
					snprintf(filePath, sizeof(filePath), "%s", path);
					
					// open the directory
					DIR *d = opendir(path);
					if (d == NULL) {
						addStringToReportMsg("could not open directory at '");
						addStringToReportMsg(path);
						addStringToReportMsg("'");
						compileError(FI, node->location);
						return 0;
					}
					
					ContextBinding *namespaceBinding = linkedList_addNode(&FI->context.bindings[FI->level], sizeof(ContextBinding) + sizeof(ContextBinding_namespace));
					
					namespaceBinding->originFile = FI;
					
					char *character = path + strlen(path) - 1;
					while (character > path && *character != '/') character--;
					namespaceBinding->key = SubString_new(character + 1, (int)(path + strlen(path) - character - 1));
					
					namespaceBinding->type = ContextBindingType_namespace;
					namespaceBinding->byteSize = 0;
					namespaceBinding->byteAlign = 0;
					
					struct dirent *dir;
					while ((dir = readdir(d)) != NULL) {
						if (*dir->d_name == '.') {
							continue;
						}
						
						snprintf(filePath + strlen(path), sizeof(filePath) - dir->d_namlen, "/%s", dir->d_name);
						
						FileInformation **filePointerData = linkedList_addNode(&((ContextBinding_namespace *)namespaceBinding->value)->files, sizeof(void *));
						*filePointerData = importFile(FI, outerSource, filePath);
					}
					
					// close the directory
					closedir(d);
				}
				
				break;
			}
			
			case ASTnodeType_constrainedType: {
				abort();
			}
			
			case ASTnodeType_struct: {
				if (FI->level != 0) {
					addStringToReportMsg("struct definitions are only allowed at top level");
					compileError(FI, node->location);
				}
				
				ASTnode_struct *data = (ASTnode_struct *)node->value;
				
				ContextBinding *structBinding = linkedList_addNode(&FI->context.bindings[FI->level], sizeof(ContextBinding) + sizeof(ContextBinding_struct));
				
				structBinding->originFile = FI;
				structBinding->key = data->name;
				structBinding->type = ContextBindingType_struct;
				structBinding->byteSize = 0;
				// 4 by default but set it to pointer_byteSize if there is a pointer is in the struct
				structBinding->byteAlign = 4;
				
				int strlength = strlen("%struct.");
				
				char *LLVMname = safeMalloc(strlength + data->name->length + 1);
				memcpy(LLVMname, "%struct.", strlength);
				memcpy(LLVMname + strlength, data->name->start, data->name->length);
				LLVMname[strlength + data->name->length] = 0;
				
				((ContextBinding_struct *)structBinding->value)->LLVMname = LLVMname;
				((ContextBinding_struct *)structBinding->value)->propertyBindings = NULL;
				((ContextBinding_struct *)structBinding->value)->methodBindings = NULL;
				
				generateStruct(FI, structBinding, node, 1);
				
				break;
			}
			
			case ASTnodeType_implement: {
				if (FI->level != 0) {
					addStringToReportMsg("implement can only be used at top level");
					compileError(FI, node->location);
				}
				
				ASTnode_implement *data = (ASTnode_implement *)node->value;
				
				// make sure the thing actually exists
				ContextBinding *bindingToImplement = NULL;
				int index = FI->level;
				while (1) {
					if (index < 0) {
						addStringToReportMsg("implement requires an identifier from this module");
						
						addStringToReportIndicator("nothing named '");
						addSubStringToReportIndicator(data->name);
						addStringToReportIndicator("'");
						compileError(FI, node->location);
					}
					linkedList_Node *current = FI->context.bindings[index];
					
					while (current != NULL) {
						ContextBinding *binding = ((ContextBinding *)current->data);
						
						if (SubString_SubString_cmp(binding->key, data->name) == 0) {
							bindingToImplement = binding;
							break;
						}
						
						current = current->next;
					}
					if (bindingToImplement != NULL) break;
					
					index--;
				}
				
				linkedList_Node *currentMethod = data->block;
				while (1) {
					if (currentMethod == NULL) {
						break;
					}
					
					ASTnode *methodNode = (ASTnode *)currentMethod->data;
					if (methodNode->nodeType != ASTnodeType_function) abort();
					ASTnode_function *methodData = (ASTnode_function *)methodNode->value;
					
					if (bindingToImplement->type == ContextBindingType_struct) {
						ContextBinding_struct *structToImplement = (ContextBinding_struct *)bindingToImplement->value;
						
						int LLVMnameSize = bindingToImplement->key->length + 1 + methodData->name->length + 1;
						char *LLVMname = safeMalloc(LLVMnameSize);
						snprintf(LLVMname, LLVMnameSize, "%.*s.%s", bindingToImplement->key->length, bindingToImplement->key->start, methodData->name->start);
						
						generateFunction(FI, FI->topLevelFunctionSource, addFunctionToList(LLVMname, FI, &structToImplement->methodBindings, methodNode), methodNode, 1);
					} else {
						abort();
					}
					currentMethod = currentMethod->next;
				}
				
				break;
			}
			
			case ASTnodeType_function: {
				if (FI->level != 0) {
					addStringToReportMsg("function definitions are only allowed at top level");
					compileError(FI, node->location);
				}
				
				ASTnode_function *data = (ASTnode_function *)node->value;
				
				// make sure that the name is not already used
				expectUnusedName(FI, ((ASTnode_function *)node->value)->name, node->location);
				
				char *LLVMname = NULL;
				
				ContextBinding *functionSymbolBinding = getContextBindingFromString(FI, "__functionSymbol");
				
				if (
					functionSymbolBinding != NULL &&
					functionSymbolBinding->type == ContextBindingType_compileTimeSetting &&
					((ContextBinding_compileTimeSetting *)functionSymbolBinding->value)->value != NULL
				) {
					SubString *functionSymbol = ((ContextBinding_compileTimeSetting *)functionSymbolBinding->value)->value;
					int LLVMnameSize = functionSymbol->length + 1;
					LLVMname = safeMalloc(LLVMnameSize);
					snprintf(LLVMname, LLVMnameSize, "%s", functionSymbol->start);
					
					((ContextBinding_compileTimeSetting *)functionSymbolBinding->value)->value = NULL;
				} else {
					ContextBinding *symbolManglingBinding = getContextBindingFromString(FI, "core.symbolMangling");
					
					if (SubString_string_cmp(((ContextBinding_compileTimeSetting *)symbolManglingBinding->value)->value, "true") == 0) {
						int LLVMnameSize = 1 + data->name->length + 1;
						LLVMname = safeMalloc(LLVMnameSize);
						snprintf(LLVMname, LLVMnameSize, "%d%s", FI->ID, data->name->start);
					} else {
						int LLVMnameSize = data->name->length + 1;
						LLVMname = safeMalloc(LLVMnameSize);
						snprintf(LLVMname, LLVMnameSize, "%s", data->name->start);
					}
				}
				
				addFunctionToList(LLVMname, FI, &FI->context.bindings[FI->level], node);
				
				break;
			}
			
			case ASTnodeType_call: {
				if (outerFunction == NULL) {
					addStringToReportMsg("function calls are only allowed in a function");
					compileError(FI, node->location);
				}
				
				ASTnode_call *data = (ASTnode_call *)node->value;
				
				CharAccumulator leftSource = {100, 0, 0};
				CharAccumulator_initialize(&leftSource);
				
				linkedList_Node *expectedTypesForCall = NULL;
				addTypeFromString(FI, &expectedTypesForCall, "Function", NULL);
				
				linkedList_Node *leftTypes = NULL;
				buildLLVM(FI, outerFunction, outerSource, &leftSource, expectedTypesForCall, &leftTypes, data->left, 1, 0, 0);
				
				ContextBinding *functionToCallBinding = ((BuilderType *)leftTypes->data)->binding;
				ContextBinding_function *functionToCall = (ContextBinding_function *)functionToCallBinding->value;
				
				int expectedArgumentCount = linkedList_getCount(&functionToCall->argumentTypes);
				int actualArgumentCount = linkedList_getCount(&data->arguments);
				
				if (expectedArgumentCount > actualArgumentCount) {
					SubString_print(functionToCallBinding->key);
					printf(" did not get enough arguments (expected %d but got %d)\n", expectedArgumentCount, actualArgumentCount);
					compileError(FI, node->location);
				}
				if (expectedArgumentCount < actualArgumentCount) {
					SubString_print(functionToCallBinding->key);
					printf(" got too many arguments (expected %d but got %d)\n", expectedArgumentCount, actualArgumentCount);
					compileError(FI, node->location);
				}
				
				if (expectedTypes != NULL) {
					expectType(FI, NULL, (BuilderType *)expectedTypes->data, &functionToCall->returnType, node->location);
				}
				
				if (types != NULL) {
					addTypeFromBuilderType(FI, types, &functionToCall->returnType);
				}
				
				// TODO: move this out of ASTnodeType_call
				if (FI != functionToCallBinding->originFile && !FileInformation_declaredInLLVM(FI, functionToCallBinding)) {
					generateFunction(FI, FI->topLevelFunctionSource, functionToCallBinding, node, 0);
				}
				
				CharAccumulator newInnerSource = {100, 0, 0};
				CharAccumulator_initialize(&newInnerSource);
				
				buildLLVM(FI, outerFunction, outerSource, &newInnerSource, functionToCall->argumentTypes, NULL, data->arguments, 1, 1, 1);
				
				if (outerSource != NULL) {
					if (strcmp(functionToCall->LLVMreturnType, "void") != 0) {
						CharAccumulator_appendChars(outerSource, "\n\t%");
						CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
						CharAccumulator_appendChars(outerSource, " = call ");
						
						if (innerSource != NULL) {
							if (withTypes) {
								CharAccumulator_appendChars(innerSource, functionToCall->LLVMreturnType);
								CharAccumulator_appendChars(innerSource, " ");
							}
							CharAccumulator_appendChars(innerSource, "%");
							CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
						}
						
						outerFunction->registerCount++;
					} else {
						CharAccumulator_appendChars(outerSource, "\n\tcall ");
					}
					CharAccumulator_appendChars(outerSource, functionToCall->LLVMreturnType);
					CharAccumulator_appendChars(outerSource, " @\"");
					CharAccumulator_appendChars(outerSource, functionToCall->LLVMname);
					CharAccumulator_appendChars(outerSource, "\"(");
					CharAccumulator_appendChars(outerSource, newInnerSource.data);
					CharAccumulator_appendChars(outerSource, ")");
					
					if (compilerOptions.includeDebugInformation) {
						addDILocation(outerSource, outerFunction->debugInformationScopeID, node->location);
					}
				}
				
				CharAccumulator_free(&newInnerSource);
				
				CharAccumulator_free(&leftSource);
				
				break;
			}
			
			case ASTnodeType_macro: {
				ASTnode_macro *data = (ASTnode_macro *)node->value;
				
				// make sure that the name is not already used
				expectUnusedName(FI, data->name, node->location);
				
				ContextBinding *macroData = linkedList_addNode(&FI->context.bindings[FI->level], sizeof(ContextBinding) + sizeof(ContextBinding_macro));
				
				macroData->originFile = FI;
				macroData->key = data->name;
				macroData->type = ContextBindingType_macro;
				// I do not think I need to set byteSize or byteAlign to anything specific
				macroData->byteSize = 0;
				macroData->byteAlign = 0;
				
				((ContextBinding_macro *)macroData->value)->codeBlock = data->codeBlock;
				break;
			}
			
			case ASTnodeType_runMacro: {
				ASTnode_runMacro *data = (ASTnode_runMacro *)node->value;
				
				linkedList_Node *expectedTypesForRunMacro = NULL;
				addTypeFromString(FI, &expectedTypesForRunMacro, "__Macro", NULL);
				
				linkedList_Node *leftTypes = NULL;
				buildLLVM(FI, outerFunction, outerSource, NULL, expectedTypesForRunMacro, &leftTypes, data->left, 1, 0, 0);
				
				ContextBinding *macroToRunBinding = ((BuilderType *)leftTypes->data)->binding;
				if (macroToRunBinding == NULL) {
					addStringToReportMsg("macro does not exist");
					compileError(FI, node->location);
				}
				ContextBinding_macro *macroToRun = (ContextBinding_macro *)macroToRunBinding->value;
				
				linkedList_Node *types = NULL;
				buildLLVM(FI, outerFunction, outerSource, NULL, NULL, &types, data->arguments, 0, 0, 0);
				int typeCount = linkedList_getCount(&types);
				
				if (macroToRunBinding->originFile == coreFilePointer) {
					// the macro is from the __core__ module, so this is a special case
					
					linkedList_Node **currentType = &types;
					linkedList_Node **currentArgument = &data->arguments;
					
					if (SubString_string_cmp(macroToRunBinding->key, "error") == 0) {
						if (typeCount != 2) {
							addStringToReportMsg("#error(message:String, indicator:String) expected 2 arguments");
							compileError(FI, node->location);
						}
						
						ASTnode *message = (ASTnode *)data->arguments->data;
						if (message->nodeType != ASTnodeType_string) {
							addStringToReportMsg("message must be a string");
							compileError(FI, message->location);
						}
						
						ASTnode *indicator = (ASTnode *)data->arguments->next->data;
						if (indicator->nodeType != ASTnodeType_string) {
							addStringToReportMsg("indicator must be a string");
							compileError(FI, indicator->location);
						}
						
						addSubStringToReportMsg(((ASTnode_string *)message->value)->value);
						addSubStringToReportIndicator(((ASTnode_string *)indicator->value)->value);
						compileError(FI, node->location);
					}
					
					else if (SubString_string_cmp(macroToRunBinding->key, "warning") == 0) {
						if (typeCount != 2) {
							addStringToReportMsg("#warning(message:String, indicator:String) expected 2 arguments");
							compileError(FI, node->location);
						}
						
						ASTnode *message = (ASTnode *)data->arguments->data;
						if (message->nodeType != ASTnodeType_string) {
							addStringToReportMsg("message must be a string");
							compileError(FI, message->location);
						}
						
						ASTnode *indicator = (ASTnode *)data->arguments->next->data;
						if (indicator->nodeType != ASTnodeType_string) {
							addStringToReportMsg("indicator must be a string");
							compileError(FI, indicator->location);
						}
						
						addSubStringToReportMsg(((ASTnode_string *)message->value)->value);
						addSubStringToReportIndicator(((ASTnode_string *)indicator->value)->value);
						compileWarning(FI, node->location, WarningType_other);
					}
					
					else if (SubString_string_cmp(macroToRunBinding->key, "describe") == 0) {
						if (typeCount != 1) {
							addStringToReportMsg("#describe(variable) expected 1 argument");
							compileError(FI, node->location);
						}
						
						if (compilerMode != CompilerMode_query) {
							ContextBinding *variableBinding = getContextBindingFromIdentifierNode(FI, (ASTnode *)data->arguments->data);
							
							CharAccumulator variableDescription = {100, 0, 0};
							CharAccumulator_initialize(&variableDescription);
							
							getVariableDescription(FI, &variableDescription, variableBinding);
							
							printf("%.*s", (int)variableDescription.size, variableDescription.data);
							
							CharAccumulator_free(&variableDescription);
						}
					}
					
					else if (SubString_string_cmp(macroToRunBinding->key, "insertLLVMIR") == 0) {
						if (typeCount != 1) {
							addStringToReportMsg("#insertLLVMIR(LLVFIR:String) expected 1 argument");
							compileError(FI, node->location);
						}
						
						expectArgumentOnMacro(FI, currentType, currentArgument, "Pointer", 1);
					}
					
					else if (SubString_string_cmp(macroToRunBinding->key, "set") == 0) {
						if (typeCount != 2) {
							addStringToReportMsg("#set(key:String, value:String) expected 2 arguments");
							compileError(FI, node->location);
						}
						
						SubString *key = getSubStringFromStringNode(
							FI,
							expectArgumentOnMacro(FI, currentType, currentArgument, "Pointer", 1)
						);
						
						SubString *value = getSubStringFromStringNode(
							FI,
							expectArgumentOnMacro(FI, currentType, currentArgument, "Pointer", 1)
						);
						
						ContextBinding *bindingToSet = getContextBindingFromSubString(FI, key);
						if (bindingToSet == NULL) {
							addStringToReportMsg("nothing to set named '");
							addSubStringToReportMsg(key);
							addStringToReportMsg("'");
							compileError(FI, node->location);
						}
						
						if (bindingToSet->type != ContextBindingType_compileTimeSetting) {
							addStringToReportMsg("'");
							addSubStringToReportMsg(key);
							addStringToReportMsg("' is not a compile time setting");
							compileError(FI, node->location);
						}
						
						((ContextBinding_compileTimeSetting *)bindingToSet->value)->value = value;
					}
					
					else {
						abort();
					}
				} else {
					// TODO: remove hack?
					FileContext originalFileContext = FI->context;
					FI->context = macroToRunBinding->originFile->context;
					buildLLVM(FI, outerFunction, outerSource, NULL, NULL, NULL, macroToRun->codeBlock, 0, 0, 0);
					FI->context = originalFileContext;
				}
				break;
			}
				
			case ASTnodeType_while: {
				if (outerFunction == NULL) {
					addStringToReportMsg("while loops are only allowed in a function");
					compileError(FI, node->location);
				}
				
				ASTnode_while *data = (ASTnode_while *)node->value;
				
				linkedList_Node *expectedTypesForWhile = NULL;
				addTypeFromString(FI, &expectedTypesForWhile, "Bool", NULL);
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				int jump1 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator jump1_outerSource = {100, 0, 0};
				CharAccumulator_initialize(&jump1_outerSource);
				buildLLVM(FI, outerFunction, &jump1_outerSource, &expressionSource, expectedTypesForWhile, NULL, data->expression, 1, 1, 0);
				
				int jump2 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator jump2_outerSource = {100, 0, 0};
				CharAccumulator_initialize(&jump2_outerSource);
				buildLLVM(FI, outerFunction, &jump2_outerSource, NULL, NULL, NULL, data->codeBlock, 0, 0, 0);
				
				int endJump = outerFunction->registerCount;
				outerFunction->registerCount++;
				
				CharAccumulator_appendChars(outerSource, "\n\tbr label %");
				CharAccumulator_appendInt(outerSource, jump1);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendInt(outerSource, jump1);
				CharAccumulator_appendChars(outerSource, ":");
				CharAccumulator_appendChars(outerSource, jump1_outerSource.data);
				CharAccumulator_appendChars(outerSource, "\n\tbr ");
				CharAccumulator_appendChars(outerSource, expressionSource.data);
				CharAccumulator_appendChars(outerSource, ", label %");
				CharAccumulator_appendInt(outerSource, jump2);
				CharAccumulator_appendChars(outerSource, ", label %");
				CharAccumulator_appendInt(outerSource, endJump);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendInt(outerSource, jump2);
				CharAccumulator_appendChars(outerSource, ":");
				CharAccumulator_appendChars(outerSource, jump2_outerSource.data);
				CharAccumulator_appendChars(outerSource, "\n\tbr label %");
				CharAccumulator_appendInt(outerSource, jump1);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendInt(outerSource, endJump);
				CharAccumulator_appendChars(outerSource, ":");
				
				CharAccumulator_free(&expressionSource);
				CharAccumulator_free(&jump1_outerSource);
				CharAccumulator_free(&jump2_outerSource);
				
				break;
			}
			
			case ASTnodeType_if: {
				if (outerFunction == NULL) {
					addStringToReportMsg("if statements are only allowed in a function");
					compileError(FI, node->location);
				}
				
				ASTnode_if *data = (ASTnode_if *)node->value;
				
				linkedList_Node *expectedTypesForIf = NULL;
				addTypeFromString(FI, &expectedTypesForIf, "Bool", NULL);
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				linkedList_Node *expressionTypes = NULL;
				buildLLVM(FI, outerFunction, outerSource, &expressionSource, expectedTypesForIf, &expressionTypes, data->expression, 1, 1, 0);
				
				int typesCount = linkedList_getCount(&expressionTypes);
				
				if (typesCount == 0) {
					addStringToReportMsg("if statement condition expected a bool but got nothing");
					
					if (data->expression == NULL) {
						addStringToReportIndicator("if statement condition is empty");
					} else if (((ASTnode_operator *)((ASTnode *)data->expression->data)->value)->operatorType == ASTnode_operatorType_assignment) {
						addStringToReportIndicator("did you mean to use two equals?");
					}
					
					compileError(FI, node->location);
				} else if (typesCount > 1) {
					addStringToReportMsg("if statement condition got more than one type");
					
					compileError(FI, node->location);
				}
				
				if (((ASTnode *)data->expression->data)->nodeType == ASTnodeType_operator) {
					applyFacts(FI, (ASTnode_operator *)((ASTnode *)data->expression->data)->value);
				}
				
				int jump1 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator trueCodeBlockSource = {100, 0, 0};
				CharAccumulator_initialize(&trueCodeBlockSource);
				int trueHasReturned = buildLLVM(FI, outerFunction, &trueCodeBlockSource, NULL, NULL, NULL, data->trueCodeBlock, 0, 0, 0);
				CharAccumulator falseCodeBlockSource = {100, 0, 0};
				CharAccumulator_initialize(&falseCodeBlockSource);
				
				int endJump;
				
				if (data->falseCodeBlock == NULL) {
					if (data->trueCodeBlock == NULL) {
						addStringToReportMsg("empty if statement");
						
						compileWarning(FI, node->location, WarningType_unused);
					}
					
					endJump = outerFunction->registerCount;
					outerFunction->registerCount++;
					
					CharAccumulator_appendChars(outerSource, "\n\tbr ");
					CharAccumulator_appendChars(outerSource, expressionSource.data);
					CharAccumulator_appendChars(outerSource, ", label %");
					CharAccumulator_appendInt(outerSource, jump1);
					CharAccumulator_appendChars(outerSource, ", label %");
					CharAccumulator_appendInt(outerSource, endJump);
					
					CharAccumulator_appendChars(outerSource, "\n\n");
					CharAccumulator_appendInt(outerSource, jump1);
					CharAccumulator_appendChars(outerSource, ":");
					CharAccumulator_appendChars(outerSource, trueCodeBlockSource.data);
					CharAccumulator_appendChars(outerSource, "\n\tbr label %");
					CharAccumulator_appendInt(outerSource, endJump);
				} else {
					int jump2 = outerFunction->registerCount;
					outerFunction->registerCount++;
					int falseHasReturned = buildLLVM(FI, outerFunction, &falseCodeBlockSource, NULL, NULL, NULL, data->falseCodeBlock, 0, 0, 0);
					
					endJump = outerFunction->registerCount;
					outerFunction->registerCount++;
					
					CharAccumulator_appendChars(outerSource, "\n\tbr ");
					CharAccumulator_appendChars(outerSource, expressionSource.data);
					CharAccumulator_appendChars(outerSource, ", label %");
					CharAccumulator_appendInt(outerSource, jump1);
					CharAccumulator_appendChars(outerSource, ", label %");
					CharAccumulator_appendInt(outerSource, jump2);
					
					CharAccumulator_appendChars(outerSource, "\n\n");
					CharAccumulator_appendInt(outerSource, jump1);
					CharAccumulator_appendChars(outerSource, ":");
					CharAccumulator_appendChars(outerSource, trueCodeBlockSource.data);
					CharAccumulator_appendChars(outerSource, "\n\tbr label %");
					CharAccumulator_appendInt(outerSource, endJump);
					
					CharAccumulator_appendChars(outerSource, "\n\n");
					CharAccumulator_appendInt(outerSource, jump2);
					CharAccumulator_appendChars(outerSource, ":");
					CharAccumulator_appendChars(outerSource, falseCodeBlockSource.data);
					CharAccumulator_appendChars(outerSource, "\n\tbr label %");
					CharAccumulator_appendInt(outerSource, endJump);
					
					if (trueHasReturned && falseHasReturned) {
//						hasReturned = 1;
					}
				}
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendInt(outerSource, endJump);
				CharAccumulator_appendChars(outerSource, ":");
				
				CharAccumulator_free(&expressionSource);
				
				CharAccumulator_free(&trueCodeBlockSource);
				CharAccumulator_free(&falseCodeBlockSource);
				
				linkedList_freeList(&expectedTypesForIf);
				linkedList_freeList(&expressionTypes);
				
				break;
			}
			
			case ASTnodeType_return: {
				if (outerFunction == NULL) {
					addStringToReportMsg("return statements are only allowed in a function");
					compileError(FI, node->location);
				}
				
				ASTnode_return *data = (ASTnode_return *)node->value;
				
				if (data->expression == NULL) {
					if (!BuilderType_hasName(&outerFunction->returnType, "Void")) {
						addStringToReportMsg("returning Void in a function that does not return Void.");
						compileError(FI, node->location);
					}
					CharAccumulator_appendChars(outerSource, "\n\tret void");
				} else {
					CharAccumulator newInnerSource = {100, 0, 0};
					CharAccumulator_initialize(&newInnerSource);
					
					buildLLVM(FI, outerFunction, outerSource, &newInnerSource, typeToList(outerFunction->returnType), NULL, data->expression, 1, 1, 0);
					
					CharAccumulator_appendChars(outerSource, "\n\tret ");
					
					CharAccumulator_appendChars(outerSource, newInnerSource.data);
					
					CharAccumulator_free(&newInnerSource);
				}
				
				if (compilerOptions.includeDebugInformation) {
					addDILocation(outerSource, outerFunction->debugInformationScopeID, node->location);
				}
				
				hasReturned = 1;
				
				outerFunction->registerCount++;
				
				break;
			}
				
			case ASTnodeType_variableDefinition: {
				if (outerFunction == NULL) {
					addStringToReportMsg("variable definitions are only allowed in a function");
					compileError(FI, node->location);
				}
				
				ASTnode_variableDefinition *data = (ASTnode_variableDefinition *)node->value;
				
				expectUnusedName(FI, data->name, node->location);
				
				BuilderType* type = getType(FI, data->type);
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				linkedList_Node *types = NULL;
				buildLLVM(FI, outerFunction, outerSource, &expressionSource, typeToList(*type), &types, data->expression, 1, 1, 0);
				
				if (types != NULL) {
					memcpy(type->factStack, ((BuilderType *)types->data)->factStack, sizeof(type->factStack));
				}
				
				CharAccumulator_appendChars(outerSource, "\n\t%");
				CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
				CharAccumulator_appendChars(outerSource, " = alloca ");
				generateType(FI, outerSource, type);
				CharAccumulator_appendChars(outerSource, ", align ");
				CharAccumulator_appendInt(outerSource, type->binding->byteAlign);
				
				if (data->expression != NULL) {
					CharAccumulator_appendChars(outerSource, "\n\tstore ");
					CharAccumulator_appendChars(outerSource, expressionSource.data);
					CharAccumulator_appendChars(outerSource, ", ptr %");
					CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, ", align ");
					CharAccumulator_appendInt(outerSource, type->binding->byteAlign);
				}
				
				ContextBinding *variableBinding = linkedList_addNode(&FI->context.bindings[FI->level], sizeof(ContextBinding) + sizeof(ContextBinding_variable));
				
				variableBinding->originFile = FI;
				variableBinding->key = data->name;
				variableBinding->type = ContextBindingType_variable;
				variableBinding->byteSize = pointer_byteSize;
				variableBinding->byteAlign = pointer_byteSize;
				
				((ContextBinding_variable *)variableBinding->value)->LLVMRegister = outerFunction->registerCount;
				((ContextBinding_variable *)variableBinding->value)->LLVMtype = getLLVMtypeFromBinding(FI, type->binding);
				((ContextBinding_variable *)variableBinding->value)->type = *type;
				
				outerFunction->registerCount++;
				
				CharAccumulator_free(&expressionSource);
				
				break;
			}
			
			case ASTnodeType_operator: {
				ASTnode_operator *data = (ASTnode_operator *)node->value;
				
				CharAccumulator leftInnerSource = {100, 0, 0};
				CharAccumulator_initialize(&leftInnerSource);
				
				CharAccumulator rightInnerSource = {100, 0, 0};
				CharAccumulator_initialize(&rightInnerSource);
				
				if (data->operatorType == ASTnode_operatorType_assignment) {
					if (outerFunction == NULL) {
						addStringToReportMsg("variable assignments are only allowed in a function");
						compileError(FI, node->location);
					}
					
					ContextBinding *leftVariable = getContextBindingFromIdentifierNode(FI, (ASTnode *)data->left->data);
					
					CharAccumulator leftSource = {100, 0, 0};
					CharAccumulator_initialize(&leftSource);
					linkedList_Node *leftType = NULL;
					buildLLVM(FI, outerFunction, outerSource, &leftSource, NULL, &leftType, data->left, 0, 0, 0);
					
					CharAccumulator rightSource = {100, 0, 0};
					CharAccumulator_initialize(&rightSource);
					buildLLVM(FI, outerFunction, outerSource, &rightSource, leftType, NULL, data->right, 1, 1, 0);
					
					CharAccumulator_appendChars(outerSource, "\n\tstore ");
					CharAccumulator_appendChars(outerSource, rightSource.data);
					CharAccumulator_appendChars(outerSource, ", ptr ");
					CharAccumulator_appendChars(outerSource, leftSource.data);
					CharAccumulator_appendChars(outerSource, ", align ");
					CharAccumulator_appendInt(outerSource, leftVariable->byteAlign);
					
					CharAccumulator_free(&rightSource);
					CharAccumulator_free(&leftSource);
					
					break;
				}
				
				else if (data->operatorType == ASTnode_operatorType_memberAccess) {
					ASTnode *rightNode = (ASTnode *)data->right->data;
					
					if (rightNode->nodeType != ASTnodeType_identifier) {
						addStringToReportMsg("right side of memberAccess must be an identifier");
						
						addStringToReportIndicator("right side of memberAccess is not an identifier");
						compileError(FI, rightNode->location);
					};
					
					linkedList_Node *leftTypes = NULL;
					buildLLVM(FI, outerFunction, outerSource, &leftInnerSource, NULL, &leftTypes, data->left, 0, 0, 0);
					
					ContextBinding *structBinding = ((BuilderType *)leftTypes->data)->binding;
					if (structBinding->type != ContextBindingType_struct) {
						addStringToReportMsg("left side of member access is not a struct");
						compileError(FI, ((ASTnode *)data->left->data)->location);
					}
					ContextBinding_struct *structData = (ContextBinding_struct *)structBinding->value;
					
					ASTnode_identifier *rightData = (ASTnode_identifier *)rightNode->value;
					
					int index = 0;
					
					linkedList_Node *currentPropertyBinding = structData->propertyBindings;
					while (1) {
						if (currentPropertyBinding == NULL) {
							break;
						}
						
						ContextBinding *propertyBinding = (ContextBinding *)currentPropertyBinding->data;
						
						if (SubString_SubString_cmp(propertyBinding->key, rightData->name) == 0) {
							if (propertyBinding->type == ContextBindingType_variable) {
								if (types != NULL) {
									addTypeFromBuilderType(FI, types, &((ContextBinding_variable *)propertyBinding->value)->type);
								}
								
								CharAccumulator_appendChars(outerSource, "\n\t%");
								CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
								CharAccumulator_appendChars(outerSource, " = getelementptr inbounds ");
								CharAccumulator_appendChars(outerSource, structData->LLVMname);
								CharAccumulator_appendChars(outerSource, ", ptr ");
								CharAccumulator_appendChars(outerSource, leftInnerSource.data);
								CharAccumulator_appendChars(outerSource, ", i32 0");
								CharAccumulator_appendChars(outerSource, ", i32 ");
								CharAccumulator_appendInt(outerSource, index);
								
								if (loadVariables) {
									CharAccumulator_appendChars(outerSource, "\n\t%");
									CharAccumulator_appendInt(outerSource, outerFunction->registerCount + 1);
									CharAccumulator_appendChars(outerSource, " = load ");
									CharAccumulator_appendChars(outerSource, ((ContextBinding_variable *)propertyBinding->value)->LLVMtype);
									CharAccumulator_appendChars(outerSource, ", ptr %");
									CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
									CharAccumulator_appendChars(outerSource, ", align ");
									CharAccumulator_appendInt(outerSource, propertyBinding->byteAlign);
									
									if (withTypes) {
										CharAccumulator_appendChars(innerSource, ((ContextBinding_variable *)propertyBinding->value)->LLVMtype);
										CharAccumulator_appendChars(innerSource, " ");
									}
									
									outerFunction->registerCount++;
								}
								
								if (innerSource) {
									CharAccumulator_appendChars(innerSource, "%");
									CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
								} else {
									addStringToReportMsg("unused member access");
									compileWarning(FI, rightNode->location, WarningType_unused);
								}
								
								outerFunction->registerCount++;
							} else {
								abort();
							}
							
							break;
						}
						
						currentPropertyBinding = currentPropertyBinding->next;
						if (propertyBinding->type != ContextBindingType_function) {
							index++;
						}
					}
					
					linkedList_Node *currentMethodBinding = structData->methodBindings;
					while (1) {
						if (currentMethodBinding == NULL) {
							break;
						}
						
						ContextBinding *methodBinding = (ContextBinding *)currentMethodBinding->data;
						if (methodBinding->type != ContextBindingType_function) abort();
						
						if (SubString_SubString_cmp(methodBinding->key, rightData->name) == 0) {
							if (types != NULL) {
								addTypeFromBinding(FI, types, methodBinding);
							}
							break;
						}
						
						currentMethodBinding = currentMethodBinding->next;
					}
					
					if (currentPropertyBinding == NULL && currentMethodBinding == NULL) {
						addStringToReportMsg("left side of memberAccess must exist");
						compileError(FI, ((ASTnode *)data->left->data)->location);
					}
					
					CharAccumulator_free(&leftInnerSource);
					CharAccumulator_free(&rightInnerSource);
					break;
				}
				
				else if (data->operatorType == ASTnode_operatorType_scopeResolution) {
					ASTnode *leftNode = (ASTnode *)data->left->data;
					ASTnode *rightNode = (ASTnode *)data->right->data;
					
					// for now just assume everything is an identifier
					if (leftNode->nodeType != ASTnodeType_identifier) abort();
					if (rightNode->nodeType != ASTnodeType_identifier) abort();
					
					SubString *leftSubString = ((ASTnode_identifier *)leftNode->value)->name;
					SubString *rightSubString = ((ASTnode_identifier *)rightNode->value)->name;
					
					ContextBinding *namespaceBinding = getContextBindingFromSubString(FI, leftSubString);
					if (namespaceBinding == NULL) {
						addStringToReportMsg("value does not exist");
						
						addStringToReportIndicator("nothing named '");
						addSubStringToReportIndicator(leftSubString);
						addStringToReportIndicator("'");
						compileError(FI, leftNode->location);
					}
					
					if (namespaceBinding->type != ContextBindingType_namespace) {
						addStringToReportMsg("not a namespace");
						
						addStringToReportIndicator("'");
						addSubStringToReportIndicator(leftSubString);
						addStringToReportIndicator("' is not a namespace");
						compileError(FI, leftNode->location);
					}
					
					ContextBinding_namespace *namespace = (ContextBinding_namespace *)namespaceBinding->value;
					
					int success = 0;
					
					// find the module
					linkedList_Node *currentFile = namespace->files;
					while (1) {
						if (currentFile == NULL) {
							addStringToReportMsg("value does not exist is namespace");
							
							addStringToReportIndicator("nothing in namespace named '");
							addSubStringToReportIndicator(leftSubString);
							addStringToReportIndicator("'");
							compileError(FI, leftNode->location);
						}
						FileInformation *fileInformation = *(FileInformation **)currentFile->data;
						
						linkedList_Node *current = fileInformation->context.bindings[0];
						while (current != NULL) {
							ContextBinding *binding = ((ContextBinding *)current->data);
							
							if (SubString_SubString_cmp(binding->key, rightSubString) == 0) {
								addTypeFromBinding(FI, types, binding);
								success = 1;
								break;
							}
							
							current = current->next;
						}
						
						if (success) break;
						
						currentFile = currentFile->next;
					}
					
					break;
				}
				
				else if (data->operatorType == ASTnode_operatorType_cast) {
					linkedList_Node *expectedTypeForLeft = NULL;
					buildLLVM(FI, outerFunction, NULL, NULL, NULL, &expectedTypeForLeft, data->left, 0, 0, 0);
					buildLLVM(FI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeft, NULL, data->left, 1, 1, 0);
					
					BuilderType *fromType = (BuilderType *)expectedTypeForLeft->data;
					
					BuilderType *toType = getType(FI, (ASTnode *)data->right->data);
					
					CharAccumulator_appendChars(outerSource, "\n\t%");
					CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, " = ");
					
					// make the type bigger
					if (fromType->binding->byteSize < toType->binding->byteSize) {
						CharAccumulator_appendChars(outerSource, "sext ");
					}
					
					// make the type smaller
					else if (fromType->binding->byteSize > toType->binding->byteSize) {
						CharAccumulator_appendChars(outerSource, "trunc ");
					}
					
					else {
						addStringToReportMsg("cast not currently supported");
						
						compileError(FI, node->location);
					}
					
					CharAccumulator_appendChars(outerSource, leftInnerSource.data);
					CharAccumulator_appendChars(outerSource, " to ");
					CharAccumulator_appendChars(outerSource, ContextBinding_getLLVMname(toType->binding));
					
					if (withTypes) {
						CharAccumulator_appendChars(innerSource, ContextBinding_getLLVMname(toType->binding));
						CharAccumulator_appendChars(innerSource, " ");
					}
					CharAccumulator_appendChars(innerSource, "%");
					CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
					
					outerFunction->registerCount++;
					
					break;
				}
				
				linkedList_Node *leftType = NULL;
				linkedList_Node *rightType = NULL;
				
				linkedList_Node *expectedTypeForLeftAndRight = NULL;
				char *expectedLLVMtype = NULL;
				if (expectedTypes != NULL) {
					expectedLLVMtype = getLLVMtypeFromBinding(FI, ((BuilderType *)expectedTypes->data)->binding);
				}
				
				if (
					(
					 expectedTypes == NULL && types != NULL
					) ||
					(
						data->operatorType == ASTnode_operatorType_equivalent ||
						data->operatorType == ASTnode_operatorType_notEquivalent ||
						data->operatorType == ASTnode_operatorType_greaterThan ||
						data->operatorType == ASTnode_operatorType_lessThan
					 )
				) {
					//
					// figure out what the type of both sides should be
					//
					
					buildLLVM(FI, outerFunction, NULL, NULL, NULL, &leftType, data->left, 0, 0, 0);
					buildLLVM(FI, outerFunction, NULL, NULL, NULL, &rightType, data->right, 0, 0, 0);
					
					expectedTypeForLeftAndRight = leftType;
					// if we did not find a number on the left side
					if (!BuilderType_isNumber((BuilderType *)expectedTypeForLeftAndRight->data)) {
						if (!BuilderType_hasName((BuilderType *)expectedTypeForLeftAndRight->data, "__Number")) {
							addStringToReportMsg("left side of operator expected a number");
							compileError(FI, node->location);
						}
						
						// replace expectedTypeForLeftAndRight with the right type
						expectedTypeForLeftAndRight = rightType;
						// if we did not find a number on the right side
						if (!BuilderType_isNumber((BuilderType *)expectedTypeForLeftAndRight->data)) {
							if (!BuilderType_hasName((BuilderType *)expectedTypeForLeftAndRight->data, "__Number")) {
								addStringToReportMsg("right side of operator expected a number");
								compileError(FI, node->location);
							}
							
							// default to Int32
							expectedTypeForLeftAndRight = NULL;
							addTypeFromString(FI, &expectedTypeForLeftAndRight, "Int32", NULL);
						}
					}
				}
				
				// all of these operators are very similar and even use the same 'icmp' instruction
				// https://llvm.org/docs/LangRef.html#fcmp-instruction
				if (
					data->operatorType == ASTnode_operatorType_equivalent ||
					data->operatorType == ASTnode_operatorType_notEquivalent ||
					data->operatorType == ASTnode_operatorType_greaterThan ||
					data->operatorType == ASTnode_operatorType_lessThan
				) {
					buildLLVM(FI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeftAndRight, NULL, data->left, 1, 0, 0);
					buildLLVM(FI, outerFunction, outerSource, &rightInnerSource, expectedTypeForLeftAndRight, NULL, data->right, 1, 0, 0);
					CharAccumulator_appendChars(outerSource, "\n\t%");
					CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, " = icmp ");
					
					if (data->operatorType == ASTnode_operatorType_equivalent) {
						// eq: yields true if the operands are equal, false otherwise. No sign interpretation is necessary or performed.
						CharAccumulator_appendChars(outerSource, "eq ");
					}
					
					else if (data->operatorType == ASTnode_operatorType_notEquivalent) {
						// ne: yields true if the operands are unequal, false otherwise. No sign interpretation is necessary or performed.
						CharAccumulator_appendChars(outerSource, "ne ");
					}
					
					else if (data->operatorType == ASTnode_operatorType_greaterThan) {
						// sgt: interprets the operands as signed values and yields true if op1 is greater than op2.
						CharAccumulator_appendChars(outerSource, "sgt ");
					}
					
					else if (data->operatorType == ASTnode_operatorType_lessThan) {
						// slt: interprets the operands as signed values and yields true if op1 is less than op2.
						CharAccumulator_appendChars(outerSource, "slt ");
					}
					
					else {
						abort();
					}
					
					if (types != NULL) {
						addTypeFromString(FI, types, "Bool", NULL);
						// addTypeResultAfterOperationToList(FI, types, "Bool", data->operatorType, left, right);
					}
					
					CharAccumulator_appendChars(outerSource, getLLVMtypeFromBinding(FI, ((BuilderType *)expectedTypeForLeftAndRight->data)->binding));
				}
				
				else if (
					data->operatorType == ASTnode_operatorType_add ||
					data->operatorType == ASTnode_operatorType_subtract ||
					data->operatorType == ASTnode_operatorType_multiply ||
					data->operatorType == ASTnode_operatorType_divide ||
					data->operatorType == ASTnode_operatorType_modulo
				) {
					if (expectedTypes == NULL && types != NULL) {
						addTypeFromBuilderType(FI, types, (BuilderType *)expectedTypeForLeftAndRight->data);
					}
					
					if (expectedTypes != NULL && outerSource != NULL) {
						addTypeFromBuilderType(FI, &expectedTypeForLeftAndRight, (BuilderType *)expectedTypes->data);
						
						buildLLVM(FI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeftAndRight, NULL, data->left, 1, 0, 0);
						buildLLVM(FI, outerFunction, outerSource, &rightInnerSource, expectedTypeForLeftAndRight, NULL, data->right, 1, 0, 0);
						CharAccumulator_appendChars(outerSource, "\n\t%");
						CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
						
						if (data->operatorType == ASTnode_operatorType_add) {
							if (BuilderType_isInt((BuilderType *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = add ");
								if (BuilderType_isSignedInt((BuilderType *)expectedTypeForLeftAndRight->data)) {
									CharAccumulator_appendChars(outerSource, "nsw ");
								}
							} else if (BuilderType_isFloat((BuilderType *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = fadd ");
							} else {
								abort();
							}
						} else if (data->operatorType == ASTnode_operatorType_subtract) {
							if (BuilderType_isInt((BuilderType *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = sub ");
								if (BuilderType_isSignedInt((BuilderType *)expectedTypeForLeftAndRight->data)) {
									CharAccumulator_appendChars(outerSource, "nsw ");
								}
							} else if (BuilderType_isFloat((BuilderType *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = fsub ");
							} else {
								abort();
							}
						} else if (data->operatorType == ASTnode_operatorType_multiply) {
							if (BuilderType_isInt((BuilderType *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = mul ");
								if (BuilderType_isSignedInt((BuilderType *)expectedTypeForLeftAndRight->data)) {
									CharAccumulator_appendChars(outerSource, "nsw ");
								}
							} else if (BuilderType_isFloat((BuilderType *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = fmul ");
							} else {
								abort();
							}
						} else if (data->operatorType == ASTnode_operatorType_divide) {
							if (BuilderType_isUnsignedInt((BuilderType *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = udiv ");
							} else if (BuilderType_isSignedInt((BuilderType *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = sdiv ");
							} else if (BuilderType_isFloat((BuilderType *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = fdiv ");
							} else {
								abort();
							}
						} else if (data->operatorType == ASTnode_operatorType_modulo) {
							if (BuilderType_isUnsignedInt((BuilderType *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = urem ");
							} else if (BuilderType_isSignedInt((BuilderType *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = srem ");
							} else if (BuilderType_isFloat((BuilderType *)expectedTypeForLeftAndRight->data)) {
								addStringToReportMsg("modulo does not support floats");
								
								compileError(FI, node->location);
							} else {
								abort();
							}
						} else {
							abort();
						}
					}
					
					if (expectedLLVMtype != NULL) CharAccumulator_appendChars(outerSource, expectedLLVMtype);
				}
				
				else if (
					data->operatorType == ASTnode_operatorType_and ||
					data->operatorType == ASTnode_operatorType_or
				) {
					linkedList_Node *expectedTypesForIf = NULL;
					addTypeFromString(FI, &expectedTypesForIf, "Bool", NULL);
					
					buildLLVM(FI, outerFunction, outerSource, &leftInnerSource, expectedTypesForIf, NULL, data->left, 1, 0, 0);
					buildLLVM(FI, outerFunction, outerSource, &rightInnerSource, expectedTypesForIf, NULL, data->right, 1, 0, 0);
					
					if (outerSource != NULL) {
						CharAccumulator_appendChars(outerSource, "\n\t%");
						CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
						if (data->operatorType == ASTnode_operatorType_and) {
							CharAccumulator_appendChars(outerSource, " = and i1");
						} else if (data->operatorType == ASTnode_operatorType_or) {
							CharAccumulator_appendChars(outerSource, " = or i1");
						}
					}
					
					if (types != NULL) {
						addTypeFromString(FI, types, "Bool", NULL);
					}
				}
				
				else {
					abort();
				}
				
				if (outerSource != NULL && innerSource != NULL) {
					CharAccumulator_appendChars(outerSource, " ");
					CharAccumulator_appendChars(outerSource, leftInnerSource.data);
					CharAccumulator_appendChars(outerSource, ", ");
					CharAccumulator_appendChars(outerSource, rightInnerSource.data);
					
					if (withTypes && expectedLLVMtype != NULL) {
						CharAccumulator_appendChars(innerSource, expectedLLVMtype);
						CharAccumulator_appendChars(innerSource, " ");
					}
					CharAccumulator_appendChars(innerSource, "%");
					CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
					
					outerFunction->registerCount++;
				}
				
				CharAccumulator_free(&leftInnerSource);
				CharAccumulator_free(&rightInnerSource);
				
				break;
			}
			
			case ASTnodeType_bool: {
				ASTnode_bool *data = (ASTnode_bool *)node->value;
				
				if (!BuilderType_hasName((BuilderType *)expectedTypes->data, "Bool")) {
					addStringToReportMsg("unexpected type");
					
					addStringToReportIndicator("expected type '");
					addSubStringToReportIndicator(((BuilderType *)expectedTypes->data)->binding->key);
					addStringToReportIndicator("' but got a bool");
					compileError(FI, node->location);
				}
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, "i1 ");
				}
				
				if (data->isTrue) {
					CharAccumulator_appendChars(innerSource, "true");	
				} else {
					CharAccumulator_appendChars(innerSource, "false");
				}
				
				if (types != NULL) {
					addTypeFromString(FI, types, "Bool", node);
				}
				
				break;
			}
				
			case ASTnodeType_number: {
				ASTnode_number *data = (ASTnode_number *)node->value;
				
				if (types != NULL) {
					addTypeFromString(FI, types, "__Number", node);
				}
				
				if (expectedTypes != NULL) {
					if (!BuilderType_isNumber((BuilderType *)expectedTypes->data)) {
						addStringToReportMsg("unexpected type");
						
						addStringToReportIndicator("expected type '");
						addSubStringToReportIndicator(getTypeAsSubString((BuilderType *)expectedTypes->data));
						addStringToReportIndicator("' but got a number.");
						compileError(FI, node->location);
					}
					
//					ContextBinding *typeBinding = ((BuilderType *)expectedTypes->data)->binding;
//					if (data->value > pow(2, (typeBinding->byteSize * 8) - 1) - 1) {
//						addStringToReportMsg("integer overflow detected");
//						
//						CharAccumulator_appendInt(&reportIndicator, data->value);
//						addStringToReportIndicator(" is larger than the maximum size of the type '");
//						addSubStringToReportIndicator(getTypeAsSubString((BuilderType *)expectedTypes->data));
//						addStringToReportIndicator("'");
//						compileWarning(FI, node->location, WarningType_unsafe);
//					}
					
					if (withTypes) {
						char *LLVMtype = getLLVMtypeFromBinding(FI, ((BuilderType *)(*currentExpectedType)->data)->binding);
						
						CharAccumulator_appendChars(innerSource, LLVMtype);
						CharAccumulator_appendChars(innerSource, " ");
					}
				}
				
				if (innerSource != NULL) CharAccumulator_appendSubString(innerSource, data->string);
				if (expectedTypes != NULL && BuilderType_isFloat((BuilderType *)expectedTypes->data)) {
					CharAccumulator_appendChars(innerSource, ".0");
				}
				
				break;
			}
				
			case ASTnodeType_string: {
				ASTnode_string *data = (ASTnode_string *)node->value;
				
				if (expectedTypes != NULL && !BuilderType_hasName((BuilderType *)expectedTypes->data, "Pointer")) {
					addStringToReportMsg("unexpected type");
					
					addStringToReportIndicator("expected type '");
					addSubStringToReportIndicator(((BuilderType *)expectedTypes->data)->binding->key);
					addStringToReportIndicator("' but got a string");
					compileError(FI, node->location);
				}
				
				// + 1 for the NULL byte
				int stringLength = (unsigned int)(data->value->length + 1);
				
				CharAccumulator string = {100, 0, 0};
				CharAccumulator_initialize(&string);
				
				int i = 0;
				int escaped = 0;
				while (i < data->value->length) {
					char character = data->value->start[i];
					
					if (escaped) {
						if (character == '\\') {
							CharAccumulator_appendChars(&string, "\\\\");
							stringLength--;
						} else if (character == 'n') {
							CharAccumulator_appendChars(&string, "\\0A");
							stringLength--;
						} else {
							printf("unexpected character in string after escape '%c'\n", character);
							compileError(FI, (SourceLocation){node->location.line, node->location.columnStart + i + 1, node->location.columnEnd - (data->value->length - i)});
						}
						escaped = 0;
					} else {
						if (character == '\\') {
							escaped = 1;
						} else {
							CharAccumulator_appendChar(&string, character);
						}
					}
					
					i++;
				}
				
				if (innerSource != NULL) {
					CharAccumulator_appendChars(FI->topLevelConstantSource, "\n@.str.");
					CharAccumulator_appendInt(FI->topLevelConstantSource, FI->stringCount);
					CharAccumulator_appendChars(FI->topLevelConstantSource, " = private unnamed_addr constant [");
					CharAccumulator_appendInt(FI->topLevelConstantSource, stringLength);
					CharAccumulator_appendChars(FI->topLevelConstantSource, " x i8] c\"");
					CharAccumulator_appendChars(FI->topLevelConstantSource, string.data);
					CharAccumulator_appendChars(FI->topLevelConstantSource, "\\00\""); // the \00 is the NULL byte
					
					if (withTypes) {
						CharAccumulator_appendChars(innerSource, "ptr ");
					}
					CharAccumulator_appendChars(innerSource, "@.str.");
					CharAccumulator_appendInt(innerSource, FI->stringCount);
				}
				
				FI->stringCount++;
				
				CharAccumulator_free(&string);
				
				if (types != NULL) {
					addTypeFromString(FI, types, "Pointer", node);
				}
				
				break;
			}
			
			case ASTnodeType_identifier: {
				ASTnode_identifier *data = (ASTnode_identifier *)node->value;
				
				ContextBinding *variableBinding = getContextBindingFromIdentifierNode(FI, node);
				if (variableBinding == NULL) {
					addStringToReportMsg("value not in scope");
					
					addStringToReportIndicator("nothing named '");
					addSubStringToReportIndicator(data->name);
					addStringToReportIndicator("'");
					compileError(FI, node->location);
				}
				
				if (
					variableBinding->type == ContextBindingType_simpleType ||
					variableBinding->type == ContextBindingType_struct
				) {
					if (types != NULL) {
						addTypeFromBinding(FI, types, variableBinding);
					}
				} else if (variableBinding->type == ContextBindingType_variable) {
					ContextBinding_variable *variable = (ContextBinding_variable *)variableBinding->value;
					
					if (expectedTypes != NULL) {
						expectType(FI, variable, (BuilderType *)expectedTypes->data, &variable->type, node->location);
					}
					
					if (types != NULL) {
						addTypeFromBuilderType(FI, types, &variable->type);
					}
					
					if (outerSource != NULL) {
						if (withTypes) {
							CharAccumulator_appendChars(innerSource, variable->LLVMtype);
							CharAccumulator_appendChars(innerSource, " ");
						}
						
						if (innerSource != NULL) CharAccumulator_appendChars(innerSource, "%");
						
						if (loadVariables) {
							CharAccumulator_appendChars(outerSource, "\n\t%");
							CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
							CharAccumulator_appendChars(outerSource, " = load ");
							CharAccumulator_appendChars(outerSource, variable->LLVMtype);
							CharAccumulator_appendChars(outerSource, ", ptr %");
							CharAccumulator_appendInt(outerSource, variable->LLVMRegister);
							CharAccumulator_appendChars(outerSource, ", align ");
							CharAccumulator_appendInt(outerSource, variableBinding->byteAlign);
							
							if (innerSource != NULL) CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
							
							outerFunction->registerCount++;
						} else {
							if (innerSource != NULL) CharAccumulator_appendInt(innerSource, variable->LLVMRegister);
						}
					}
				} else if (variableBinding->type == ContextBindingType_function) {
					if (expectedTypes != NULL) {
						if (!BuilderType_hasName((BuilderType *)expectedTypes->data, "Function")) {
							addStringToReportMsg("unexpected type");
							
							addStringToReportIndicator("expected type '");
							addSubStringToReportIndicator(((BuilderType *)expectedTypes->data)->binding->key);
							addStringToReportIndicator("' but got a function");
							compileError(FI, node->location);
						}
					}
					
					if (types != NULL) {
						addTypeFromBinding(FI, types, variableBinding);
					}
				} else if (variableBinding->type == ContextBindingType_macro) {
					if (expectedTypes != NULL) {
						if (!BuilderType_hasName((BuilderType *)expectedTypes->data, "__Macro")) {
							addStringToReportMsg("unexpected type");
							
							addStringToReportIndicator("expected type '");
							addSubStringToReportIndicator(((BuilderType *)expectedTypes->data)->binding->key);
							addStringToReportIndicator("' but got a macro");
							compileError(FI, node->location);
						}
					}
					
					if (types != NULL) {
						addTypeFromBinding(FI, types, variableBinding);
					}
				} else {
					abort();
				}
				
				break;
			}
			
//			case ASTnodeType_subscript: {
//				ASTnode_subscript *data = (ASTnode_subscript *)node->value;
//				
//				break;
//			}
			
			default: {
				printf("unknown node type: %u\n", node->nodeType);
				compileError(FI, node->location);
				break;
			}
		}
		
		if (innerSource != NULL && withCommas && mainLoopCurrent->next != NULL) {
			CharAccumulator_appendChars(innerSource, ",");
		}
		
		mainLoopCurrent = mainLoopCurrent->next;
		if (currentExpectedType != NULL) {
			*currentExpectedType = (*currentExpectedType)->next;
		}
	}
	
	//
	// after loop
	//
	
	if (FI->level == 0) {
		linkedList_Node *afterLoopCurrent = current;
		while (afterLoopCurrent != NULL) {
			ASTnode *node = ((ASTnode *)afterLoopCurrent->data);
			
			if (node->nodeType == ASTnodeType_function) {
				ASTnode_function *data = (ASTnode_function *)node->value;
				
				ContextBinding *functionBinding = getContextBindingFromSubString(FI, data->name);
				if (functionBinding == NULL || functionBinding->type != ContextBindingType_function) abort();
				
				generateFunction(FI, outerSource, functionBinding, node, 1);
			}
			
			afterLoopCurrent = afterLoopCurrent->next;
		}
	}
	
	if (FI->level != 0) {
		linkedList_freeList(&FI->context.bindings[FI->level]);
	}
	
	FI->level--;
	
	return hasReturned;
}
