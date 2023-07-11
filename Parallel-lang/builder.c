#include <stdio.h>
#include <string.h>

#include "globals.h"
#include "builder.h"
#include "error.h"

void expectTypeWithType(ASTnode *expectedTypeNode, ASTnode *actualTypeNode, SourceLocation location) {
	if (expectedTypeNode->type != ASTnodeType_type || actualTypeNode->type != ASTnodeType_type) {
		abort();
	}
	
	ASTnode_type *expectedType = (ASTnode_type *)expectedTypeNode->value;
	ASTnode_type *actualType = (ASTnode_type *)actualTypeNode->value;
	
	if (SubString_SubString_cmp(expectedType->name, actualType->name) != 0) {
		printf("expected type '");
		SubString_print(expectedType->name);
		printf("' but got type '");
		SubString_print(actualType->name);
		printf("'\n");
		compileError(location);
	}
}

void expectTypeWithString(ASTnode *expectedTypeNode, char *actualTypeString, SourceLocation location) {
	if (expectedTypeNode->type != ASTnodeType_type) {
		abort();
	}
	
	ASTnode_type *expectedType = (ASTnode_type *)expectedTypeNode->value;
	
	if (SubString_string_cmp(expectedType->name, actualTypeString) != 0) {
		printf("expected type '");
		SubString_print(expectedType->name);
		printf("' but got type '%s'\n", actualTypeString);
		compileError(location);
	}
}

void addTypeAsString(linkedList_Node **list, char *string) {
	SubString *subString = malloc(sizeof(SubString));
	if (subString == NULL) {
		printf("malloc failed\n");
		abort();
	}
	*subString = (SubString){string, (int)strlen(string)};
	
	ASTnode *data = linkedList_addNode(list, sizeof(ASTnode) + sizeof(ASTnode_type));
	data->type = ASTnodeType_type;
	data->location = (SourceLocation){0, 0, 0};
	((ASTnode_type *)data->value)->name = subString;
}

void addBuilderType(linkedList_Node **variables, SubString *key, char *LLVMtype) {
	Variable *data = linkedList_addNode(variables, sizeof(Variable) + sizeof(Variable_type));
	
	data->key = key;
	data->type = VariableType_type;
	((Variable_type *)data->value)->LLVMtype = LLVMtype;
}

Variable *getBuilderVariable(linkedList_Node **variables, int level, SubString *key) {
	int index = level;
	while (index >= 0) {
		linkedList_Node *current = variables[index];
		
		while (current != NULL) {
			Variable *variable = ((Variable *)current->data);
			
			if (SubString_SubString_cmp(key, variable->key) == 0) {
				return variable;
			}
			
			current = current->next;
		}
		
		index--;
	}
	
	return NULL;
}

char *getLLVMtypeFromType(linkedList_Node **variables, int level, ASTnode *type) {
	Variable *LLVMtypeVariable = getBuilderVariable(variables, level, ((ASTnode_type *)(type)->value)->name);
	
	return ((Variable_type *)LLVMtypeVariable->value)->LLVMtype;
}

