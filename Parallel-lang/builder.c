#include <stdio.h>
#include <string.h>

#include "globals.h"
#include "builder.h"
#include "error.h"

int expectTypeWithType(ASTnode *expectedTypeNode, ASTnode *actualTypeNode) {
	if (expectedTypeNode->nodeType != ASTnodeType_type || actualTypeNode->nodeType != ASTnodeType_type) {
		abort();
	}
	
	ASTnode_type *expectedType = (ASTnode_type *)expectedTypeNode->value;
	ASTnode_type *actualType = (ASTnode_type *)actualTypeNode->value;
	
	if (SubString_SubString_cmp(expectedType->name, actualType->name) != 0) {
		return 1;
	}
	
	return 0;
}

int expectTypeWithString(ASTnode *expectedTypeNode, char *actualTypeString) {
	if (expectedTypeNode->nodeType != ASTnodeType_type) {
		abort();
	}
	
	ASTnode_type *expectedType = (ASTnode_type *)expectedTypeNode->value;
	
	if (SubString_string_cmp(expectedType->name, actualTypeString) != 0) {
		return 1;
	}
	
	return 0;
}

void addTypeAsString(linkedList_Node **list, char *string) {
	SubString *subString = malloc(sizeof(SubString));
	if (subString == NULL) {
		printf("malloc failed\n");
		abort();
	}
	*subString = (SubString){string, (int)strlen(string)};
	
	ASTnode *data = linkedList_addNode(list, sizeof(ASTnode) + sizeof(ASTnode_type));
	data->nodeType = ASTnodeType_type;
	data->location = (SourceLocation){0, 0, 0};
	((ASTnode_type *)data->value)->name = subString;
}

void addTypeAsType(linkedList_Node **list, ASTnode *typeNode) {
	ASTnode *data = linkedList_addNode(list, sizeof(ASTnode) + sizeof(ASTnode_type));
	data->nodeType = ASTnodeType_type;
	data->location = (SourceLocation){0, 0, 0};
	memcpy(data->value, typeNode->value, sizeof(ASTnode_type));
}

void addBuilderType(linkedList_Node **variables, SubString *key, char *LLVMtype, int byteSize) {
	Variable *data = linkedList_addNode(variables, sizeof(Variable) + sizeof(Variable_type));
	
	data->key = key;
	data->type = VariableType_type;
	data->byteSize = byteSize;
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
	
	if (LLVMtypeVariable->type == VariableType_type) {
		return ((Variable_type *)LLVMtypeVariable->value)->LLVMtype;
	} else if (LLVMtypeVariable->type == VariableType_struct) {
		return ((Variable_struct *)LLVMtypeVariable->value)->LLVMname;
	}
	
	abort();
}

void generateFunction(GlobalBuilderInformation *GBI, CharAccumulator *outerSource, Variable_function *function, ASTnode *node) {
	ASTnode_function *data = (ASTnode_function *)node->value;
	
	if (data->external) {
		CharAccumulator_appendSubString(outerSource, ((ASTnode_string *)(((ASTnode *)(data->codeBlock)->data)->value))->value);
	}
	
	else {
		char *LLVMreturnType = getLLVMtypeFromType(GBI->variables, GBI->level, (ASTnode *)data->returnType->data);
		
		CharAccumulator_appendChars(outerSource, "\n\ndefine ");
		CharAccumulator_appendChars(outerSource, LLVMreturnType);
		CharAccumulator_appendChars(outerSource, " @");
		CharAccumulator_appendChars(outerSource, function->LLVMname);
		CharAccumulator_appendChars(outerSource, "(");
		
		CharAccumulator functionSource = {100, 0, 0};
		CharAccumulator_initialize(&functionSource);
		
		linkedList_Node *currentArgument = data->argumentTypes;
		linkedList_Node *currentArgumentName = data->argumentNames;
		if (currentArgument != NULL) {
			int argumentCount =  linkedList_getCount(&data->argumentTypes);
			while (1) {
				char *currentArgumentLLVMtype = getLLVMtypeFromType(GBI->variables, GBI->level, (ASTnode *)currentArgument->data);
				CharAccumulator_appendChars(outerSource, currentArgumentLLVMtype);
				CharAccumulator_appendChars(outerSource, " %");
				CharAccumulator_appendUint(outerSource, function->registerCount);
				
				CharAccumulator_appendChars(&functionSource, "\n\t%");
				CharAccumulator_appendUint(&functionSource, function->registerCount + argumentCount + 1);
				CharAccumulator_appendChars(&functionSource, " = alloca ");
				CharAccumulator_appendChars(&functionSource, currentArgumentLLVMtype);
				// align pointers on eight bytes and everything else on four bytes
				if (strcmp(currentArgumentLLVMtype, "ptr") == 0) {
					CharAccumulator_appendChars(&functionSource, ", align 8");
				} else {
					CharAccumulator_appendChars(&functionSource, ", align 4");
				}
				
				CharAccumulator_appendChars(&functionSource, "\n\tstore ");
				CharAccumulator_appendChars(&functionSource, currentArgumentLLVMtype);
				CharAccumulator_appendChars(&functionSource, " %");
				CharAccumulator_appendUint(&functionSource, function->registerCount);
				CharAccumulator_appendChars(&functionSource, ", ptr %");
				CharAccumulator_appendUint(&functionSource, function->registerCount + argumentCount + 1);
				// align pointers on eight bytes and everything else on four bytes
				if (strcmp(currentArgumentLLVMtype, "ptr") == 0) {
					CharAccumulator_appendChars(&functionSource, ", align 8");
				} else {
					CharAccumulator_appendChars(&functionSource, ", align 4");
				}
				
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
				
				CharAccumulator_appendChars(outerSource, ", ");
				currentArgument = currentArgument->next;
				currentArgumentName = currentArgumentName->next;
			}
			
			function->registerCount += argumentCount;
		}
		
		function->registerCount++;
		
		CharAccumulator_appendChars(outerSource, ") {");
		CharAccumulator_appendChars(outerSource, functionSource.data);
		buildLLVM(GBI, function, outerSource, NULL, NULL, NULL, data->codeBlock, 0, 0);
		
		if (!function->hasReturned) {
			printf("function did not return\n");
			compileError(node->location);
		}
		
		CharAccumulator_appendChars(outerSource, "\n}");
		
		CharAccumulator_free(&functionSource);
	}
}

