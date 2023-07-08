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

void addTypeAsStringToList(linkedList_Node **list, char *string) {
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

char *getLLVMtypeFromType(linkedList_Node **variables, int level, linkedList_Node *type) {
	Variable *LLVMtypeVariable = getBuilderVariable(variables, level, ((ASTnode_type *)((ASTnode *)type->data)->value)->name);
	
	return ((Variable_type *)LLVMtypeVariable->value)->LLVMtype;
}

char *buildLLVM(GlobalBuilderInformation *globalBuilderInformation, linkedList_Node **variables, int level, CharAccumulator *outerSource, SubString *outerName, linkedList_Node *expectedTypes, linkedList_Node *current, int withTypes, int withCommas) {
	level++;
	if (level > maxVariablesLevel) {
		printf("level (%i) > maxVariablesLevel (%i)\n", level, maxVariablesLevel);
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
				
				break;
			}
				
			case ASTnodeType_call: {
				if (outerSource == NULL || outerName == NULL) {
					printf("function call in a weird spot\n");
					compileError(node->location);
				}
				
				break;
			}
				
			case ASTnodeType_while: {
				if (outerSource == NULL || outerName == NULL) {
					printf("while loop in a weird spot\n");
					compileError(node->location);
				}
				
				break;
			}
				
			case ASTnodeType_if: {
				if (outerSource == NULL || outerName == NULL) {
					printf("if statement in a weird spot\n");
					compileError(node->location);
				}
				
				break;
			}
				
			case ASTnodeType_return: {
				if (outerSource == NULL || outerName == NULL) {
					printf("return in a weird spot\n");
					compileError(node->location);
				}
				
				break;
			}
				
			case ASTnodeType_variableDefinition: {
				if (outerSource == NULL || outerName == NULL) {
					printf("variable definition in a weird spot\n");
					compileError(node->location);
				}
				
				break;
			}
				
			case ASTnodeType_variableAssignment: {
				if (outerSource == NULL || outerName == NULL) {
					printf("variable assignment in a weird spot\n");
					compileError(node->location);
				}
				
				break;
			}
				
			case ASTnodeType_operator: {
				if (expectedTypes == NULL) {
					printf("unexpected operator\n");
					compileError(node->location);
				}
				
				break;
			}
				
			case ASTnodeType_true: {
				if (expectedTypes == NULL) {
					printf("unexpected bool\n");
					compileError(node->location);
				}
				
				break;
			}

			case ASTnodeType_false: {
				if (expectedTypes == NULL) {
					printf("unexpected bool\n");
					compileError(node->location);
				}
				
				break;
			}
				
			case ASTnodeType_number: {
				if (expectedTypes == NULL) {
					printf("unexpected number\n");
					compileError(node->location);
				}
				
				break;
			}
				
			case ASTnodeType_string: {
				if (expectedTypes == NULL) {
					printf("unexpected string\n");
					compileError(node->location);
				}
				
				break;
			}
				
			case ASTnodeType_variable: {
				if (expectedTypes == NULL) {
					printf("unexpected variable\n");
					compileError(node->location);
				}
				
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