void generateFunction(GlobalBuilderInformation *GBI, Variable_function *function, ASTnode *node) {
	ASTnode_function *data = (ASTnode_function *)node->value;
	
	if (data->external) {
		CharAccumulator_appendSubString(GBI->topLevelSource, ((ASTnode_string *)(((ASTnode *)(data->codeBlock)->data)->value))->value);
	}
	
	else {
		char *LLVMreturnType = getLLVMtypeFromType(GBI->variables, GBI->level, (ASTnode *)data->returnType->data);
		
		CharAccumulator_appendChars(GBI->topLevelSource, "\ndefine ");
		CharAccumulator_appendChars(GBI->topLevelSource, LLVMreturnType);
		CharAccumulator_appendChars(GBI->topLevelSource, " @");
		CharAccumulator_appendSubString(GBI->topLevelSource, data->name);
		CharAccumulator_appendChars(GBI->topLevelSource, "(");
		
		CharAccumulator functionSource = {100, 0, 0};
		CharAccumulator_initialize(&functionSource);
		
		linkedList_Node *currentArgument = data->argumentTypes;
		linkedList_Node *currentArgumentName = data->argumentNames;
		if (currentArgument != NULL) {
			int argumentCount =  linkedList_getCount(&data->argumentTypes);
			while (1) {
				char *currentArgumentLLVMtype = getLLVMtypeFromType(GBI->variables, GBI->level, (ASTnode *)currentArgument->data);
				CharAccumulator_appendChars(GBI->topLevelSource, currentArgumentLLVMtype);
				CharAccumulator_appendChars(GBI->topLevelSource, " %");
				CharAccumulator_appendUint(GBI->topLevelSource, function->registerCount);
				
				CharAccumulator_appendChars(&functionSource, "\n\t%");
				CharAccumulator_appendUint(&functionSource, function->registerCount + argumentCount + 1);
				CharAccumulator_appendChars(&functionSource, " = alloca ");
				CharAccumulator_appendChars(&functionSource, currentArgumentLLVMtype);
				CharAccumulator_appendChars(&functionSource, ", align 8");
				
				CharAccumulator_appendChars(&functionSource, "\n\tstore ");
				CharAccumulator_appendChars(&functionSource, currentArgumentLLVMtype);
				CharAccumulator_appendChars(&functionSource, " %");
				CharAccumulator_appendUint(&functionSource, function->registerCount);
				CharAccumulator_appendChars(&functionSource, ", ptr %");
				CharAccumulator_appendUint(&functionSource, function->registerCount + argumentCount + 1);
				CharAccumulator_appendChars(&functionSource, ", align 8");
				
				Variable *argumentVariable = getBuilderVariable(GBI->variables, GBI->level, (SubString *)currentArgumentName->data);
				
				if (argumentVariable != NULL) {
					printf("there is already something named '");
					SubString_print((SubString *)currentArgumentName->data);
					printf("'\n");
					compileError(node->location);
				}
				
				Variable *argumentVariableData = linkedList_addNode(&GBI->variables[GBI->level + 1], sizeof(Variable) + sizeof(Variable_variable));
				
				argumentVariableData->key = (SubString *)currentArgumentName->data;
				
				argumentVariableData->type = VariableType_variable;
				
				((Variable_variable *)argumentVariableData->value)->LLVMRegister = function->registerCount + argumentCount + 1;
				((Variable_variable *)argumentVariableData->value)->LLVMtype = currentArgumentLLVMtype;
				((Variable_variable *)argumentVariableData->value)->type = currentArgument;
				
				function->registerCount++;
				
				if (currentArgument->next == NULL) {
					break;
				}
				
				CharAccumulator_appendChars(GBI->topLevelSource, ", ");
				currentArgument = currentArgument->next;
				currentArgumentName = currentArgumentName->next;
			}
			
			function->registerCount += argumentCount;
		}
		
		function->registerCount++;
		
		CharAccumulator_appendChars(GBI->topLevelSource, ") {");
		CharAccumulator_appendChars(GBI->topLevelSource, functionSource.data);
		buildLLVM(GBI, data->name, NULL, NULL, NULL, data->codeBlock, 0, 0);
		
		if (!function->hasReturned) {
			printf("function did not return\n");
			compileError(node->location);
		}
		
		CharAccumulator_appendChars(GBI->topLevelSource, "\n}");
		
		CharAccumulator_free(&functionSource);
	}
}

