#include <stdio.h>
#include <string.h>

#include "builder.h"
#include "error.h"

char *buildLLVM(String *outerSource, linkedList_Node *current) {
	String LLVMsource = {100, 0, 0};
	String_initialize(&LLVMsource);
	
	while (1) {
		if (current == NULL) {
			break;
		}
		
		ASTnode *node = ((ASTnode *)((current)->data));
		
		printf("node type: %u\n", node->type);
		
		switch (node->type) {
			case ASTnodeType_function: {
				if (outerSource != NULL) {
					printf("function definitions are only allowed at top level\n");
					compileError(node->location);
				}
				ASTnode_function *function = (ASTnode_function *)node->value;
				
				String_appendChars(&LLVMsource, "\ndefine i32 @");
				String_appendChars(&LLVMsource, function->name);
				String_appendChars(&LLVMsource, "() {");
				
				String newOuterSource = {100, 0, 0};
				String_initialize(&newOuterSource);
				
				buildLLVM(&newOuterSource, function->codeBlock);
				
				String_appendChars(&LLVMsource, newOuterSource.data);
				
				String_free(&newOuterSource);
				
				String_appendChars(&LLVMsource, "\n}");
				
				break;
			}
				
//			case ASTnodeType_type: {
//				break;
//			}
			
			case ASTnodeType_number: {
				ASTnode_number *number = (ASTnode_number *)node->value;
				
				String_appendChars(outerSource, "\n\tret i32 ");
				String_appendChars(outerSource, number->string);
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
