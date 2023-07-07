#include <stdio.h>
#include <string.h>

#include "builder.h"
#include "error.h"

void expectType(ASTnode *expectedTypeNode, ASTnode *actualTypeNode, SourceLocation location) {
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

char *getLLVMtypeFromType(linkedList_Node **variables, int level, linkedList_Node *type) {
	Variable *LLVMtypeVariable = getBuilderVariable(variables, level, ((ASTnode_type *)((ASTnode *)type->data)->value)->name);
	
	return ((Variable_type *)LLVMtypeVariable->value)->LLVMtype;
}

char *buildLLVM(GlobalBuilderInformation *globalBuilderInformation, linkedList_Node **variables, int level, CharAccumulator *outerSource, SubString *outerName, linkedList_Node *expectedTypes, linkedList_Node *current, int withTypes, int withCommas) {
	level++;
	if (level > maxBuilderLevel) {
		printf("level (%i) > maxBuilderLevel (%i)\n", level, maxBuilderLevel);
		abort();
	}
	
	CharAccumulator LLVMsource = {100, 0, 0};
	CharAccumulator_initialize(&LLVMsource);
	
	//
	// pre-loop
	//
	
	if (level == 0) {
		linkedList_Node *preLoopCurrent = current;
		while (1) {
			if (preLoopCurrent == NULL) {
				break;
			}
			
			ASTnode *node = ((ASTnode *)preLoopCurrent->data);
			
			if (node->type == ASTnodeType_function) {
				ASTnode_function *data = (ASTnode_function *)node->value;
				
				// make sure that the return type actually exists
				Variable *type = getBuilderVariable(variables, level, ((ASTnode_type *)((ASTnode *)data->returnType->data)->value)->name);
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
					
					Variable *currentArgumentType = getBuilderVariable(variables, level, ((ASTnode_type *)((ASTnode *)currentArgument->data)->value)->name);
					if (currentArgumentType == NULL) {
						printf("unknown type: '");
						SubString_print(((ASTnode_type *)((ASTnode *)currentArgument->data)->value)->name);
						printf("'\n");
						compileError(((ASTnode *)currentArgument->data)->location);
					}
					
					currentArgument = currentArgument->next;
				}
				
				char *LLVMreturnType = getLLVMtypeFromType(variables, level, data->returnType);
				
				Variable *function = linkedList_addNode(&variables[0], sizeof(Variable) + sizeof(Variable_function));
				
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
				((Variable_function *)function->value)->registerCount = 1;
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
				if (level != 0 || outerSource != NULL) {
					printf("function definitions are only allowed at top level\n");
					compileError(node->location);
				}
				ASTnode_function *data = (ASTnode_function *)node->value;
				
				Variable *type = getBuilderVariable(variables, level, ((ASTnode_type *)((ASTnode *)data->returnType->data)->value)->name);
				
				Variable *functionVariable = getBuilderVariable(variables, level, data->name);
				if (functionVariable == NULL || functionVariable->type != VariableType_function) abort();
				Variable_function *function = (Variable_function *)functionVariable->value;
				
				if (data->external) {
					CharAccumulator_appendSubString(&LLVMsource, ((ASTnode_string *)(((ASTnode *)(data->codeBlock)->data)->value))->value);
				} else {
					CharAccumulator newOuterSource = {100, 0, 0};
					CharAccumulator_initialize(&newOuterSource);
					
					char *LLVMtype = ((Variable_type *)type->value)->LLVMtype;
					CharAccumulator_appendChars(&LLVMsource, "\ndefine ");
					CharAccumulator_appendChars(&LLVMsource, LLVMtype);
					CharAccumulator_appendChars(&LLVMsource, " @");
					CharAccumulator_appendSubString(&LLVMsource, data->name);
					CharAccumulator_appendChars(&LLVMsource, "(");
					
					linkedList_Node *currentArgument = data->argumentTypes;
					linkedList_Node *currentArgumentName = data->argumentNames;
					if (currentArgument != NULL) {
						int argumentCount =  linkedList_getCount(&data->argumentTypes);
						function->registerCount = 0;
						while (1) {
							Variable *currentArgumentType = getBuilderVariable(variables, level, ((ASTnode_type *)((ASTnode *)currentArgument->data)->value)->name);
							CharAccumulator_appendChars(&LLVMsource, ((Variable_type *)currentArgumentType->value)->LLVMtype);
							CharAccumulator_appendChars(&LLVMsource, " %");
							CharAccumulator_appendUint(&LLVMsource, function->registerCount);
							
							CharAccumulator_appendChars(&newOuterSource, "\n\t%");
							CharAccumulator_appendUint(&newOuterSource, function->registerCount + argumentCount + 1);
							CharAccumulator_appendChars(&newOuterSource, " = alloca ");
							CharAccumulator_appendChars(&newOuterSource, ((Variable_type *)currentArgumentType->value)->LLVMtype);
							CharAccumulator_appendChars(&newOuterSource, ", align 8");
							
							CharAccumulator_appendChars(&newOuterSource, "\n\tstore ");
							CharAccumulator_appendChars(&newOuterSource, ((Variable_type *)currentArgumentType->value)->LLVMtype);
							CharAccumulator_appendChars(&newOuterSource, " %");
							CharAccumulator_appendUint(&newOuterSource, function->registerCount);
							CharAccumulator_appendChars(&newOuterSource, ", ");
							CharAccumulator_appendChars(&newOuterSource, ((Variable_type *)currentArgumentType->value)->LLVMtype);
							CharAccumulator_appendChars(&newOuterSource, " %");
							CharAccumulator_appendUint(&newOuterSource, function->registerCount + argumentCount + 1);
							CharAccumulator_appendChars(&newOuterSource, ", align 8");
							
							Variable *argumentVariable = getBuilderVariable(variables, level, (SubString *)currentArgumentName->data);
							
							if (argumentVariable != NULL) {
								printf("there is already something named '");
								SubString_print((SubString *)currentArgumentName->data);
								printf("'\n");
								compileError(node->location);
							}
							
							Variable *argumentVariableData = linkedList_addNode(&variables[level + 1], sizeof(Variable) + sizeof(Variable_variable));
							
							argumentVariableData->key = (SubString *)currentArgumentName->data;
							
							argumentVariableData->type = VariableType_variable;
							
							((Variable_variable *)argumentVariableData->value)->LLVMRegister = function->registerCount + argumentCount + 1;
							((Variable_variable *)argumentVariableData->value)->LLVMtype = ((Variable_type *)currentArgumentType->value)->LLVMtype;
							((Variable_variable *)argumentVariableData->value)->type = currentArgument;
							
							function->registerCount++;
							
							if (currentArgument->next == NULL) {
								break;
							}
							
							CharAccumulator_appendChars(&LLVMsource, ", ");
							currentArgument = currentArgument->next;
							currentArgumentName = currentArgumentName->next;
						}
						
						function->registerCount += argumentCount + 1;
					}
					
					free(buildLLVM(globalBuilderInformation, variables, level, &newOuterSource, data->name, NULL, data->codeBlock, 1, 0));
					
					if (!function->hasReturned) {
						printf("function did not return\n");
						compileError(node->location);
					}
					
					CharAccumulator_appendChars(&LLVMsource, ") {");
					CharAccumulator_appendChars(&LLVMsource, newOuterSource.data);
					CharAccumulator_appendChars(&LLVMsource, "\n}");
					
					CharAccumulator_free(&newOuterSource);
				}
				
				break;
			}
				
			case ASTnodeType_call: {
				ASTnode_call *data = (ASTnode_call *)node->value;
				
				if (outerSource == NULL || outerName == NULL) {
					printf("function call in a weird spot\n");
					compileError(node->location);
				}
				Variable *outerFunctionVariable = getBuilderVariable(variables, level, outerName);
				if (outerFunctionVariable == NULL || outerFunctionVariable->type != VariableType_function) abort();
				Variable_function *outerFunction = (Variable_function *)outerFunctionVariable->value;
				
				Variable *functionToCallVariable = getBuilderVariable(variables, level, data->name);
				if (functionToCallVariable == NULL || functionToCallVariable->type != VariableType_function) {
					printf("no function named '");
					SubString_print(data->name);
					printf("'\n");
					compileError(node->location);
				}
				Variable_function *functionToCall = (Variable_function *)functionToCallVariable->value;
				
				if (expectedTypes != NULL) {
					SubString *expectedTypeName = ((ASTnode_type *)((ASTnode *)expectedTypes->data)->value)->name;
					
					SubString *actualTypeName = ((ASTnode_type *)((ASTnode *)functionToCall->returnType->data)->value)->name;
					
					if (SubString_SubString_cmp(expectedTypeName, actualTypeName) != 0) {
						printf("expected type '");
						SubString_print(expectedTypeName);
						printf("' but got type '");
						SubString_print(actualTypeName);
						printf("'\n");
						compileError(node->location);
					}
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
				
				char *LLVMarguments = buildLLVM(globalBuilderInformation, variables, level, outerSource, outerName, functionToCall->argumentTypes, data->arguments, 1, 1);
				
				if (strcmp(functionToCall->LLVMreturnType, "void") != 0) {
					CharAccumulator_appendChars(outerSource, "\n\t%");
					CharAccumulator_appendUint(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, " = call ");
					
					if (withTypes) {
						CharAccumulator_appendChars(&LLVMsource, functionToCall->LLVMreturnType);
						CharAccumulator_appendChars(&LLVMsource, " ");
					}
					CharAccumulator_appendChars(&LLVMsource, "%");
					CharAccumulator_appendUint(&LLVMsource, outerFunction->registerCount);
					
					outerFunction->registerCount++;
				} else {
					CharAccumulator_appendChars(outerSource, "\n\tcall ");
				}
				CharAccumulator_appendChars(outerSource, functionToCall->LLVMreturnType);
				CharAccumulator_appendChars(outerSource, " @");
				CharAccumulator_appendChars(outerSource, functionToCall->LLVMname);
				CharAccumulator_appendChars(outerSource, "(");
				CharAccumulator_appendChars(outerSource, LLVMarguments);
				CharAccumulator_appendChars(outerSource, ")");
				
				free(LLVMarguments);
				
				break;
			}
				
			case ASTnodeType_if: {
				if (outerSource == NULL || outerName == NULL) {
					printf("if statement in a weird spot\n");
					compileError(node->location);
				}
				
				ASTnode_if *data = (ASTnode_if *)node->value;
				
				Variable *outerFunctionVariable = getBuilderVariable(variables, level, outerName);
				if (outerFunctionVariable == NULL || outerFunctionVariable->type != VariableType_function) abort();
				Variable_function *outerFunction = (Variable_function *)outerFunctionVariable->value;
				
				linkedList_Node *expectedTypesForIf = NULL;
				ASTnode *expectedTypesForIfData = linkedList_addNode(&expectedTypesForIf, sizeof(ASTnode) + sizeof(ASTnode_type));
				expectedTypesForIfData->type = ASTnodeType_type;
				expectedTypesForIfData->location = (SourceLocation){0, 0, 0};
				((ASTnode_type *)expectedTypesForIfData->value)->name = &(SubString){"Bool", strlen("Bool")};
				
				char *newExpressionSource = buildLLVM(globalBuilderInformation, variables, level, outerSource, outerName, expectedTypesForIf, data->expression, 1, 0);
				
				int jump1 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator newOuterSource = {100, 0, 0};
				CharAccumulator_initialize(&newOuterSource);
				free(buildLLVM(globalBuilderInformation, variables, level, &newOuterSource, outerName, NULL, data->codeBlock, 1, 0));
				int jump2 = outerFunction->registerCount;
				outerFunction->registerCount++;
				
				CharAccumulator_appendChars(outerSource, "\n\tbr ");
				CharAccumulator_appendChars(outerSource, newExpressionSource);
				CharAccumulator_appendChars(outerSource, ", label %");
				CharAccumulator_appendUint(outerSource, jump1);
				CharAccumulator_appendChars(outerSource, ", label %");
				CharAccumulator_appendUint(outerSource, jump2);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendUint(outerSource, jump1);
				CharAccumulator_appendChars(outerSource, ":");
				CharAccumulator_appendChars(outerSource, newOuterSource.data);
				CharAccumulator_appendChars(outerSource, "\n\tbr label %");
				CharAccumulator_appendUint(outerSource, jump2);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendUint(outerSource, jump2);
				CharAccumulator_appendChars(outerSource, ":");
				
				free(newExpressionSource);
				CharAccumulator_free(&newOuterSource);
				
				break;
			}
				
			case ASTnodeType_return: {
				if (outerSource == NULL || outerName == NULL) {
					printf("return in a weird spot\n");
					compileError(node->location);
				}
				
				Variable *outerFunctionVariable = getBuilderVariable(variables, level, outerName);
				
				if (outerFunctionVariable == NULL || outerFunctionVariable->type != VariableType_function) {
					abort();
				}
				
				Variable_function *outerFunction = (Variable_function *)outerFunctionVariable->value;
				
				ASTnode_return *data = (ASTnode_return *)node->value;
				
				outerFunction->hasReturned = 1;
				
				if (data->expression == NULL) {
					CharAccumulator_appendChars(outerSource, "\n\tret void");
				} else {
					char *newSource = buildLLVM(globalBuilderInformation, variables, level, outerSource, outerName, outerFunction->returnType, data->expression, 1, 0);
					
					CharAccumulator_appendChars(outerSource, "\n\tret ");
					CharAccumulator_appendChars(outerSource, newSource);
					
					free(newSource);
				}
				
				break;
			}
				
			case ASTnodeType_variableDefinition: {
				if (outerSource == NULL || outerName == NULL) {
					printf("variable definition in a weird spot\n");
					compileError(node->location);
				}
				
				ASTnode_variableDefinition *data = (ASTnode_variableDefinition *)node->value;
				
				Variable *variableVariable = getBuilderVariable(variables, level, data->name);
				if (variableVariable != NULL) {
					printf("The name '");
					SubString_print(data->name);
					printf("' is already used.\n");
					compileError(node->location);
				}
				
				Variable *outerFunctionVariable = getBuilderVariable(variables, level, outerName);
				if (outerFunctionVariable == NULL || outerFunctionVariable->type != VariableType_function) abort();
				Variable_function *outerFunction = (Variable_function *)outerFunctionVariable->value;
				
				char *LLVMtype = getLLVMtypeFromType(variables, level, data->type);
				
				char *newExpressionSource = buildLLVM(globalBuilderInformation, variables, level, outerSource, outerName, data->type, data->expression, 1, 0);
				
				CharAccumulator_appendChars(outerSource, "\n\t%");
				CharAccumulator_appendUint(outerSource, outerFunction->registerCount);
				CharAccumulator_appendChars(outerSource, " = alloca ");
				CharAccumulator_appendChars(outerSource, LLVMtype);
				// align pointers on eight bytes and everything else on four bytes
				if (strcmp(LLVMtype, "ptr") == 0) {
					CharAccumulator_appendChars(outerSource, ", align 8");
				} else {
					CharAccumulator_appendChars(outerSource, ", align 4");
				}
				
				CharAccumulator_appendChars(outerSource, "\n\tstore ");
				CharAccumulator_appendChars(outerSource, newExpressionSource);
				CharAccumulator_appendChars(outerSource, ", ptr %");
				CharAccumulator_appendUint(outerSource, outerFunction->registerCount);
				if (strcmp(LLVMtype, "ptr") == 0) {
					CharAccumulator_appendChars(outerSource, ", align 8");
				} else {
					CharAccumulator_appendChars(outerSource, ", align 4");
				}
				
				Variable *variableData = linkedList_addNode(&variables[level], sizeof(Variable) + sizeof(Variable_variable));
				
				variableData->key = data->name;
				
				variableData->type = VariableType_variable;
				
				((Variable_variable *)variableData->value)->LLVMRegister = outerFunction->registerCount;
				((Variable_variable *)variableData->value)->LLVMtype = LLVMtype;
				((Variable_variable *)variableData->value)->type = data->type;
				
				outerFunction->registerCount++;
				
				free(newExpressionSource);
				
				break;
			}
				
			case ASTnodeType_variableAssignment: {
				if (outerSource == NULL || outerName == NULL) {
					printf("variable assignment in a weird spot\n");
					compileError(node->location);
				}
				
				ASTnode_variableAssignment *data = (ASTnode_variableAssignment *)node->value;
				
				Variable *variableVariable = getBuilderVariable(variables, level, data->name);
				if (variableVariable == NULL || variableVariable->type != VariableType_variable) {
					printf("variable does not exist\n");
					compileError(node->location);
				}
				Variable_variable *variable = (Variable_variable *)variableVariable->value;
				
				char *newExpressionSource = buildLLVM(globalBuilderInformation, variables, level, outerSource, outerName, variable->type, data->expression, 1, 0);
				
				CharAccumulator_appendChars(outerSource, "\n\tstore ");
				CharAccumulator_appendChars(outerSource, newExpressionSource);
				CharAccumulator_appendChars(outerSource, ", ptr %");
				CharAccumulator_appendUint(outerSource, variable->LLVMRegister);
				if (strcmp(variable->LLVMtype, "ptr") == 0) {
					CharAccumulator_appendChars(outerSource, ", align 8");
				} else {
					CharAccumulator_appendChars(outerSource, ", align 4");
				}
				
				free(newExpressionSource);
				
				break;
			}
				
			case ASTnodeType_operator: {
				ASTnode_operator *data = (ASTnode_operator *)node->value;
				
				Variable *outerFunctionVariable = getBuilderVariable(variables, level, outerName);
				if (outerFunctionVariable == NULL || outerFunctionVariable->type != VariableType_function) abort();
				Variable_function *outerFunction = (Variable_function *)outerFunctionVariable->value;
				
				char *expectedLLVMtype = getLLVMtypeFromType(variables, level, expectedTypes);
				
				if (expectedTypes == NULL || ((ASTnode *)expectedTypes->data)->type != ASTnodeType_type) {
					printf("expected type '%s' but got type a number\n", getLLVMtypeFromType(variables, level, expectedTypes));
					compileError(node->location);
				}
				
				linkedList_Node *expectedTypeForLeftAndRight = NULL;
				ASTnode *expectedTypeData = linkedList_addNode(&expectedTypeForLeftAndRight, sizeof(ASTnode) + sizeof(ASTnode_type));
				memcpy(expectedTypeData, (ASTnode *)expectedTypes->data, sizeof(ASTnode) + sizeof(ASTnode_type));
				
				char *leftSource = buildLLVM(globalBuilderInformation, variables, level, outerSource, outerName, expectedTypeForLeftAndRight, data->left, 0, 0);
				char *rightSource = buildLLVM(globalBuilderInformation, variables, level, outerSource, outerName, expectedTypeForLeftAndRight, data->right, 0, 0);
				
				CharAccumulator_appendChars(outerSource, "\n\t%");
				CharAccumulator_appendUint(outerSource, outerFunction->registerCount);
				CharAccumulator_appendChars(outerSource, " = add nsw ");
				CharAccumulator_appendChars(outerSource, expectedLLVMtype);
				CharAccumulator_appendChars(outerSource, " ");
				CharAccumulator_appendChars(outerSource, leftSource);
				CharAccumulator_appendChars(outerSource, ", ");
				CharAccumulator_appendChars(outerSource, rightSource);
				
				if (withTypes) {
					CharAccumulator_appendChars(&LLVMsource, expectedLLVMtype);
					CharAccumulator_appendChars(&LLVMsource, " ");
				}
				CharAccumulator_appendChars(&LLVMsource, "%");
				CharAccumulator_appendUint(&LLVMsource, outerFunction->registerCount);
				
				free(leftSource);
				free(rightSource);
				linkedList_freeList(&expectedTypeForLeftAndRight);
				
				outerFunction->registerCount++;
				break;
			}
				
			case ASTnodeType_true: {
				// if no types are expected
				if (expectedTypes == NULL) {
					printf("unexpected bool\n");
					compileError(node->location);
				}
				
				SubString *expectedTypeName = ((ASTnode_type *)((ASTnode *)expectedTypes->data)->value)->name;
				
				if (SubString_string_cmp(expectedTypeName, "Bool") != 0) {
					printf("expected type '");
					SubString_print(expectedTypeName);
					printf("' but got type 'Bool'\n");
					compileError(node->location);
				}
				
				if (withTypes) {
					CharAccumulator_appendChars(&LLVMsource, "i1 ");
				}
				
				CharAccumulator_appendChars(&LLVMsource, "true");
				break;
			}

			case ASTnodeType_false: {
				// if no types are expected
				if (expectedTypes == NULL) {
					printf("unexpected bool\n");
					compileError(node->location);
				}
				
				SubString *expectedTypeName = ((ASTnode_type *)((ASTnode *)expectedTypes->data)->value)->name;
				
				if (SubString_string_cmp(expectedTypeName, "Bool") != 0) {
					printf("expected type '");
					SubString_print(expectedTypeName);
					printf("' but got type 'Bool'\n");
					compileError(node->location);
				}
				
				if (withTypes) {
					CharAccumulator_appendChars(&LLVMsource, "i1 ");
				}
				
				CharAccumulator_appendChars(&LLVMsource, "false");
				break;
			}
				
			case ASTnodeType_number: {
				ASTnode_number *data = (ASTnode_number *)node->value;
				
				if (expectedTypes == NULL) {
					printf("unexpected number\n");
					compileError(node->location);
				}
				
				if (((ASTnode *)expectedTypes->data)->type != ASTnodeType_type) abort();
				SubString *expectedTypeName = ((ASTnode_type *)((ASTnode *)expectedTypes->data)->value)->name;
				
				if (
					SubString_string_cmp(expectedTypeName, "Int8") != 0 &&
					SubString_string_cmp(expectedTypeName, "Int32") != 0 &&
					SubString_string_cmp(expectedTypeName, "Int64") != 0)
				{
					printf("expected type '");
					SubString_print(expectedTypeName);
					printf("' but got a number\n");
					compileError(node->location);
				}
				
				Variable *expectedTypeVariable = getBuilderVariable(variables, level, expectedTypeName);
				if (expectedTypeVariable == NULL || expectedTypeVariable->type != VariableType_type) abort();
				Variable_type *expectedType = (Variable_type *)expectedTypeVariable->value;
				
				if (withTypes) {
					CharAccumulator_appendChars(&LLVMsource, expectedType->LLVMtype);
					CharAccumulator_appendChars(&LLVMsource, " ");
				}
				CharAccumulator_appendSubString(&LLVMsource, data->string);
				break;
			}
				
			case ASTnodeType_string: {
				// @.str = private unnamed_addr constant [13 x i8] c"hello world\0A\00"
				
				ASTnode_string *data = (ASTnode_string *)node->value;
				
				if (expectedTypes == NULL) {
					printf("unexpected string\n");
					compileError(node->location);
				}
				
				SubString *expectedTypeName = ((ASTnode_type *)((ASTnode *)expectedTypes->data)->value)->name;
				
				if (SubString_string_cmp(expectedTypeName, "Pointer") != 0) {
					printf("expected type '");
					SubString_print(expectedTypeName);
					printf("' but got type 'Pointer'\n");
					compileError(node->location);
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
							compileError(node->location);
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
				
				CharAccumulator_appendChars(globalBuilderInformation->topLevelSource, "\n@.str.");
				CharAccumulator_appendUint(globalBuilderInformation->topLevelSource, globalBuilderInformation->stringCount);
				CharAccumulator_appendChars(globalBuilderInformation->topLevelSource, " = private unnamed_addr constant [");
				CharAccumulator_appendUint(globalBuilderInformation->topLevelSource, stringLength);
				CharAccumulator_appendChars(globalBuilderInformation->topLevelSource, " x i8] c\"");
				CharAccumulator_appendChars(globalBuilderInformation->topLevelSource, string.data);
				CharAccumulator_appendChars(globalBuilderInformation->topLevelSource, "\\00\""); // the \00 is the NULL byte
				
				if (withTypes) {
					CharAccumulator_appendChars(&LLVMsource, "ptr ");
				}
				CharAccumulator_appendChars(&LLVMsource, "@.str.");
				CharAccumulator_appendUint(&LLVMsource, globalBuilderInformation->stringCount);
				
				globalBuilderInformation->stringCount++;
				
				CharAccumulator_free(&string);
				
				break;
			}
				
			case ASTnodeType_variable: {
				// if no types are expected
				if (expectedTypes == NULL) {
					printf("unexpected variable\n");
					compileError(node->location);
				}
				
				ASTnode_variable *data = (ASTnode_variable *)node->value;
				
				Variable *outerFunctionVariable = getBuilderVariable(variables, level, outerName);
				if (outerFunctionVariable == NULL || outerFunctionVariable->type != VariableType_function) abort();
				Variable_function *outerFunction = (Variable_function *)outerFunctionVariable->value;
				
				Variable *variableVariable = getBuilderVariable(variables, level, data->name);
				if (variableVariable == NULL) {
					printf("no variable named '");
					SubString_print(data->name);
					printf("'\n");
					compileError(node->location);
				}
				Variable_variable *variable = (Variable_variable *)variableVariable->value;
				
				expectType((ASTnode *)expectedTypes->data, (ASTnode *)variable->type->data, node->location);
				
				CharAccumulator_appendChars(outerSource, "\n\t%");
				CharAccumulator_appendUint(outerSource, outerFunction->registerCount);
				CharAccumulator_appendChars(outerSource, " = load ");
				CharAccumulator_appendChars(outerSource, variable->LLVMtype);
				CharAccumulator_appendChars(outerSource, ", ptr %");
				CharAccumulator_appendUint(outerSource, variable->LLVMRegister);
				if (strcmp(variable->LLVMtype, "ptr") == 0) {
					CharAccumulator_appendChars(outerSource, ", align 8");
				} else {
					CharAccumulator_appendChars(outerSource, ", align 4");
				}
				
				if (withTypes) {
					CharAccumulator_appendChars(&LLVMsource, variable->LLVMtype);
					CharAccumulator_appendChars(&LLVMsource, " ");
				}
				CharAccumulator_appendChars(&LLVMsource, "%");
				CharAccumulator_appendUint(&LLVMsource, outerFunction->registerCount);
				
				outerFunction->registerCount++;
				
				break;
			}
			
			default: {
				printf("unknown node type: %u\n", node->type);
				compileError(node->location);
				break;
			}
		}
		
		if (withCommas && current->next != NULL) {
			CharAccumulator_appendChars(&LLVMsource, ",");
		}
		
		current = current->next;
	}
	
	linkedList_freeList(&variables[level]);
	
	return LLVMsource.data;
}
