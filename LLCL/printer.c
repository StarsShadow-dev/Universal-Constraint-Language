#include <stdlib.h>
#include <stdio.h>

#include "types.h"
#include "globals.h"
#include "printer.h"

void getASTnodeDescription(FileInformation *FI, CharAccumulator *charAccumulator, ASTnode *node) {
	switch (node->nodeType) {
		case ASTnodeType_bool: {
			ASTnode_bool *data = (ASTnode_bool *)node->value;
			
			if (data->isTrue) {
				CharAccumulator_appendChars(charAccumulator, "true");
			} else {
				CharAccumulator_appendChars(charAccumulator, "false");
			}
			return;
		}
		
		case ASTnodeType_number: {
			CharAccumulator_appendInt(charAccumulator, ((ASTnode_number *)node->value)->value);
			return;
		}
		
		case ASTnodeType_string: {
			CharAccumulator_appendChars(charAccumulator, "\"");
			CharAccumulator_appendSubString(charAccumulator, ((ASTnode_string *)node->value)->value);
			CharAccumulator_appendChars(charAccumulator, "\"");
			return;
		}
		
		default: {
			CharAccumulator_appendChars(charAccumulator, "/*can not print*/");
			return;
		}
	}
}

void getTypeDescription(FileInformation *FI, CharAccumulator *charAccumulator, BuilderType *type, int withFactInformation) {
	CharAccumulator_appendSubString(charAccumulator, type->binding->key);
	
	if (!withFactInformation) return;
	
	CharAccumulator_appendChars(charAccumulator, "\n");
	
	int index = 0;
	while (index <= FI->level) {
		linkedList_Node *currentFact = type->factStack[index];
		
		if (currentFact == NULL) {
			index++;
			continue;
		}
		
		CharAccumulator_appendChars(charAccumulator, "[");
		CharAccumulator_appendInt(charAccumulator, index);
		CharAccumulator_appendChars(charAccumulator, "]: ");
		
		while (currentFact != NULL) {
			Fact *fact = (Fact *)currentFact->data;
			
			if (fact->type == FactType_expression) {
				Fact_expression *expressionFact = (Fact_expression *)fact->value;
				
				CharAccumulator_appendChars(charAccumulator, "(");
				
				if (expressionFact->operatorType == ASTnode_operatorType_equivalent) {
					if (expressionFact->left == NULL) {
						CharAccumulator_appendChars(charAccumulator, "$");
					} else {
						abort();
					}
					CharAccumulator_appendChars(charAccumulator, " == ");
					getASTnodeDescription(FI, charAccumulator, expressionFact->rightConstant);
				} else {
					abort();
				}
				
				CharAccumulator_appendChars(charAccumulator, ")");
			} else {
				abort();
			}
			
			currentFact = currentFact->next;
		}
		
		CharAccumulator_appendChars(charAccumulator, "\n");
		
		index++;
	}
}

void getVariableDescription(FileInformation *FI, CharAccumulator *charAccumulator, ContextBinding *variableBinding) {
	if (variableBinding->type != ContextBindingType_variable) abort();
	ContextBinding_variable *variable = (ContextBinding_variable *)variableBinding->value;
	
	CharAccumulator_appendChars(charAccumulator, "variable description for '");
	CharAccumulator_appendSubString(charAccumulator, variableBinding->key);
	CharAccumulator_appendChars(charAccumulator, "'\nbyteSize = ");
	CharAccumulator_appendInt(charAccumulator, variableBinding->byteSize);
	CharAccumulator_appendChars(charAccumulator, "\nbyteAlign = ");
	CharAccumulator_appendInt(charAccumulator, variableBinding->byteAlign);
	CharAccumulator_appendChars(charAccumulator, "\n\ntype = ");
	
	getTypeDescription(FI, charAccumulator, &variable->type, 1);
}

int printComma = 0;

void printKeyword(int type, char *name, char *documentation) {
	if (printComma) {
		putchar(',');
	}
	printComma = 1;
	printf("[%d, \"%s\", \"LLCL Keyword\\n\\n%s\"]", type, name, documentation);
}

