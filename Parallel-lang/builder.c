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
			
			if (SubString_string_cmp(key, variable->key) != 0) {
				current = current->next;
				continue;
			}
			
			return variable;
		}
		
		index--;
	}
	
	return NULL;
}

char *buildLLVM(linkedList_Node **variables, int level, CharAccumulator *outerSource, SubString *outerName, linkedList_Node *expectedTypes, linkedList_Node *current) {
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
					
					free(buildLLVM(variables, level, &newOuterSource, data->name, NULL, data->codeBlock));
					
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
				
				char *LLVMarguments = buildLLVM(variables, level, outerSource, outerName, functionToCall->argumentTypes, data->arguments);
				
				CharAccumulator_appendChars(outerSource, "\n\t");
				CharAccumulator_appendChars(outerSource, "%");
				CharAccumulator_appendUint(outerSource, outerFunction->registerCount);
				CharAccumulator_appendChars(outerSource, " = call ");
				CharAccumulator_appendChars(outerSource, functionToCall->LLVMreturnType);
				CharAccumulator_appendChars(outerSource, " @");
				CharAccumulator_appendChars(outerSource, functionToCall->LLVMname);
				CharAccumulator_appendChars(outerSource, "(");
				CharAccumulator_appendChars(outerSource, LLVMarguments);
				CharAccumulator_appendChars(outerSource, ")");
				
				CharAccumulator_appendChars(&LLVMsource, functionToCall->LLVMreturnType);
				CharAccumulator_appendChars(&LLVMsource, " %");
				CharAccumulator_appendUint(&LLVMsource, outerFunction->registerCount);
				
				outerFunction->registerCount++;
				
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
				
				char *newExpressionSource = buildLLVM(variables, level, outerSource, outerName, expectedTypesForIf, data->expression);
				
				int jump1 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator newOuterSource = {100, 0, 0};
				CharAccumulator_initialize(&newOuterSource);
				free(buildLLVM(variables, level, &newOuterSource, outerName, expectedTypesForIf, data->codeBlock));
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
				
				char *newSource = buildLLVM(variables, level, outerSource, outerName, outerFunction->returnType, data->expression);
				
				CharAccumulator_appendChars(outerSource, "\n\tret ");
				CharAccumulator_appendChars(outerSource, newSource);
				
				free(newSource);
				
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
				
				CharAccumulator_appendChars(&LLVMsource, "i1 1");
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
				
				CharAccumulator_appendChars(&LLVMsource, "i1 0");
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
