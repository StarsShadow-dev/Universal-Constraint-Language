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
	CharAccumulator_appendSubString(charAccumulator, type->scopeObject->key);
	
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
				
				if (expressionFact->operatorType == ASTnode_infixOperatorType_equivalent) {
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

int printComma = 0;

// error is 0

void printError(SourceLocation location, char *msg, char *indicator) {
	if (printComma) {
		putchar(',');
	}
	printComma = 1;
	printf("[0, %d, %d, %d, \"%s\", \"%s\"]", location.line, location.columnStart, location.columnEnd, msg, indicator);
}

// warning is 1

void printKeyword(int type, char *name, char *documentation) {
	if (printComma) {
		putchar(',');
	}
	printComma = 1;
	printf("[2, %d, \"%s\", \"LLCL Keyword\\n\\n%s\"]", type, name, documentation);
}

void printScopeObject(FileInformation *FI, ScopeObject *scopeObject) {
	int type;
	SubString *name = scopeObject->key;
	
	CharAccumulator documentation = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(&documentation);
	
	switch (scopeObject->scopeObjectType) {
		case ScopeObjectType_struct: {
			CharAccumulator_appendChars(&documentation, "```\\nstruct```\\n");
			
			type = 21;
			break;
		}
		
		case ScopeObjectType_function: {
			ScopeObject_function *data = (ScopeObject_function *)scopeObject->value;
			
			CharAccumulator_appendChars(&documentation, "```\\nfunction ");
			CharAccumulator_appendSubString(&documentation, name);
			CharAccumulator_appendChars(&documentation, "(");
			CharAccumulator_appendChars(&documentation, "): ");
			getTypeDescription(FI, &documentation, &data->returnType, 0);
			CharAccumulator_appendChars(&documentation, ";\\n```\\n\\n");
			
			type = 2;
			break;
		}
		
		case ScopeObjectType_value: {
			CharAccumulator_appendChars(&documentation, "value\\n");
			
			type = 11;
			break;
		}
	}
	
//	if (binding->originFile != coreFilePointer) {
//		CharAccumulator_appendChars(&documentation, "[origin file](");
//		CharAccumulator_appendChars(&documentation, binding->originFile->context.path);
//		CharAccumulator_appendChars(&documentation, ")");
//	}
	
	if (printComma) {
		putchar(',');
	}
	printComma = 1;
	
	printf("[2, %d, \"%.*s\", \"%s\"]", type, name->length, name->start, documentation.data);
	
	CharAccumulator_free(&documentation);
}

void printKeywords(FileInformation *FI) {
	printKeyword(13, "true", "");
	printKeyword(13, "false", "");
	
	if (FI->level == 0) {
		printKeyword(13, "import", "");
		printKeyword(13, "struct", "");
		printKeyword(13, "fn", "");
	} else {
		printKeyword(13, "while", "");
		printKeyword(13, "if", "If statement syntax:\\n```\\nif (/*condition*/) {\\n\\t// code\\n}\\n```");
		printKeyword(13, "return", "");
		printKeyword(13, "var", "");
	}
}

void printScopeObjects(FileInformation *FI) {
	int index = FI->level;
	while (index >= 0) {
		linkedList_Node *current = FI->context.scopeObjects[index];
		
		while (current != NULL) {
			ScopeObject *scopeObject = ((ScopeObject *)current->data);
			
			printScopeObject(FI, scopeObject);
			
			current = current->next;
		}
		
		index--;
	}
}