void printBinding(FileInformation *FI, ContextBinding *binding) {
	int type;
	SubString *name = binding->key;
	
	// do not print if the name starts with "__"
	if (name->length >= 2) {
		if (name->start[0] == '_' && name->start[1] == '_') {
			return;
		}
	}
	
	CharAccumulator documentation = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(&documentation);
	
	switch (binding->type) {
		case ContextBindingType_simpleType: {
			CharAccumulator_appendChars(&documentation, "primitive type");
			
			type = 6;
			break;
		}
		
		case ContextBindingType_function: {
			ContextBinding_function *data = (ContextBinding_function *)binding->value;
			
			CharAccumulator_appendChars(&documentation, "```\\nfunction ");
			CharAccumulator_appendSubString(&documentation, binding->key);
			CharAccumulator_appendChars(&documentation, "(");
			CharAccumulator_appendChars(&documentation, "): ");
			getTypeDescription(FI, &documentation, &data->returnType, 0);
			CharAccumulator_appendChars(&documentation, ";\\n```\\n\\n");
			
			type = 2;
			break;
		}
		
		case ContextBindingType_macro: {
			CharAccumulator_appendChars(&documentation, "macro");
			
			type = 11;
			break;
		}
		
		case ContextBindingType_variable: {
			ContextBinding_variable *data = (ContextBinding_variable *)binding->value;
			
			CharAccumulator_appendChars(&documentation, "```\\nvar ");
			CharAccumulator_appendSubString(&documentation, binding->key);
			CharAccumulator_appendChars(&documentation, ": ");
			getTypeDescription(FI, &documentation, &data->type, 0);
			CharAccumulator_appendChars(&documentation, ";\\n```\\n\\n");
			
			type = 5;
			break;
		}
		
		case ContextBindingType_compileTimeSetting: {
			return;
		}
		
		case ContextBindingType_struct: {
			CharAccumulator_appendChars(&documentation, "```\\nstruct ");
			CharAccumulator_appendSubString(&documentation, binding->key);
			CharAccumulator_appendChars(&documentation, " {");
			CharAccumulator_appendChars(&documentation, "}\\n```\\n\\n");
			
			type = 21;
			break;
		}
		
		case ContextBindingType_namespace: {
			CharAccumulator_appendChars(&documentation, "namespace");
			
			type = 8;
			break;
		}
	}
	
	if (binding->type != ContextBindingType_namespace && binding->originFile != coreFilePointer) {
		CharAccumulator_appendChars(&documentation, "[origin file](");
		CharAccumulator_appendChars(&documentation, binding->originFile->context.path);
		CharAccumulator_appendChars(&documentation, ")");
	}
	
	if (printComma) {
		putchar(',');
	}
	printComma = 1;
	
	printf("[%d, \"%.*s\", \"%s\"]", type, name->length, name->start, documentation.data);
	
	CharAccumulator_free(&documentation);
}

void printKeywords(FileInformation *FI) {
	if (FI->level == 0) {
		printKeyword(13, "import", "");
		printKeyword(13, "macro", "");
		printKeyword(13, "struct", "");
		printKeyword(13, "impl", "");
		printKeyword(13, "function", "");
	} else {
		printKeyword(13, "while", "");
		printKeyword(13, "if", "If statement syntax:\\n```\\nif (/*condition*/) {\\n\\t// code\\n}\\n```");
		printKeyword(13, "return", "");
		printKeyword(13, "var", "");
	}
}

void printBindings(FileInformation *FI) {
	int index = FI->level;
	while (index >= 0) {
		linkedList_Node *current = FI->context.bindings[index];
		
		while (current != NULL) {
			ContextBinding *binding = ((ContextBinding *)current->data);
			
			printBinding(FI, binding);
			
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
				printBinding(FI, binding);
			}
			
			current = current->next;
		}
		
		currentFile = currentFile->next;
	}
}
