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

Variable *getBuilderVariable(linkedList_Node **variables, int level, char *key) {
	int index = level;
	while (index >= 0) {
		linkedList_Node *current = variables[index];
		
		while (current != NULL) {
			Variable *variable = ((Variable *)current->data);
			
			if (strcmp(variable->key, key) != 0) {
				current = current->next;
				continue;
			}
			
			return variable;
		}
		
		index--;
	}
	
	return NULL;
}

char *buildLLVM(linkedList_Node **variables, int level, String *outerSource, char *outerName, linkedList_Node *expectedTypes, linkedList_Node *current) {
	level++;
	if (level > maxBuilderLevel) {
		printf("level (%i) > maxBuilderLevel (%i)\n", level, maxBuilderLevel);
		abort();
	}
	
	String LLVMsource = {100, 0, 0};
	String_initialize(&LLVMsource);
	
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
				
				String newOuterSource = {100, 0, 0};
				String_initialize(&newOuterSource);
				
				Variable *type = getBuilderVariable(variables, level, ((ASTnode_type *)((ASTnode *)data->returnType->data)->value)->name);
				
				if (type == NULL) {
					printf("unknown type: '%s'\n", ((ASTnode_type *)((ASTnode *)data->returnType->data)->value)->name);
					compileError(((ASTnode *)data->returnType->data)->location);
				}
				
				if (type->type != VariableType_type) {
					abort();
				}
				
				Variable *LLVMreturnTypeVariable = getBuilderVariable(variables, level, ((ASTnode_type *)((ASTnode *)data->returnType->data)->value)->name);
				
				Variable_type *LLVMreturnType = (Variable_type *)LLVMreturnTypeVariable->value;
				
				Variable *function = linkedList_addNode(&variables[0], sizeof(Variable) + sizeof(Variable_function));
				
				function->key = data->name;
				
				function->type = VariableType_function;
				
				((Variable_function *)function->value)->LLVMname = data->name;
				((Variable_function *)function->value)->LLVMreturnType = LLVMreturnType->LLVMname;
				((Variable_function *)function->value)->argumentTypes = data->argumentTypes;
				((Variable_function *)function->value)->returnType = data->returnType;
				((Variable_function *)function->value)->hasReturned = 0;
				((Variable_function *)function->value)->registerCount = 1;
				
				if (data->external) {
					String_appendChars(&LLVMsource, ((ASTnode_string *)(((ASTnode *)(data->codeBlock)->data)->value))->string);
				} else {
					free(buildLLVM(variables, level, &newOuterSource, data->name, NULL, data->codeBlock));
					
					if (!((Variable_function *)function->value)->hasReturned) {
						printf("function did not return\n");
						compileError(node->location);
					}
					
					char *LLVMtype = ((Variable_type *)type->value)->LLVMname;
					String_appendChars(&LLVMsource, "\ndefine ");
					String_appendChars(&LLVMsource, LLVMtype);
					String_appendChars(&LLVMsource, " @");
					String_appendChars(&LLVMsource, data->name);
					String_appendChars(&LLVMsource, "() {");
					String_appendChars(&LLVMsource, newOuterSource.data);
					String_appendChars(&LLVMsource, "\n}");
					
					String_free(&newOuterSource);
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
					printf("no function named '%s'\n", data->name);
					compileError(node->location);
				}
				Variable_function *functionToCall = (Variable_function *)functionToCallVariable->value;
				
				int expectedArgumentCount = linkedList_getCount(&functionToCall->argumentTypes);
				int actualArgumentCount = linkedList_getCount(&data->arguments);
				
				if (expectedArgumentCount > actualArgumentCount) {
					printf("%s did not get enough arguments (expected %d but got %d)\n", data->name, expectedArgumentCount, actualArgumentCount);
					compileError(node->location);
				}
				if (expectedArgumentCount < actualArgumentCount) {
					printf("%s got too many arguments (expected %d but got %d)\n", data->name, expectedArgumentCount, actualArgumentCount);
					compileError(node->location);
				}
				
				char *LLVMarguments = buildLLVM(variables, level, outerSource, outerName, functionToCall->argumentTypes, data->arguments);
				
				String_appendChars(outerSource, "\n\tcall ");
				String_appendChars(outerSource, functionToCall->LLVMreturnType);
				String_appendChars(outerSource, " @");
				String_appendChars(outerSource, functionToCall->LLVMname);
				String_appendChars(outerSource, "(");
				String_appendChars(outerSource, LLVMarguments);
				String_appendChars(outerSource, ")");
				
//				String_appendChars(&LLVMsource, "%");
//				String_appendUint(&LLVMsource, outerFunction->registerCount);
				
				outerFunction->registerCount++;
				
				free(LLVMarguments);
				
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
				
				String_appendChars(outerSource, "\n\tret ");
				String_appendChars(outerSource, newSource);
				
				free(newSource);
				
				break;
			}
				
//			case ASTnodeType_type: {
//				break;
//			}
				
			case ASTnodeType_number: {
				ASTnode_number *data = (ASTnode_number *)node->value;
				
				if (expectedTypes == NULL) {
					printf("unexpected number\n");
					compileError(node->location);
				}
				
				if (((ASTnode *)expectedTypes->data)->type != ASTnodeType_type) {
					abort();
				}
				
				char *expectedTypeName = ((ASTnode_type *)((ASTnode *)expectedTypes->data)->value)->name;
				
				if (
					strcmp(expectedTypeName, "Int8") != 0 &&
					strcmp(expectedTypeName, "Int32") != 0 &&
					strcmp(expectedTypeName, "Int64") != 0)
				{
					printf("expected type '%s' but got a number\n", expectedTypeName);
					compileError(node->location);
				}
				
				Variable *expectedTypeVariable = getBuilderVariable(variables, level, expectedTypeName);
				if (expectedTypeVariable == NULL || expectedTypeVariable->type != VariableType_type) abort();
				Variable_type *expectedType = (Variable_type *)expectedTypeVariable->value;
				
				String_appendChars(&LLVMsource, expectedType->LLVMname);
				
				String_appendChars(&LLVMsource, " ");
				
				String_appendChars(&LLVMsource, data->string);
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