void buildLLVM(GlobalBuilderInformation *GBI, SubString *outerName, CharAccumulator *innerSource, linkedList_Node *expectedTypes, linkedList_Node **types, linkedList_Node *current, int withTypes, int withCommas) {
	GBI->level++;
	if (GBI->level > maxVariablesLevel) {
		printf("level (%i) > maxVariablesLevel (%i)\n", GBI->level, maxVariablesLevel);
		abort();
	}
	
	linkedList_Node **currentExpectedType = NULL;
	if (expectedTypes != NULL) {
		currentExpectedType = &expectedTypes;
	}
	
	//
	// pre-loop
	//
	
	if (GBI->level == 0) {
		linkedList_Node *preLoopCurrent = current;
		while (1) {
			if (preLoopCurrent == NULL) {
				break;
			}
			
			ASTnode *node = ((ASTnode *)preLoopCurrent->data);
			
			if (node->type == ASTnodeType_function) {
				ASTnode_function *data = (ASTnode_function *)node->value;
				
				// make sure that the return type actually exists
				Variable *type = getBuilderVariable(GBI->variables, GBI->level, ((ASTnode_type *)((ASTnode *)data->returnType->data)->value)->name);
				if (type == NULL) {
					printf("unknown type: '");
					SubString_print(((ASTnode_type *)((ASTnode *)data->returnType->data)->value)->name);
					printf("'\n");
					compileError(((ASTnode *)data->returnType->data)->location);
				}
				
				if (type->type != VariableType_type) {
					abort();
				}
				
				// make sure that the types of all arguments actually exist
				linkedList_Node *currentArgument = data->argumentTypes;
				while (currentArgument != NULL) {
//					SubString_print(((ASTnode_type *)((ASTnode *)currentArgument->data)->value)->name);
					
					Variable *currentArgumentType = getBuilderVariable(GBI->variables, GBI->level, ((ASTnode_type *)((ASTnode *)currentArgument->data)->value)->name);
					if (currentArgumentType == NULL) {
						printf("unknown type: '");
						SubString_print(((ASTnode_type *)((ASTnode *)currentArgument->data)->value)->name);
						printf("'\n");
						compileError(((ASTnode *)currentArgument->data)->location);
					}
					
					currentArgument = currentArgument->next;
				}
				
				char *LLVMreturnType = getLLVMtypeFromType(GBI->variables, GBI->level, (ASTnode *)data->returnType->data);
				
				Variable *function = linkedList_addNode(&GBI->variables[0], sizeof(Variable) + sizeof(Variable_function));
				
				char *functionLLVMname = malloc(data->name->length + 1);
				memcpy(functionLLVMname, data->name->start, data->name->length);
				functionLLVMname[data->name->length] = 0;
				
				function->key = data->name;
				
				function->type = VariableType_function;
				
				((Variable_function *)function->value)->LLVMname = functionLLVMname;
				((Variable_function *)function->value)->LLVMreturnType = LLVMreturnType;
				((Variable_function *)function->value)->argumentTypes = data->argumentTypes;
				((Variable_function *)function->value)->returnType = data->returnType;
				((Variable_function *)function->value)->hasReturned = 0;
				((Variable_function *)function->value)->registerCount = 0;
			}
			
			preLoopCurrent = preLoopCurrent->next;
		}
	}
	
	//
	// main loop
	//
	
	while (1) {
		if (current == NULL) {
			break;
		}
		
		ASTnode *node = ((ASTnode *)current->data);
		
		switch (node->type) {
			case ASTnodeType_function: {
				if (GBI->level != 0) {
					printf("function definitions are only allowed at top level\n");
					compileError(node->location);
				}
				
				ASTnode_function *data = (ASTnode_function *)node->value;
				
				Variable *functionVariable = getBuilderVariable(GBI->variables, GBI->level, data->name);
				if (functionVariable == NULL || functionVariable->type != VariableType_function) abort();
				Variable_function *function = (Variable_function *)functionVariable->value;
				
				generateFunction(GBI, function, node);
				
				break;
			}
				
			case ASTnodeType_call: {
				if (outerName == NULL) {
					printf("function call in a weird spot\n");
					compileError(node->location);
				}
				
				ASTnode_call *data = (ASTnode_call *)node->value;
				
				Variable *outerFunctionVariable = getBuilderVariable(GBI->variables, GBI->level, outerName);
				if (outerFunctionVariable == NULL || outerFunctionVariable->type != VariableType_function) abort();
				Variable_function *outerFunction = (Variable_function *)outerFunctionVariable->value;
				
				Variable *functionToCallVariable = getBuilderVariable(GBI->variables, GBI->level, data->name);
				if (functionToCallVariable == NULL || functionToCallVariable->type != VariableType_function) {
					printf("no function named '");
					SubString_print(data->name);
					printf("'\n");
					compileError(node->location);
				}
				Variable_function *functionToCall = (Variable_function *)functionToCallVariable->value;
				
				if (types != NULL) {
					linkedList_join(types, &functionToCall->returnType);
					break;
				}
				
				int expectedArgumentCount = linkedList_getCount(&functionToCall->argumentTypes);
				int actualArgumentCount = linkedList_getCount(&data->arguments);
				
				if (expectedArgumentCount > actualArgumentCount) {
					SubString_print(data->name);
					printf(" did not get enough arguments (expected %d but got %d)\n", expectedArgumentCount, actualArgumentCount);
					compileError(node->location);
				}
				if (expectedArgumentCount < actualArgumentCount) {
					SubString_print(data->name);
					printf(" got too many arguments (expected %d but got %d)\n", expectedArgumentCount, actualArgumentCount);
					compileError(node->location);
				}
				
				if (types == NULL) {
					CharAccumulator newInnerSource = {100, 0, 0};
					CharAccumulator_initialize(&newInnerSource);
					
					buildLLVM(GBI, outerName, &newInnerSource, functionToCall->argumentTypes, NULL, data->arguments, 1, 1);
					
					if (strcmp(functionToCall->LLVMreturnType, "void") != 0) {
						CharAccumulator_appendChars(GBI->topLevelSource, "\n\t%");
						CharAccumulator_appendUint(GBI->topLevelSource, outerFunction->registerCount);
						CharAccumulator_appendChars(GBI->topLevelSource, " = call ");
						
						if (innerSource != NULL) {
							if (withTypes) {
								CharAccumulator_appendChars(innerSource, functionToCall->LLVMreturnType);
								CharAccumulator_appendChars(innerSource, " ");
							}
							CharAccumulator_appendChars(innerSource, "%");
							CharAccumulator_appendUint(innerSource, outerFunction->registerCount);
						}
					} else {
						CharAccumulator_appendChars(GBI->topLevelSource, "\n\tcall ");
					}
					CharAccumulator_appendChars(GBI->topLevelSource, functionToCall->LLVMreturnType);
					CharAccumulator_appendChars(GBI->topLevelSource, " @");
					CharAccumulator_appendChars(GBI->topLevelSource, functionToCall->LLVMname);
					CharAccumulator_appendChars(GBI->topLevelSource, "(");
					CharAccumulator_appendChars(GBI->topLevelSource, newInnerSource.data);
					CharAccumulator_appendChars(GBI->topLevelSource, ")");
					
					CharAccumulator_free(&newInnerSource);
					
					outerFunction->registerCount++;
				}
				
				break;
			}
				
//			case ASTnodeType_while: {
//				if (outerName == NULL) {
//					printf("while loop in a weird spot\n");
//					compileError(node->location);
//				}
//
//				break;
//			}
//
//			case ASTnodeType_if: {
//				if (outerName == NULL) {
//					printf("if statement in a weird spot\n");
//					compileError(node->location);
//				}
//
//				break;
//			}
				
			case ASTnodeType_return: {
				if (outerName == NULL) {
					printf("return in a weird spot\n");
					compileError(node->location);
				}
				
				Variable *outerFunctionVariable = getBuilderVariable(GBI->variables, GBI->level, outerName);
				if (outerFunctionVariable == NULL || outerFunctionVariable->type != VariableType_function) abort();
				Variable_function *outerFunction = (Variable_function *)outerFunctionVariable->value;
				
				ASTnode_return *data = (ASTnode_return *)node->value;
				
				if (data->expression == NULL) {
					CharAccumulator_appendChars(GBI->topLevelSource, "\n\tret void");
				} else {
					CharAccumulator newInnerSource = {100, 0, 0};
					CharAccumulator_initialize(&newInnerSource);
					
					buildLLVM(GBI, outerName, &newInnerSource, outerFunction->returnType, NULL, data->expression, 1, 0);
					
					CharAccumulator_appendChars(GBI->topLevelSource, "\n\tret ");
					
					CharAccumulator_appendChars(GBI->topLevelSource, newInnerSource.data);
					
					CharAccumulator_free(&newInnerSource);
				}
				
				outerFunction->hasReturned = 1;
				
				break;
			}
				
//			case ASTnodeType_variableDefinition: {
//				if (outerName == NULL) {
//					printf("variable definition in a weird spot\n");
//					compileError(node->location);
//				}
//			}
//
//				break;
//			}
//
//			case ASTnodeType_variableAssignment: {
//				if (outerName == NULL) {
//					printf("variable assignment in a weird spot\n");
//					compileError(node->location);
//				}
//
//				break;
//			}
//
//			case ASTnodeType_operator: {
//
//				break;
//			}
//
//			case ASTnodeType_true: {
//
//				break;
//			}
//
//			case ASTnodeType_false: {
//
//				break;
//			}
				
			case ASTnodeType_number: {
				ASTnode_number *data = (ASTnode_number *)node->value;
				
				if (types != NULL) {
					addTypeAsString(types, "number");
					break;
				}
				
				char *LLVMtype = getLLVMtypeFromType(GBI->variables, GBI->level, (ASTnode *)(*currentExpectedType)->data);
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, LLVMtype);
					CharAccumulator_appendChars(innerSource, " ");
				}
				
				CharAccumulator_appendSubString(innerSource, data->string);
				
				break;
			}
				
//			case ASTnodeType_string: {
//
//				break;
//			}
//
//			case ASTnodeType_variable: {
//
//				break;
//			}
				
			default: {
				printf("unknown node type: %u\n", node->type);
				compileError(node->location);
				break;
			}
		}
		
		if (withCommas && current->next != NULL) {
			CharAccumulator_appendChars(innerSource, ",");
		}
		
		current = current->next;
		if (currentExpectedType != NULL) {
			*currentExpectedType = (*currentExpectedType)->next;
		}
	}
	
	linkedList_freeList(&GBI->variables[GBI->level]);
	
	GBI->level--;
}