void buildLLVM(GlobalBuilderInformation *GBI, Variable_function *outerFunction, CharAccumulator *outerSource, CharAccumulator *innerSource, linkedList_Node *expectedTypes, linkedList_Node **types, linkedList_Node *current, int withTypes, int withCommas) {
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
		while (preLoopCurrent != NULL) {
			ASTnode *node = ((ASTnode *)preLoopCurrent->data);
			
			if (node->nodeType == ASTnodeType_function) {
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
				// I do not think I need to set byteSize to anything specific
				function->byteSize = 0;
				
				((Variable_function *)function->value)->LLVMname = functionLLVMname;
				((Variable_function *)function->value)->LLVMreturnType = LLVMreturnType;
				((Variable_function *)function->value)->argumentTypes = data->argumentTypes;
				((Variable_function *)function->value)->returnType = data->returnType;
				((Variable_function *)function->value)->hasReturned = 0;
				((Variable_function *)function->value)->registerCount = 0;
			}
			
			else if (node->nodeType == ASTnodeType_struct) {
				ASTnode_struct *data = (ASTnode_struct *)node->value;
				
				Variable *structVariableData = linkedList_addNode(&GBI->variables[GBI->level], sizeof(Variable) + sizeof(Variable_struct));
				structVariableData->key = data->name;
				structVariableData->type = VariableType_struct;
				structVariableData->byteSize = 0;
				
				int strlength = strlen("%struct.");
				
				char *LLVMname = malloc(strlength + data->name->length + 1);
				memcpy(LLVMname, "%struct.", strlength);
				memcpy(LLVMname + strlength, data->name->start, data->name->length);
				LLVMname[strlength + data->name->length] = 0;
				
				((Variable_struct *)structVariableData->value)->LLVMname = LLVMname;
				((Variable_struct *)structVariableData->value)->hasPointer = 0;
				
				CharAccumulator_appendChars(outerSource, "\n\n%struct.");
				CharAccumulator_appendSubString(outerSource, data->name);
				CharAccumulator_appendChars(outerSource, " = type { ");
				
				CharAccumulator methodSource = {100, 0, 0};
				CharAccumulator_initialize(&methodSource);
				
				linkedList_Node *currentMember = data->block;
				while (currentMember != NULL) {
					ASTnode *node = ((ASTnode *)currentMember->data);
					
					if (node->nodeType == ASTnodeType_variableDefinition) {
						ASTnode_variableDefinition *memberData = (ASTnode_variableDefinition *)node->value;
						
						char *LLVMtype = getLLVMtypeFromType(GBI->variables, GBI->level, (ASTnode *)memberData->type->data);
						
						Variable *type = getBuilderVariable(GBI->variables, GBI->level, ((ASTnode_type *)((ASTnode *)memberData->type->data)->value)->name);
						if (type == NULL) {
							printf("unknown type: '");
							SubString_print(((ASTnode_type *)((ASTnode *)memberData->type->data)->value)->name);
							printf("'\n");
							compileError(((ASTnode *)memberData->type->data)->location);
						}
						
						if (strcmp(LLVMtype, "ptr") == 0) {
							((Variable_struct *)structVariableData->value)->hasPointer = 1;
						}
						
						structVariableData->byteSize += type->byteSize;
						
						CharAccumulator_appendChars(outerSource, LLVMtype);
						
						Variable *variableData = linkedList_addNode(&((Variable_struct *)structVariableData->value)->members, sizeof(Variable) + sizeof(Variable_variable));
						
						variableData->key = data->name;
						variableData->type = VariableType_variable;
						
						// TODO: LLVMRegister
						((Variable_variable *)variableData->value)->LLVMRegister = 0;
						((Variable_variable *)variableData->value)->LLVMtype = LLVMtype;
						((Variable_variable *)variableData->value)->type = memberData->type;
						
					} else if (node->nodeType == ASTnodeType_function) {
						ASTnode_function *memberData = (ASTnode_function *)node->value;
						
						// make sure that the return type actually exists
						Variable *type = getBuilderVariable(GBI->variables, GBI->level, ((ASTnode_type *)((ASTnode *)memberData->returnType->data)->value)->name);
						if (type == NULL) {
							printf("unknown type: '");
							SubString_print(((ASTnode_type *)((ASTnode *)memberData->returnType->data)->value)->name);
							printf("'\n");
							compileError(((ASTnode *)memberData->returnType->data)->location);
						}
						
						if (type->type != VariableType_type) {
							abort();
						}
						
						// make sure that the types of all arguments actually exist
						linkedList_Node *currentArgument = memberData->argumentTypes;
						while (currentArgument != NULL) {
							Variable *currentArgumentType = getBuilderVariable(GBI->variables, GBI->level, ((ASTnode_type *)((ASTnode *)currentArgument->data)->value)->name);
							if (currentArgumentType == NULL) {
								printf("unknown type: '");
								SubString_print(((ASTnode_type *)((ASTnode *)currentArgument->data)->value)->name);
								printf("'\n");
								compileError(((ASTnode *)currentArgument->data)->location);
							}
							
							currentArgument = currentArgument->next;
						}
						
						char *LLVMreturnType = getLLVMtypeFromType(GBI->variables, GBI->level, (ASTnode *)memberData->returnType->data);
						
						Variable *function = linkedList_addNode(&((Variable_struct *)structVariableData->value)->members, sizeof(Variable) + sizeof(Variable_function));
						
						// TODO: this is a mess
						int functionLLVMnameLength = data->name->length + memberData->name->length;
						char *functionLLVMname = malloc(functionLLVMnameLength + 2);
						memcpy(functionLLVMname, data->name->start, data->name->length);
						*((char *)functionLLVMname + memberData->name->length - 1) = '.';
						memcpy(functionLLVMname + memberData->name->length, memberData->name->start, memberData->name->length);
						functionLLVMname[functionLLVMnameLength + 1] = 0;
						
						function->key = data->name;
						function->type = VariableType_function;
						
						((Variable_function *)function->value)->LLVMname = functionLLVMname;
						((Variable_function *)function->value)->LLVMreturnType = LLVMreturnType;
						((Variable_function *)function->value)->argumentTypes = memberData->argumentTypes;
						((Variable_function *)function->value)->returnType = memberData->returnType;
						((Variable_function *)function->value)->hasReturned = 0;
						((Variable_function *)function->value)->registerCount = 0;
						
						generateFunction(GBI, &methodSource, (Variable_function *)function->value, node);
					} else {
						abort();
					}
					
					currentMember = currentMember->next;
				}
				
				CharAccumulator_appendChars(outerSource, " }");
				
				CharAccumulator_appendChars(outerSource, methodSource.data);
				
				CharAccumulator_free(&methodSource);
			}
			
			preLoopCurrent = preLoopCurrent->next;
		}
	}
	
	//
	// main loop
	//
	
	while (current != NULL) {
		ASTnode *node = ((ASTnode *)current->data);
		
		switch (node->nodeType) {
			case ASTnodeType_function: {
				if (GBI->level != 0) {
					printf("function definitions are only allowed at top level\n");
					compileError(node->location);
				}
				
				ASTnode_function *data = (ASTnode_function *)node->value;
				
				Variable *functionVariable = getBuilderVariable(GBI->variables, GBI->level, data->name);
				if (functionVariable == NULL || functionVariable->type != VariableType_function) abort();
				Variable_function *function = (Variable_function *)functionVariable->value;
				
				generateFunction(GBI, outerSource, function, node);
				
				break;
			}
			
			case ASTnodeType_struct: {
				if (GBI->level != 0) {
					printf("struct definitions are only allowed at top level\n");
					compileError(node->location);
				}
				break;
			}
			
			case ASTnodeType_new: {
				ASTnode_new *data = (ASTnode_new *)node->value;
				
				Variable *structVariable = getBuilderVariable(GBI->variables, GBI->level, data->name);
				if (structVariable == NULL || structVariable->type != VariableType_struct) abort();
				Variable_struct *structToInit = (Variable_struct *)structVariable->value;
				
				CharAccumulator_appendChars(outerSource, "\n\t%");
				CharAccumulator_appendUint(outerSource, outerFunction->registerCount);
				CharAccumulator_appendChars(outerSource, " = call ptr @malloc(i64 noundef ");
				CharAccumulator_appendUint(outerSource, structVariable->byteSize);
				CharAccumulator_appendChars(outerSource, ") allocsize(0)");
				
				CharAccumulator_appendChars(outerSource, "\n\tcall void @");
				
				linkedList_Node *currentMember = structToInit->members;
				while (1) {
					if (currentMember == NULL) {
						printf("tried to initialize a struct that does not have an initializer");
						compileError(node->location);
					}
					Variable *variable = (Variable *)currentMember->data;
					
					if (variable->type == VariableType_function && SubString_string_cmp(variable->key, "__init")) {
						Variable_function *variableFunction = (Variable_function *)variable->value;
						
						CharAccumulator_appendChars(outerSource, variableFunction->LLVMname);
						break;
					}
					
					currentMember = currentMember->next;
				}
				
				CharAccumulator_appendChars(outerSource, "(ptr %");
				CharAccumulator_appendUint(outerSource, outerFunction->registerCount);
				CharAccumulator_appendChars(outerSource, ")");
				
				CharAccumulator_appendChars(innerSource, "ptr %");
				CharAccumulator_appendUint(innerSource, outerFunction->registerCount);
				
				outerFunction->registerCount++;
				
				break;
			}
			
			case ASTnodeType_call: {
				if (outerFunction == NULL) {
					printf("function calls are only allowed in a function\n");
					compileError(node->location);
				}
				
				ASTnode_call *data = (ASTnode_call *)node->value;
				
				Variable *functionToCallVariable = getBuilderVariable(GBI->variables, GBI->level, data->name);
				if (functionToCallVariable == NULL || functionToCallVariable->type != VariableType_function) {
					printf("no function named '");
					SubString_print(data->name);
					printf("'\n");
					compileError(node->location);
				}
				Variable_function *functionToCall = (Variable_function *)functionToCallVariable->value;
				
				if (types != NULL) {
					addTypeAsType(types, (ASTnode *)functionToCall->returnType->data);
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
					
					buildLLVM(GBI, outerFunction, outerSource, &newInnerSource, functionToCall->argumentTypes, NULL, data->arguments, 1, 1);
					
					if (strcmp(functionToCall->LLVMreturnType, "void") != 0) {
						CharAccumulator_appendChars(outerSource, "\n\t%");
						CharAccumulator_appendUint(outerSource, outerFunction->registerCount);
						CharAccumulator_appendChars(outerSource, " = call ");
						
						if (innerSource != NULL) {
							if (withTypes) {
								CharAccumulator_appendChars(innerSource, functionToCall->LLVMreturnType);
								CharAccumulator_appendChars(innerSource, " ");
							}
							CharAccumulator_appendChars(innerSource, "%");
							CharAccumulator_appendUint(innerSource, outerFunction->registerCount);
						}
						
						outerFunction->registerCount++;
					} else {
						CharAccumulator_appendChars(outerSource, "\n\tcall ");
					}
					CharAccumulator_appendChars(outerSource, functionToCall->LLVMreturnType);
					CharAccumulator_appendChars(outerSource, " @");
					CharAccumulator_appendChars(outerSource, functionToCall->LLVMname);
					CharAccumulator_appendChars(outerSource, "(");
					CharAccumulator_appendChars(outerSource, newInnerSource.data);
					CharAccumulator_appendChars(outerSource, ")");
					
					CharAccumulator_free(&newInnerSource);
				}
				
				break;
			}
				
			case ASTnodeType_while: {
				if (outerFunction == NULL) {
					printf("while loops are only allowed in a function\n");
					compileError(node->location);
				}
				
				ASTnode_while *data = (ASTnode_while *)node->value;
				
				linkedList_Node *expectedTypesForWhile = NULL;
				addTypeAsString(&expectedTypesForWhile, "Bool");
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				int jump1 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator jump1_outerSource = {100, 0, 0};
				CharAccumulator_initialize(&jump1_outerSource);
				buildLLVM(GBI, outerFunction, &jump1_outerSource, &expressionSource, expectedTypesForWhile, NULL, data->expression, 1, 0);
				
				int jump2 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator jump2_outerSource = {100, 0, 0};
				CharAccumulator_initialize(&jump2_outerSource);
				buildLLVM(GBI, outerFunction, &jump2_outerSource, NULL, NULL, NULL, data->codeBlock, 1, 0);
				
				int endJump = outerFunction->registerCount;
				outerFunction->registerCount++;
				
				CharAccumulator_appendChars(outerSource, "\n\tbr label %");
				CharAccumulator_appendUint(outerSource, jump1);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendUint(outerSource, jump1);
				CharAccumulator_appendChars(outerSource, ":");
				CharAccumulator_appendChars(outerSource, jump1_outerSource.data);
				CharAccumulator_appendChars(outerSource, "\n\tbr ");
				CharAccumulator_appendChars(outerSource, expressionSource.data);
				CharAccumulator_appendChars(outerSource, ", label %");
				CharAccumulator_appendUint(outerSource, jump2);
				CharAccumulator_appendChars(outerSource, ", label %");
				CharAccumulator_appendUint(outerSource, endJump);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendUint(outerSource, jump2);
				CharAccumulator_appendChars(outerSource, ":");
				CharAccumulator_appendChars(outerSource, jump2_outerSource.data);
				CharAccumulator_appendChars(outerSource, "\n\tbr label %");
				CharAccumulator_appendUint(outerSource, jump1);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendUint(outerSource, endJump);
				CharAccumulator_appendChars(outerSource, ":");
				
				CharAccumulator_free(&expressionSource);
				CharAccumulator_free(&jump1_outerSource);
				CharAccumulator_free(&jump2_outerSource);
				
				break;
			}
			
			case ASTnodeType_if: {
				if (outerFunction == NULL) {
					printf("if statements are only allowed in a function\n");
					compileError(node->location);
				}
				
				ASTnode_if *data = (ASTnode_if *)node->value;
				
				linkedList_Node *expectedTypesForIf = NULL;
				addTypeAsString(&expectedTypesForIf, "Bool");
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				buildLLVM(GBI, outerFunction, outerSource, &expressionSource, expectedTypesForIf, NULL, data->expression, 1, 0);
				
				int jump1 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator codeBlockSource = {100, 0, 0};
				CharAccumulator_initialize(&codeBlockSource);
				buildLLVM(GBI, outerFunction, &codeBlockSource, NULL, NULL, NULL, data->codeBlock, 0, 0);
				int jump2 = outerFunction->registerCount;
				outerFunction->registerCount++;
				
				CharAccumulator_appendChars(outerSource, "\n\tbr ");
				CharAccumulator_appendChars(outerSource, expressionSource.data);
				CharAccumulator_appendChars(outerSource, ", label %");
				CharAccumulator_appendUint(outerSource, jump1);
				CharAccumulator_appendChars(outerSource, ", label %");
				CharAccumulator_appendUint(outerSource, jump2);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendUint(outerSource, jump1);
				CharAccumulator_appendChars(outerSource, ":");
				CharAccumulator_appendChars(outerSource, codeBlockSource.data);
				CharAccumulator_appendChars(outerSource, "\n\tbr label %");
				CharAccumulator_appendUint(outerSource, jump2);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendUint(outerSource, jump2);
				CharAccumulator_appendChars(outerSource, ":");
				
				CharAccumulator_free(&expressionSource);
				
				break;
			}
			
			case ASTnodeType_return: {
				if (outerFunction == NULL) {
					printf("return statements are only allowed in a function\n");
					compileError(node->location);
				}
				
				ASTnode_return *data = (ASTnode_return *)node->value;
				
				if (data->expression == NULL) {
					CharAccumulator_appendChars(outerSource, "\n\tret void");
				} else {
					CharAccumulator newInnerSource = {100, 0, 0};
					CharAccumulator_initialize(&newInnerSource);
					
					buildLLVM(GBI, outerFunction, outerSource, &newInnerSource, outerFunction->returnType, NULL, data->expression, 1, 0);
					
					CharAccumulator_appendChars(outerSource, "\n\tret ");
					
					CharAccumulator_appendChars(outerSource, newInnerSource.data);
					
					CharAccumulator_free(&newInnerSource);
				}
				
				outerFunction->hasReturned = 1;
				
				break;
			}
				
			case ASTnodeType_variableDefinition: {
				if (outerFunction == NULL) {
					printf("variable definitions are only allowed in a function\n");
					compileError(node->location);
				}
				
				ASTnode_variableDefinition *data = (ASTnode_variableDefinition *)node->value;
				
				if (data->expression == NULL) {
					printf("variable not initialized with a value\n");
					compileError(node->location);
				}
				
				Variable *variableVariable = getBuilderVariable(GBI->variables, GBI->level, data->name);
				if (variableVariable != NULL) {
					printf("The name '");
					SubString_print(data->name);
					printf("' is already used.\n");
					compileError(node->location);
				}
				
				Variable *type = getBuilderVariable(GBI->variables, GBI->level, ((ASTnode_type *)((ASTnode *)data->type->data)->value)->name);
				if (type == NULL) {
					printf("unknown type: '");
					SubString_print(((ASTnode_type *)((ASTnode *)data->type->data)->value)->name);
					printf("'\n");
					compileError(((ASTnode *)data->type->data)->location);
				}
				
				char *LLVMtype = getLLVMtypeFromType(GBI->variables, GBI->level, (ASTnode *)data->type->data);
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				buildLLVM(GBI, outerFunction, outerSource, &expressionSource, data->type, NULL, data->expression, 1, 0);
				
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
				CharAccumulator_appendChars(outerSource, expressionSource.data);
				CharAccumulator_appendChars(outerSource, ", ptr %");
				CharAccumulator_appendUint(outerSource, outerFunction->registerCount);
				// align pointers on eight bytes and everything else on four bytes
				if (strcmp(LLVMtype, "ptr") == 0) {
					CharAccumulator_appendChars(outerSource, ", align 8");
				} else {
					CharAccumulator_appendChars(outerSource, ", align 4");
				}
				
				Variable *variableData = linkedList_addNode(&GBI->variables[GBI->level], sizeof(Variable) + sizeof(Variable_variable));
				
				variableData->key = data->name;
				variableData->type = VariableType_variable;
				
				((Variable_variable *)variableData->value)->LLVMRegister = outerFunction->registerCount;
				((Variable_variable *)variableData->value)->LLVMtype = LLVMtype;
				((Variable_variable *)variableData->value)->type = data->type;
				
				outerFunction->registerCount++;
				
				CharAccumulator_free(&expressionSource);
				
				break;
			}
			
			case ASTnodeType_variableAssignment: {
				if (outerFunction == NULL) {
					printf("variable assignments are only allowed in a function\n");
					compileError(node->location);
				}
				
				ASTnode_variableAssignment *data = (ASTnode_variableAssignment *)node->value;
				
				Variable *variableVariable = getBuilderVariable(GBI->variables, GBI->level, data->name);
				if (variableVariable == NULL || variableVariable->type != VariableType_variable) {
					printf("variable does not exist\n");
					compileError(node->location);
				}
				Variable_variable *variable = (Variable_variable *)variableVariable->value;
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				buildLLVM(GBI, outerFunction, outerSource, &expressionSource, variable->type, NULL, data->expression, 1, 0);
				
				CharAccumulator_appendChars(outerSource, "\n\tstore ");
				CharAccumulator_appendChars(outerSource, expressionSource.data);
				CharAccumulator_appendChars(outerSource, ", ptr %");
				CharAccumulator_appendUint(outerSource, variable->LLVMRegister);
				if (strcmp(variable->LLVMtype, "ptr") == 0) {
					CharAccumulator_appendChars(outerSource, ", align 8");
				} else {
					CharAccumulator_appendChars(outerSource, ", align 4");
				}
				
				CharAccumulator_free(&expressionSource);
				
				break;
			}
			
			case ASTnodeType_operator: {
				ASTnode_operator *data = (ASTnode_operator *)node->value;
				
				char *expectedLLVMtype = getLLVMtypeFromType(GBI->variables, GBI->level, (ASTnode *)expectedTypes->data);
				
				CharAccumulator leftInnerSource = {100, 0, 0};
				CharAccumulator_initialize(&leftInnerSource);
				
				CharAccumulator rightInnerSource = {100, 0, 0};
				CharAccumulator_initialize(&rightInnerSource);
				
				linkedList_Node *expectedTypeForLeftAndRight = NULL;
				
				// all of these operators are very similar and even use the same 'icmp' instruction
				// https://llvm.org/docs/LangRef.html#fcmp-instruction
				if (
					data->operatorType == ASTnode_operatorType_equivalent ||
					data->operatorType == ASTnode_operatorType_greaterThan ||
					data->operatorType == ASTnode_operatorType_lessThan
				) {
					buildLLVM(GBI, outerFunction, outerSource, &leftInnerSource, NULL, &expectedTypeForLeftAndRight, data->left, 0, 0);
					if (
						expectTypeWithString((ASTnode *)expectedTypeForLeftAndRight->data, "Int8") &&
						expectTypeWithString((ASTnode *)expectedTypeForLeftAndRight->data, "Int32") &&
						expectTypeWithString((ASTnode *)expectedTypeForLeftAndRight->data, "Int64")
					) {
						if (expectTypeWithString((ASTnode *)expectedTypeForLeftAndRight->data, "__Number")) {
							printf("operator expected a number\n");
							compileError(node->location);
						}
						
						linkedList_freeList(&expectedTypeForLeftAndRight);
						buildLLVM(GBI, outerFunction, outerSource, &leftInnerSource, NULL, &expectedTypeForLeftAndRight, data->right, 0, 0);
						if (
							expectTypeWithString((ASTnode *)expectedTypeForLeftAndRight->data, "Int8") &&
							expectTypeWithString((ASTnode *)expectedTypeForLeftAndRight->data, "Int32") &&
							expectTypeWithString((ASTnode *)expectedTypeForLeftAndRight->data, "Int64")
						) {
							if (expectTypeWithString((ASTnode *)expectedTypeForLeftAndRight->data, "__Number")) {
								printf("operator expected a number\n");
								compileError(node->location);
							}
							
							// default to Int32
							linkedList_freeList(&expectedTypeForLeftAndRight);
							addTypeAsString(&expectedTypeForLeftAndRight, "Int32");
						}
					}
					
					
					buildLLVM(GBI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeftAndRight, NULL, data->left, 0, 0);
					buildLLVM(GBI, outerFunction, outerSource, &rightInnerSource, expectedTypeForLeftAndRight, NULL, data->right, 0, 0);
					CharAccumulator_appendChars(outerSource, "\n\t%");
					CharAccumulator_appendUint(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, " = icmp ");
					
					if (data->operatorType == ASTnode_operatorType_equivalent) {
						// eq: yields true if the operands are equal, false otherwise. No sign interpretation is necessary or performed.
						CharAccumulator_appendChars(outerSource, "eq ");
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
					
					CharAccumulator_appendChars(outerSource, getLLVMtypeFromType(GBI->variables, GBI->level, (ASTnode *)expectedTypeForLeftAndRight->data));
				}
				
				// +
				else if (data->operatorType == ASTnode_operatorType_add) {
					// the expected type for both sides of the operator is the same type that is expected for the operator
					ASTnode *expectedTypeData = linkedList_addNode(&expectedTypeForLeftAndRight, sizeof(ASTnode) + sizeof(ASTnode_type));
					memcpy(expectedTypeData, (ASTnode *)expectedTypes->data, sizeof(ASTnode) + sizeof(ASTnode_type));
					
					buildLLVM(GBI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeftAndRight, NULL, data->left, 0, 0);
					buildLLVM(GBI, outerFunction, outerSource, &rightInnerSource, expectedTypeForLeftAndRight, NULL, data->right, 0, 0);
					CharAccumulator_appendChars(outerSource, "\n\t%");
					CharAccumulator_appendUint(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, " = add nsw ");
					CharAccumulator_appendChars(outerSource, expectedLLVMtype);
				}
				
				// -
				else if (data->operatorType == ASTnode_operatorType_subtract) {
					// the expected type for both sides of the operator is the same type that is expected for the operator
					ASTnode *expectedTypeData = linkedList_addNode(&expectedTypeForLeftAndRight, sizeof(ASTnode) + sizeof(ASTnode_type));
					memcpy(expectedTypeData, (ASTnode *)expectedTypes->data, sizeof(ASTnode) + sizeof(ASTnode_type));
					
					buildLLVM(GBI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeftAndRight, NULL, data->left, 0, 0);
					buildLLVM(GBI, outerFunction, outerSource, &rightInnerSource, expectedTypeForLeftAndRight, NULL, data->right, 0, 0);
					CharAccumulator_appendChars(outerSource, "\n\t%");
					CharAccumulator_appendUint(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, " = sub nsw ");
					CharAccumulator_appendChars(outerSource, expectedLLVMtype);
				}
				
				else {
					abort();
				}
				CharAccumulator_appendChars(outerSource, " ");
				CharAccumulator_appendChars(outerSource, leftInnerSource.data);
				CharAccumulator_appendChars(outerSource, ", ");
				CharAccumulator_appendChars(outerSource, rightInnerSource.data);
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, expectedLLVMtype);
					CharAccumulator_appendChars(innerSource, " ");
				}
				CharAccumulator_appendChars(innerSource, "%");
				CharAccumulator_appendUint(innerSource, outerFunction->registerCount);
				
				CharAccumulator_free(&leftInnerSource);
				CharAccumulator_free(&rightInnerSource);
				
				linkedList_freeList(&expectedTypeForLeftAndRight);
				
				outerFunction->registerCount++;
				
				break;
			}
			
			case ASTnodeType_true: {
				if (expectTypeWithString((ASTnode *)expectedTypes->data, "Bool")) {
					printf("unexpected Bool\n");
					compileError(node->location);
				}
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, "i1 ");
				}
				
				CharAccumulator_appendChars(innerSource, "true");
				
				break;
			}

			case ASTnodeType_false: {
				if (expectTypeWithString((ASTnode *)expectedTypes->data, "Bool")) {
					printf("unexpected Bool\n");
					compileError(node->location);
				}
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, "i1 ");
				}
				
				CharAccumulator_appendChars(innerSource, "false");
				
				break;
			}
				
			case ASTnodeType_number: {
				ASTnode_number *data = (ASTnode_number *)node->value;
				
				if (types != NULL) {
					addTypeAsString(types, "__Number");
					break;
				}
				
				if (
					expectTypeWithString((ASTnode *)expectedTypes->data, "Int8") &&
					expectTypeWithString((ASTnode *)expectedTypes->data, "Int32") &&
					expectTypeWithString((ASTnode *)expectedTypes->data, "Int64")
				) {
					printf("unexpected number\n");
					compileError(node->location);
				}
				
				char *LLVMtype = getLLVMtypeFromType(GBI->variables, GBI->level, (ASTnode *)(*currentExpectedType)->data);
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, LLVMtype);
					CharAccumulator_appendChars(innerSource, " ");
				}
				
				CharAccumulator_appendSubString(innerSource, data->string);
				
				break;
			}
				
			case ASTnodeType_string: {
				ASTnode_string *data = (ASTnode_string *)node->value;
				
				if (expectTypeWithString((ASTnode *)expectedTypes->data, "Pointer")) {
					printf("unexpected string\n");
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
							compileError((SourceLocation){node->location.line, node->location.columnStart + i + 1, node->location.columnEnd - (data->value->length - i)});
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
				
				CharAccumulator_appendChars(GBI->topLevelConstantSource, "\n@.str.");
				CharAccumulator_appendUint(GBI->topLevelConstantSource, GBI->stringCount);
				CharAccumulator_appendChars(GBI->topLevelConstantSource, " = private unnamed_addr constant [");
				CharAccumulator_appendUint(GBI->topLevelConstantSource, stringLength);
				CharAccumulator_appendChars(GBI->topLevelConstantSource, " x i8] c\"");
				CharAccumulator_appendChars(GBI->topLevelConstantSource, string.data);
				CharAccumulator_appendChars(GBI->topLevelConstantSource, "\\00\""); // the \00 is the NULL byte
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, "ptr ");
				}
				CharAccumulator_appendChars(innerSource, "@.str.");
				CharAccumulator_appendUint(innerSource, GBI->stringCount);
				
				GBI->stringCount++;
				
				CharAccumulator_free(&string);
				
				break;
			}
			
			case ASTnodeType_variable: {
				ASTnode_variable *data = (ASTnode_variable *)node->value;
				
				Variable *variableVariable = getBuilderVariable(GBI->variables, GBI->level, data->name);
				if (variableVariable == NULL) {
					printf("no variable named '");
					SubString_print(data->name);
					printf("'\n");
					compileError(node->location);
				}
				Variable_variable *variable = (Variable_variable *)variableVariable->value;
				
				if (types != NULL) {
					addTypeAsType(types, (ASTnode *)variable->type->data);
					break;
				}
				
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
					CharAccumulator_appendChars(innerSource, variable->LLVMtype);
					CharAccumulator_appendChars(innerSource, " ");
				}
				CharAccumulator_appendChars(innerSource, "%");
				CharAccumulator_appendUint(innerSource, outerFunction->registerCount);
				
				outerFunction->registerCount++;
				
				break;
			}
			
			default: {
				printf("unknown node type: %u\n", node->nodeType);
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
