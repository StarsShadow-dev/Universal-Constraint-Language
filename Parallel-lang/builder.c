#include <stdio.h>
#include <string.h>

#include "builder.h"
#include "error.h"

void addBuilderVariable_type(linkedList_Node **variables, char *key, char *LLVMname) {
	Variable *data = linkedList_addNode(variables, sizeof(Variable) + sizeof(Variable_type));
	
	data->key = key;
	
	data->type = VariableType_type;
	
	((Variable_type *)data->value)->LLVMname = LLVMname;
}

Variable *getBuilderVariable(linkedList_Node **variables, int level, SubString *key) {
	int index = level;
	while (index >= 0) {
		linkedList_Node *current = variables[index];
		
		while (current != NULL) {
			Variable *variable = ((Variable *)current->data);
			
			if (SubString_string_cmp(key, variable->key) == 0) {
				return variable;
			}
			
			current = current->next;
		}
		
		index--;
	}
	
	return NULL;
}

char *buildLLVM(GlobalBuilderInformation *globalBuilderInformation, linkedList_Node **variables, int level, CharAccumulator *outerSource, SubString *outerName, linkedList_Node *expectedTypes, linkedList_Node *current) {
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
				
				Variable *LLVMreturnTypeVariable = getBuilderVariable(variables, level, ((ASTnode_type *)((ASTnode *)data->returnType->data)->value)->name);
				
				Variable_type *LLVMreturnType = (Variable_type *)LLVMreturnTypeVariable->value;
				
				Variable *function = linkedList_addNode(&variables[0], sizeof(Variable) + sizeof(Variable_function));
				
				char *functionName = malloc(data->name->length + 1);
				memcpy(functionName, data->name->start, data->name->length);
				functionName[data->name->length] = 0;
//				char *functionLLVMname = malloc(data->name->length + 1);
				
				function->key = functionName;
				
				function->type = VariableType_function;
				
				((Variable_function *)function->value)->LLVMname = functionName;
				((Variable_function *)function->value)->LLVMreturnType = LLVMreturnType->LLVMname;
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
					
					free(buildLLVM(globalBuilderInformation, variables, level, &newOuterSource, data->name, NULL, data->codeBlock));
					
					if (!function->hasReturned) {
						printf("function did not return\n");
						compileError(node->location);
					}
					
					char *LLVMtype = ((Variable_type *)type->value)->LLVMname;
					CharAccumulator_appendChars(&LLVMsource, "\ndefine ");
					CharAccumulator_appendChars(&LLVMsource, LLVMtype);
					CharAccumulator_appendChars(&LLVMsource, " @");
					CharAccumulator_appendSubString(&LLVMsource, data->name);
					CharAccumulator_appendChars(&LLVMsource, "() {");
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
				
				char *LLVMarguments = buildLLVM(globalBuilderInformation, variables, level, outerSource, outerName, functionToCall->argumentTypes, data->arguments);
				
				if (strcmp(functionToCall->LLVMreturnType, "void") != 0) {
					CharAccumulator_appendChars(outerSource, "\n\t%");
					CharAccumulator_appendUint(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, " = call ");
					
					CharAccumulator_appendChars(&LLVMsource, functionToCall->LLVMreturnType);
					CharAccumulator_appendChars(&LLVMsource, " %");
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
				
				char *newExpressionSource = buildLLVM(globalBuilderInformation, variables, level, outerSource, outerName, expectedTypesForIf, data->expression);
				
				int jump1 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator newOuterSource = {100, 0, 0};
				CharAccumulator_initialize(&newOuterSource);
				free(buildLLVM(globalBuilderInformation, variables, level, &newOuterSource, outerName, NULL, data->codeBlock));
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
					char *newSource = buildLLVM(globalBuilderInformation, variables, level, outerSource, outerName, outerFunction->returnType, data->expression);
					
					CharAccumulator_appendChars(outerSource, "\n\tret ");
					CharAccumulator_appendChars(outerSource, newSource);
					
					free(newSource);
				}
				
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
				
				CharAccumulator_appendChars(&LLVMsource, "i1 true");
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
				
				CharAccumulator_appendChars(&LLVMsource, "i1 false");
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
				
				CharAccumulator_appendChars(&LLVMsource, expectedType->LLVMname);
				CharAccumulator_appendChars(&LLVMsource, " ");
				CharAccumulator_appendSubString(&LLVMsource, data->string);
				break;
			}
				
			case ASTnodeType_string: {
				ASTnode_string *data = (ASTnode_string *)node->value;
				
				// @.str = private unnamed_addr constant [13 x i8] c"hello world\0A\00"
				
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
				
				CharAccumulator_appendChars(globalBuilderInformation->topLevelSource, "@.str.");
				CharAccumulator_appendUint(globalBuilderInformation->topLevelSource, globalBuilderInformation->stringCount);
				CharAccumulator_appendChars(globalBuilderInformation->topLevelSource, " = private unnamed_addr constant [");
				CharAccumulator_appendUint(globalBuilderInformation->topLevelSource, stringLength);
				CharAccumulator_appendChars(globalBuilderInformation->topLevelSource, " x i8] c\"");
				CharAccumulator_appendChars(globalBuilderInformation->topLevelSource, string.data);
				CharAccumulator_appendChars(globalBuilderInformation->topLevelSource, "\\00\""); // the \00 is the NULL byte
				
				CharAccumulator_appendChars(&LLVMsource, "ptr @.str.");
				CharAccumulator_appendUint(&LLVMsource, globalBuilderInformation->stringCount);
				
				globalBuilderInformation->stringCount++;
				
				CharAccumulator_free(&string);
				
				break;
			}
			
			default: {
				printf("unknown node type: %u\n", node->type);
				compileError(node->location);
				break;
			}
		}
		
		current = current->next;
	}
	
	return LLVMsource.data;
}
