#include <stdlib.h>
#include <stdio.h>

#include "types.h"
#include "globals.h"
#include "printer.h"

int printComma = 0;

void printKeyword(int type, char *name, char *documentation) {
	if (printComma) {
		putchar(',');
	}
	printComma = 1;
	printf("[%d, \"%s\", \"LLCL Keyword\\n\\n%s\"]", type, name, documentation);
}

void printBinding(ContextBinding *binding) {
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
			type = 6;
			break;
		}
		
		case ContextBindingType_function: {
			type = 2;
			break;
		}
		
		case ContextBindingType_macro: {
			type = 11;
			break;
		}
		
		case ContextBindingType_variable: {
			type = 5;
			break;
		}
		
		case ContextBindingType_compileTimeSetting: {
			return;
		}
		
		case ContextBindingType_struct: {
			type = 21;
			break;
		}
		
		case ContextBindingType_namespace: {
			type = 8;
			break;
		}
	}
	
	if (binding->type != ContextBindingType_namespace && binding->originFile != coreFilePointer) {
		CharAccumulator_appendChars(&documentation, "file = ");
		CharAccumulator_appendChars(&documentation, binding->originFile->context.path);
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
			
			printBinding(binding);
			
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
			
			printBinding(binding);
			
			current = current->next;
		}
		
		currentFile = currentFile->next;
	}
}
