#include <stdio.h>
#include <string.h>

#include "builder.h"
#include "error.h"

void addBuilderVariable(linkedList_Node **variables, char *key, char *LLVMname) {
	Variable *data = linkedList_addNode(variables, sizeof(Variable) + sizeof(Variable_type));
	
	data->key = key;
	
	data->type = VariableType_type;
	
	((Variable_type *)data->value)->LLVMname = LLVMname;
}

char *getBuilderType(linkedList_Node **variables, int level, char *key) {
	int index = level;
	while (index >= 0) {
		linkedList_Node *current = variables[index];
		
		while (current != NULL) {
			Variable *variable = ((Variable *)current->data);
			
			if (variable->type != VariableType_type) {
				abort();
			}
			
			if (strcmp(variable->key, key) != 0) {
				current = current->next;
				continue;
			}
			
			Variable_type *data = (Variable_type *)variable->value;
			
			return data->LLVMname;
		}
		
		index--;
	}
	
	return NULL;
}

char *buildLLVM(linkedList_Node **variables, int level, String *outerSource, linkedList_Node *current) {
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
				if (outerSource != NULL) {
					printf("function definitions are only allowed at top level\n");
					compileError(node->location);
				}
				ASTnode_function *data = (ASTnode_function *)node->value;
				
				String newOuterSource = {100, 0, 0};
				String_initialize(&newOuterSource);
				
				String_appendChars(&LLVMsource, "\ndefine ");
				char *LLVMtype = getBuilderType(variables, level, ((ASTnode_type *)((ASTnode *)data->returnType->data)->value)->name);
				if (LLVMtype == NULL) {
					printf("unknown type\n");
					compileError(((ASTnode *)data->returnType->data)->location);
				}
				String_appendChars(&LLVMsource, LLVMtype);
				String_appendChars(&LLVMsource, " @");
				String_appendChars(&LLVMsource, data->name);
				String_appendChars(&LLVMsource, "() {");
				
				free(buildLLVM(variables, level, &newOuterSource, data->codeBlock));
				
				String_appendChars(&LLVMsource, newOuterSource.data);
				
				String_free(&newOuterSource);
				
				String_appendChars(&LLVMsource, "\n}");
				
				break;
			}
				
			case ASTnodeType_return: {
				if (outerSource == NULL) {
					printf("return in a weird spot\n");
					compileError(node->location);
				}
				ASTnode_return *data = (ASTnode_return *)node->value;
				
				String_appendChars(outerSource, "\n\tret ");
				
				char *newSource = buildLLVM(variables, level, outerSource, data->expression);
				
				String_appendChars(outerSource, newSource);
				
				free(newSource);
				
				break;
			}
				
//			case ASTnodeType_type: {
//				break;
//			}
			
			case ASTnodeType_number: {
				ASTnode_number *data = (ASTnode_number *)node->value;
				
				String_appendChars(&LLVMsource, "i32 ");
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
